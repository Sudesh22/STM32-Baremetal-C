#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stm32f1xx.h>

#define GPS_BUFFER_SIZE 256  // Define buffer size for GPS data

// UART initialization functions
void uart1_init(void);
void uart2_init(void);

// UART transmission functions
void uart1_tran_byte(uint8_t data);
void uart1_tran_string(const char *str);

// Circular buffer functions for UART2
void uart2_receive_handler(void);  // ISR handler for UART2
uint8_t uart2_read_char(uint8_t *data);

#endif
