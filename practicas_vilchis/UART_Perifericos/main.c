#include "stm32h7xx.h"
#include <stdint.h>

/* ====================== Parámetros ====================== */
#define UART_BAUDRATE     9600u          // Baud rate deseado
#define USART3_CLK_HZ     64000000u     // Se esta usando el reloj HSI que es de 64 MHz

/* ====================== Utilidad BRR ==================== */
// Parte del codigo encargada de que mi reloj tenga el valor correcto para el registro BRR
// BRR = Tiempo exacto de cada bit
static inline uint32_t brr_from(uint32_t fclk, uint32_t baud) {
    return (fclk + (baud/2u)) / baud;
}

/* ====================== DWT para delay ================== */
/// Se utiliza el DWT como un contador de ticks,
// lo que hace esta parte del codigo es que calcula por ticks, cuanto dura un segundo y asi volver a mandar el caracter
static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // habilita el bloque de trazas (necesario para DWT)
    DWT->LAR = 0xC5ACCE55;                           // desbloqueo (algunos H7 lo requieren)
    DWT->CYCCNT = 0;                                 // limpia el contador de ciclos
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;            // enciende el contador de ciclos
}

static void delay_ms(uint32_t ms) {
    uint32_t start = DWT->CYCCNT;
    uint64_t ticks = ((uint64_t)SystemCoreClock * ms) / 1000u;
    while ((uint32_t)(DWT->CYCCNT - start) < ticks) { /* espera ocupada */ }
}

/* ================== GPIO: PB10/PB11 como AF7 = USART3 ================== */
// Esta parte del codigo se encarga de poner los GPIO en el camino del USART3
static void GPIO_Init_USART3_PB10_PB11(void) {
    // 1) Habilita el reloj del puerto B
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;

    // 2) Modo Alternate Function en PB10 y PB11 (MODER: 10b)
    GPIOB->MODER &= ~((3u<<(10*2)) | (3u<<(11*2)));
    GPIOB->MODER |=  ((2u<<(10*2)) | (2u<<(11*2)));

    // El modo AF  lo que hace es conectarlo a un periferico interno

    // 3) Selecciona AF7 (USART3) en AFR[1] (pines 8..15)
    GPIOB->AFR[1] &= ~((0xFu<<((10-8)*4)) | (0xFu<<((11-8)*4)));
    GPIOB->AFR[1] |=  ((7u  <<((10-8)*4)) | (7u  <<((11-8)*4)));

    // 4) Push-pull y velocidad muy alta (mejores flancos en TX)
    GPIOB->OTYPER  &= ~((1u<<10) | (1u<<11));
    GPIOB->OSPEEDR |=  (3u<<(10*2)) | (3u<<(11*2));   // 11b = very high

    // 5) Pull-up en RX (PB11) para mantener línea en 'idle' alto
    GPIOB->PUPDR   &= ~(3u<<(11*2));
    GPIOB->PUPDR   |=  (1u<<(11*2));                  // 01b = pull-up
}

/* ================== USART3: 9600 8N1 por registros ================== */
/* CR1/CR2/CR3 en modo básico: 8 bits, sin paridad, 1 stop, oversampling x16. */
static void USART3_Init_9600_8N1(void) {
    // 1) Habilita el reloj del USART3
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;

    // 2) Reset corto para arrancar “limpio”
    RCC->APB1LRSTR |=  RCC_APB1LRSTR_USART3RST;
    RCC->APB1LRSTR &= ~RCC_APB1LRSTR_USART3RST;

    // 3) Asegúrate de que esté deshabilitado antes de configurar
    USART3->CR1 &= ~USART_CR1_UE;

    // 4) Config básica: 8N1, sin paridad, sin flow/DMA
    USART3->CR1 = 0x00000000u;   // M1/M0=0 (8 bits), PCE=0, OVER8=0 (x16)
    USART3->CR2 = 0x00000000u;   // STOP=00 (1 stop)
    USART3->CR3 = 0x00000000u;   // sin RTS/CTS, sin DMA

    // 5) Baud rate
    USART3->BRR = brr_from(USART3_CLK_HZ, UART_BAUDRATE);

    // 6) Habilita solo el transmisor (TE) y luego el periférico (UE)
    USART3->CR1 |= USART_CR1_TE;
    USART3->CR1 |= USART_CR1_UE;

    // 7) Espera confirmación de transmisión habilitada
    while ((USART3->ISR & USART_ISR_TEACK) == 0u) { /* wait */ }
}

/* ================== Envío de un byte (polling) ================== */
/* Espera a que el TDR/FIFO tenga espacio y escribe el byte en TDR. */
static void USART3_SendByte(uint8_t b) {
    while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0u) { /* wait */ }
    USART3->TDR = b;
}

/* ============================== main ============================== */
int main(void) {
    // 0) Asegura que SystemCoreClock esté correcto (para DWT)
    SystemCoreClockUpdate();

    // 1) Inicializa el contador de ciclos (para delay_ms)
    DWT_Init();

    // 2) Configura PB10/PB11 como AF7 (USART3)
    GPIO_Init_USART3_PB10_PB11();

    // 3) Inicializa USART3 a 9600-8N1
    USART3_Init_9600_8N1();

    // 4) Bucle: enviar 'U' (0x55) una vez por segundo
    for (;;) {
        USART3_SendByte('H');   // patrón 01010101 (LSB primero), ideal para medir
        delay_ms(1000);         // espera ~1 s entre envíos
    }
}
