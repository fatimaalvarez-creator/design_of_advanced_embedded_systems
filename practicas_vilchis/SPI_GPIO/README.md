# SPI Implementation via GPIO on STM32H7

## 📖 Overview

A bare-metal implementation of the **SPI communication protocol** using GPIO bit-banging on the STM32H7 microcontroller.
This project demonstrates how to emulate SPI functionality through manual GPIO toggling and precise timing using the **DWT cycle counter**, without relying on dedicated SPI hardware peripherals.

## ⚡ Features

* **Protocol**: SPI Mode 0 (CPOL=0, CPHA=0)
* **Frame Format**: 8-bit data, MSB-first
* **Hardware**: GPIOA pins (PA5 = SCK, PA8 = MOSI, PA6 = MISO, PA4 = CS)
* **Timing Method**: DWT cycle counter for precise clock generation
* **Development**: Direct register access (no HAL libraries, no CMSIS drivers)

## 🛠️ Hardware Setup

* **Microcontroller**: STM32H7
* **SPI Signals**:

  * **SCK (Clock)** → PA5
  * **MOSI (Master Out Slave In)** → PA8
  * **MISO (Master In Slave Out)** → PA6
  * **CS (Chip Select, active low)** → PA4
* **Test Tool**: Logic analyzer or oscilloscope to verify signals

## 📁 Code Structure

### GPIO Pin Mapping

```c
#define SPI_SCK   5U   // PA5
#define SPI_MOSI  8U   // PA8
#define SPI_MISO  6U   // PA6
#define SPI_CS    4U   // PA4
```

### GPIO Control Helpers

```c
static inline void sck_high(void) { GPIO_BSRR(SPI_PORT_BASE) = (1UL << SPI_SCK); }
static inline void sck_low(void)  { GPIO_BSRR(SPI_PORT_BASE) = (1UL << (SPI_SCK + 16U)); }
static inline void mosi_high(void){ GPIO_BSRR(SPI_PORT_BASE) = (1UL << SPI_MOSI); }
static inline void mosi_low(void) { GPIO_BSRR(SPI_PORT_BASE) = (1UL << (SPI_MOSI + 16U)); }
static inline void cs_high(void)  { GPIO_BSRR(SPI_PORT_BASE) = (1UL << SPI_CS); }
static inline void cs_low(void)   { GPIO_BSRR(SPI_PORT_BASE) = (1UL << (SPI_CS + 16U)); }
```

### Precision Timing with DWT

```c
static void DWT_Init(void) {
    REG32(COREDEBUG_DEMCR) |= DEMCR_TRCENA; // Enable DWT
    DWT_LAR = 0xC5ACCE55UL;                 // Unlock DWT
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;         // Enable cycle counter
}

static void delay_us(uint32_t us) {
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = (CPU_HZ / 1000000UL) * us;
    while ((DWT_CYCCNT - start) < cycles);
}
```

### GPIO Initialization

```c
static void GPIO_Init_SPI(void) {
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOAEN;

    // Configure SCK, MOSI, CS as outputs, MISO as input
    uint32_t moder = GPIO_MODER(SPI_PORT_BASE);
    moder &= ~((3UL << (SPI_SCK*2)) | (3UL << (SPI_MOSI*2)) | (3UL << (SPI_CS*2)) | (3UL << (SPI_MISO*2)));
    moder |= (1UL << (SPI_SCK*2)) | (1UL << (SPI_MOSI*2)) | (1UL << (SPI_CS*2));
    GPIO_MODER(SPI_PORT_BASE) = moder;

    // Push-pull, high speed, pull-up on MISO
    GPIO_OTYPER(SPI_PORT_BASE) &= ~((1UL << SPI_SCK) | (1UL << SPI_MOSI) | (1UL << SPI_CS));
    GPIO_OSPEEDR(SPI_PORT_BASE) |= (3UL << (SPI_SCK*2)) | (3UL << (SPI_MOSI*2)) | (3UL << (SPI_CS*2));
    GPIO_PUPDR(SPI_PORT_BASE) |= (1UL << (SPI_MISO*2));

    // Idle states (Mode 0)
    sck_low();
    mosi_low();
    cs_high();
}
```

### SPI Transfer Function

```c
static uint8_t SPI_Transfer(uint8_t data) {
    uint8_t rx = 0;

    cs_low(); // Select slave
    for (int i = 7; i >= 0; --i) {
        if (data & (1U << i)) mosi_high(); else mosi_low();

        sck_high();        // Rising edge: sample MISO
        delay_us(10);

        rx <<= 1;
        if (GPIO_IDR(SPI_PORT_BASE) & (1UL << SPI_MISO)) rx |= 1U;

        sck_low();         // Falling edge
        delay_us(10);
    }
    cs_high(); // Deselect slave
    return rx;
}
```

### Main Loop

```c
int main(void) {
    DWT_Init();
    GPIO_Init_SPI();

    for (;;) {
        (void)SPI_Transfer('H'); // Send ASCII 'H'
        delay_ms(1000);          // 1-second interval
    }
}
```

## 📊 Results & Verification
Using a logic analyzer, the following were observed:

* **Clock Frequency**: \~50 kHz (set by 10 µs high + 10 µs low delay)
* **Frame Structure**: Correct 8-bit transmission of `'H'` (`0x48 = 01001000`) MSB-first
* **Signal Integrity**: Stable transitions on SCK, MOSI, and CS lines
* **MISO Sampling**: Captured on SCK rising edge (SPI Mode 0 behavior)

## 🎥 Demonstration Video
[View Demonstration Video](https://drive.google.com/drive/folders/1flIwbmp-BuFbyIQAPML2CrOwXLVyA54j)

## ✅ Conclusion
The GPIO-based SPI implementation worked as intended using DWT cycle counting for clock timing.
This method is useful when:

* Hardware SPI peripherals are unavailable or already in use
* Learning protocol-level details of SPI
* Custom clocking/timing flexibility is required

**Limitations**:
* Higher CPU usage compared to hardware SPI
* Limited maximum speed due to software delays
* Not recommended for high-speed or production-grade applications

## 👩‍💻 Author

**Fatima Álvarez Nuño** - A01645815
**Course**: Design of Advanced Embedded Systems (Group 601)
**Professor**: José Ignacio Parra Vilchis
**Institution**: Campus Guadalajara
**Date**: September 2025

## 📝 License
Educational project for academic purposes.

