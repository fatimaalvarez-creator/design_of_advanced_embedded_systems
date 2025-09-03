
#include "stm32h7xx.h"

/* ===== Configuración ===== */
#define TX_PORT       GPIOA
#define TX_PORT_ENR   RCC_AHB4ENR_GPIOAEN
#define TX_PIN        5u            // PA5  <-- cámbialo si quieres otro pin
#define BAUD_RATE     9600u

/* Ciclos de CPU por bit (DWT corre a SystemCoreClock) */
#define BIT_CYCLES    (SystemCoreClock / BAUD_RATE)

/* --- Helpers GPIO: escribir rápido con BSRR --- */
static inline void tx_high(void) { TX_PORT->BSRR = (1u << TX_PIN); }
static inline void tx_low (void) { TX_PORT->BSRR = (1u << (TX_PIN + 16u)); }

/* ===== DWT delay/tiempos ===== */
static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // habilita DWT
    DWT->LAR = 0xC5ACCE55;                           // desbloqueo (algunos H7 lo requieren)
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // enciende el contador
}

static inline void wait_until(uint32_t deadline_cycles) {
    // espera hasta que CYCCNT alcance 'deadline' (maneja overflow con aritmética signada)
    while ((int32_t)(DWT->CYCCNT - deadline_cycles) < 0) { __NOP(); }
}

static void delay_ms(uint32_t ms) {
    uint32_t start = DWT->CYCCNT;
    uint64_t cycles = (uint64_t)SystemCoreClock * ms / 1000u;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) { /* busy wait */ }
}

/* ===== GPIO init puro ===== */
static void GPIO_Init_TX(void) {
    // Habilita clock del puerto
    RCC->AHB4ENR |= TX_PORT_ENR;

    // PA5 como salida push-pull, very high speed, sin pull
    TX_PORT->MODER   &= ~(3u << (TX_PIN * 2));      // limpia
    TX_PORT->MODER   |=  (1u << (TX_PIN * 2));      // 01: output
    TX_PORT->OTYPER  &= ~(1u << TX_PIN);            // push-pull
    TX_PORT->OSPEEDR |=  (3u << (TX_PIN * 2));      // very high
    TX_PORT->PUPDR   &= ~(3u << (TX_PIN * 2));      // no pull

    // Línea en reposo = alto
    tx_high();
}

/* ===== UART bit-bang: 8N1 LSB-first ===== */
static void UART_SendByte(uint8_t b) {
    __disable_irq();                 // reduce jitter durante la trama

    // Start bit (0)
    tx_low();
    uint32_t deadline = DWT->CYCCNT + BIT_CYCLES;
    wait_until(deadline);

    // 8 datos LSB primero
    for (int i = 0; i < 8; i++) {
        if (b & (1u << i)) tx_high(); else tx_low();
        deadline += BIT_CYCLES;
        wait_until(deadline);
    }

    // Stop bit (1)
    tx_high();
    deadline += BIT_CYCLES;
    wait_until(deadline);

    __enable_irq();
}

/* ===== main ===== */
int main(void) {
    // Asegura que SystemCoreClock esté correcto (lo fija SystemInit y esto lo actualiza)
    SystemCoreClockUpdate();

    DWT_Init();
    GPIO_Init_TX();

    while (1) {
        UART_SendByte('H');          // envía una 'H' cada segundo
        delay_ms(1000);
    }
}
