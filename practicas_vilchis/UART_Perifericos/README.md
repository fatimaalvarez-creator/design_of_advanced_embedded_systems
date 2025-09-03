# UART Implementation on STM32H745

## 📖 Overview
This project demonstrates a bare-metal implementation of the UART (Universal Asynchronous Receiver/Transmitter) protocol on an STM32H745 microcontroller using direct register access. The implementation includes GPIO configuration, USART peripheral setup, baud rate calculation, and data transmission, verified using an oscilloscope.

## ⚡ Features
- **Baud Rate**: 9600
- **Data Format**: 8 data bits, no parity, 1 stop bit (8N1)
- **Hardware Pins**: PB10 (TX) and PB11 (RX) with alternate function AF7 (USART3)
- **Development Approach**: Direct register manipulation (no HAL libraries)

## 🛠️ Implementation Details

### GPIO Configuration
The function `GPIO_Init_USART3_PB10_PB11()` configures the GPIO pins:
- Enables clock for GPIOB
- Sets PB10 and PB11 to alternate function mode
- Selects AF7 (USART3) for both pins
- Configures high output speed and pull-up on RX pin

### USART Initialization
The function `USART3_Init_9600_8N1()`:
- Enables the USART3 clock
- Performs a peripheral reset
- Disables USART before configuration
- Sets 8N1 format (no parity, 1 stop bit)
- Calculates and sets the baud rate register value
- Enables transmitter and USART
- Waits for transmission enable acknowledgment

### Baud Rate Calculation
The inline function `brr_from()` calculates the BRR register value using rounding for better accuracy:
```c
return (fclk + (baud/2u)) / baud;
```

### Data Transmission
The function `USART3_SendByte()`:
- Waits for the transmit data register to be empty
- Writes the byte to the transmit data register

## 📊 Results & Verification
The implementation was verified using an oscilloscope:
- **Idle State**: Line remains high (logic level 1)
- **Character 'H' (0x48) Transmission**:
  - Start bit (low level)
  - Data bits: 01001000 (LSB first: 0-0-0-1-0-0-1-0)
  - Stop bit (high level)
- Clean transitions between logic levels
- Well-defined square waveform
- Accurate timing confirming correct baud rate

## 🎥 Demonstration Video
A video demonstrating the hardware setup, oscilloscope measurements, and waveform analysis is available:
[View Demonstration Video](https://drive.google.com/drive/folders/1JIuHIXCeUXGwLCJnHyIXJzaIKoOi6JnH?usp=sharing)

The video shows:
- Hardware configuration
- Oscilloscope signal during transmission of character 'H'
- Timing measurements confirming correct baud rate
- Waveform analysis matching the expected UART frame

## ✅ Conclusion
The UART implementation using direct register access was successful, demonstrating precise and functional asynchronous serial communication. This project provides a solid foundation for more complex developments requiring serial communication capabilities.

## 👨‍💻 Author
**Fatima Álvarez Nuño** - A01645815  
**Course**: Design of Advanced Embedded Systems (Group 601)  
**Professor**: José Ignacio Parra Vilchis  
**Institution**: Campus Guadalajara  
**Date**: 02 September 2025

## 📝 License
This project is intended for educational purposes as part of academic coursework.