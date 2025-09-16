// SPI bit-bang STM32H7 (GPIOA: PA5 SCK, PA8 MOSI, PA6 MISO, PA4 CS)
// Bare-metal: without "stm32h7xx.h" — only stdint and MMIO.
#include <stdint.h>

/* ================== MMIO Utility ================== */
#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ================== Base addresses ================== */
/* RCC (Reset & Clock Control) */
#define RCC_BASE        0x58024400UL
/* GPIO */
#define GPIOA_BASE      0x58020000UL
/* CoreDebug / DWT (for DWT->CYCCNT) */
#define COREDEBUG_DEMCR 0xE000EDFCUL     // Debug Exception and Monitor Control Register
#define DWT_BASE        0xE0001000UL
#define DWT_CTRL        REG32(DWT_BASE + 0x000UL)
#define DWT_CYCCNT      REG32(DWT_BASE + 0x004UL)
#define DWT_LAR         REG32(DWT_BASE + 0xFB0UL)  // Lock Access Register

/* ================== Register offsets ================== */
/* RCC */
#define RCC_AHB4ENR     REG32(RCC_BASE + 0xE0UL)
/* GPIO (valid for any GPIOx) */
#define GPIO_MODER(p)   REG32((p) + 0x00UL)
#define GPIO_OTYPER(p)  REG32((p) + 0x04UL)
#define GPIO_OSPEEDR(p) REG32((p) + 0x08UL)
#define GPIO_PUPDR(p)   REG32((p) + 0x0CUL)
#define GPIO_IDR(p)     REG32((p) + 0x10UL)
#define GPIO_ODR(p)     REG32((p) + 0x14UL)
#define GPIO_BSRR(p)    REG32((p) + 0x18UL)

/* ================== Useful bits ================== */
#define DEMCR_TRCENA    (1UL << 24)          // Enable trace block (needed for DWT)
#define DWT_CTRL_CYCCNTENA (1UL << 0)

#define RCC_AHB4ENR_GPIOAEN (1UL << 0)       // Clock enable for GPIOA

/* ================== Pin mapping ================== */
#define SPI_PORT_BASE   GPIOA_BASE
#define SPI_SCK         5U   // PA5
#define SPI_MOSI        8U   // PA8
#define SPI_MISO        6U   // PA6
#define SPI_CS          4U   // PA4

/* ================== CPU frequency for delays ================== */
#ifndef CPU_HZ
#define CPU_HZ          64000000UL           // Adjust to your real SYSCLK (e.g. 400000000UL)
#endif

/* ================== BSRR helpers (atomic set/reset) ================== */
static inline void sck_high(void) { GPIO_BSRR(SPI_PORT_BASE) = (1UL << SPI_SCK); }
static inline void sck_low(void)  { GPIO_BSRR(SPI_PORT_BASE) = (1UL << (SPI_SCK + 16U)); }
static inline void mosi_high(void){ GPIO_BSRR(SPI_PORT_BASE) = (1UL << SPI_MOSI); }
static inline void mosi_low(void) { GPIO_BSRR(SPI_PORT_BASE) = (1UL << (SPI_MOSI + 16U)); }
static inline void cs_high(void)  { GPIO_BSRR(SPI_PORT_BASE) = (1UL << SPI_CS); }
static inline void cs_low(void)   { GPIO_BSRR(SPI_PORT_BASE) = (1UL << (SPI_CS + 16U)); }

/* ================== DWT for precise delays ================== */
static void DWT_Init(void) {
    REG32(COREDEBUG_DEMCR) |= DEMCR_TRCENA;        // enable DWT
    DWT_LAR = 0xC5ACCE55UL;                        // unlock DWT (needed on some cores)
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;                // start cycle counter
}

/* ================== Cycle-based delays ================== */
static void delay_us(uint32_t us) {
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = (uint32_t)((CPU_HZ / 1000000UL) * us);
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) { /* busy wait */ }
}

static void delay_ms(uint32_t ms) {
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = (uint32_t)((CPU_HZ / 1000UL) * ms);
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) { /* busy wait */ }
}

/* ================== GPIO Init for SPI bit-bang ================== */
static void GPIO_Init_SPI(void) {
    /* 1) Enable GPIOA clock */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOAEN;

    /* 2) MODER: SCK/MOSI/CS as outputs (01), MISO as input (00) */
    uint32_t moder = GPIO_MODER(SPI_PORT_BASE);
    moder &= ~((3UL << (SPI_SCK*2)) | (3UL << (SPI_MOSI*2)) | (3UL << (SPI_CS*2)) | (3UL << (SPI_MISO*2)));
    moder |=  (1UL << (SPI_SCK*2)) | (1UL << (SPI_MOSI*2)) | (1UL << (SPI_CS*2));
    GPIO_MODER(SPI_PORT_BASE) = moder;

    /* 3) OTYPER: push-pull for outputs */
    uint32_t otyper = GPIO_OTYPER(SPI_PORT_BASE);
    otyper &= ~((1UL << SPI_SCK) | (1UL << SPI_MOSI) | (1UL << SPI_CS));
    GPIO_OTYPER(SPI_PORT_BASE) = otyper;

    /* 4) OSPEEDR: high speed (11) for SCK/MOSI/CS */
    uint32_t ospeed = GPIO_OSPEEDR(SPI_PORT_BASE);
    ospeed |= (3UL << (SPI_SCK*2)) | (3UL << (SPI_MOSI*2)) | (3UL << (SPI_CS*2));
    GPIO_OSPEEDR(SPI_PORT_BASE) = ospeed;

    /* 5) PUPDR: no pull for outputs; pull-up on MISO if slave floats */
    uint32_t pupdr = GPIO_PUPDR(SPI_PORT_BASE);
    pupdr &= ~((3UL << (SPI_SCK*2)) | (3UL << (SPI_MOSI*2)) | (3UL << (SPI_CS*2)) | (3UL << (SPI_MISO*2)));
    pupdr |=  (1UL << (SPI_MISO*2));   // 01 = Pull-up on MISO (adjust if not required)
    GPIO_PUPDR(SPI_PORT_BASE) = pupdr;

    /* 6) Initial states (Mode 0: CPOL=0, CPHA=0) */
    sck_low();
    mosi_low();
    cs_high();      // deselect slave
}

/* ================== SPI bit-bang (Mode 0, MSB first) ================== */
static uint8_t SPI_Transfer(uint8_t data) {
    uint8_t rx = 0;

    cs_low();   // select slave

    for (int i = 7; i >= 0; --i) {
        // Set MOSI
        if (data & (1U << i)) mosi_high(); else mosi_low();

        // Rising edge: data valid, sample MISO (Mode 0)
        sck_high();
        delay_us(10);

        rx <<= 1;
        if (GPIO_IDR(SPI_PORT_BASE) & (1UL << SPI_MISO)) rx |= 1U;

        // Falling edge
        sck_low();
        delay_us(10);
    }

    cs_high();  // deselect slave
    return rx;
}

/* ================== main ================== */
int main(void) {
    DWT_Init();
    GPIO_Init_SPI();

    for (;;) {
        (void)SPI_Transfer('H');
        delay_ms(1000);
    }
}
