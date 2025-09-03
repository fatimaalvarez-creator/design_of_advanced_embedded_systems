# UART Implementation via GPIO on STM32H7

## 📖 Overview
A bare-metal implementation of UART communication protocol using GPIO bit-banging on STM32H7 microcontroller. This project demonstrates how to emulate UART functionality through precise timing and direct GPIO control without using dedicated hardware peripherals.

## ⚡ Features
- **Protocol**: UART 8N1 (8 data bits, no parity, 1 stop bit)
- **Baud Rate**: 9600
- **Hardware**: GPIOA Pin 5 (Software-controlled TX)
- **Timing Method**: DWT cycle counter for precise timing
- **Development**: Direct register access (no HAL libraries)

## 🛠️ Hardware Setup
- **Microcontroller**: STM32H7
- **Transmit Pin**: PA5 (GPIO-controlled)
- **Measurement Tool**: Digital oscilloscope for signal verification

## 📁 Code Structure

### Configuration
```c
#define TX_PORT    GPIOA
#define TX_PORT_ENR   RCC_AHB4ENR_GPIOAEN
#define TX_PIN    5u    // PA5
#define BAUD_RATE    9600u
#define BIT_CYCLES  (SystemCoreClock / BAUD_RATE)
```

### GPIO Control Helpers
```c
static inline void tx_high(void) { TX_PORT->BSRR = (1u << TX_PIN); }
static inline void tx_low(void) { TX_PORT->BSRR = (1u << (TX_PIN + 16u)); }
```

### Precision Timing with DWT
```c
static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55;    // Unlock
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void wait_until(uint32_t deadline_cycles) {
    while ((int32_t)(DWT->CYCCNT - deadline_cycles) < 0) {
        __NOP();
    }
}
```

### GPIO Initialization
```c
static void GPIO_Init_TX(void) {
    RCC->AHB4ENR |= TX_PORT_ENR;
    TX_PORT->MODER &= ~(3u << (TX_PIN * 2)); 
    TX_PORT->MODER |= (1u << (TX_PIN * 2)); 
    TX_PORT->OTYPER &= ~(1u << TX_PIN); 
    TX_PORT->OSPEEDR |= (3u << (TX_PIN * 2)); 
    TX_PORT->PUPDR &= ~(3u << (TX_PIN * 2));
    tx_high(); // Idle state = high
}
```

### UART Transmission Function
```c
static void UART_SendByte(uint8_t b) {
    __disable_irq(); // Reduce timing jitter
    
    // Start bit (0)
    tx_low();
    uint32_t deadline = DWT->CYCCNT + BIT_CYCLES;
    wait_until(deadline);

    // 8 data bits (LSB first)
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
```

## 📊 Results & Verification
Oscilloscope analysis confirmed:
- **Timing Accuracy**: 104.16 µs per bit (1/9600 ≈ 104.17 µs) with <0.01% error
- **Signal Quality**: Clean square wave with fast transitions
- **Frame Structure**: Correct UART frame for character 'H' (0x48):
  - Start bit (low)
  - Data bits: 01001000 (LSB first: 0-0-0-1-0-0-1-0)
  - Stop bit (high)

## 🎥 Demonstration Video
[View Demonstration Video](https://drive.google.com/drive/folders/1vNbGcepM1lL0qdgJ6yXr3bUrbuIz-3pi?usp=sharing)

The video shows:
- Hardware setup with STM32H7 board
- Oscilloscope connections and measurements
- Real-time UART signal transmission
- Detailed waveform analysis matching UART 8N1 protocol

## ✅ Conclusion
The GPIO-based UART implementation proved effective and precise using DWT cycle counting for timing. This approach is valuable when:
- Hardware UART peripherals are unavailable
- Multiple UART channels are needed
- Protocol flexibility is required

**Limitations**:
- Higher CPU usage compared to hardware solutions
- Sensitive to system timing variations
- Not recommended for high-speed or critical applications

## 👩‍💻 Author
**Fatima Álvarez Nuño** - A01645815  
**Course**: Design of Advanced Embedded Systems (Group 601)  
**Professor**: José Ignacio Parra Vilchis  
**Institution**: Campus Guadalajara  
**Date**: August 2025

## 📝 License
Educational project for academic purposes.