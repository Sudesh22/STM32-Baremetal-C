#include "uart.h"
#include "string.h"

volatile char gps_buffer[GPS_BUFFER_SIZE];  // Circular buffer for GPS data
volatile uint16_t gps_head = 0;             // Write index
volatile uint16_t gps_tail = 0;             // Read index

// UART1 Initialization (PA9: TX, PA10: RX)
void uart1_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;  // Enable USART1 clock
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;   // Enable GPIOA clock

    // Configure PA9 (TX) as Alternate Function Push-Pull
    GPIOA->CRH |= GPIO_CRH_MODE9;         // Output mode, max speed 50 MHz
    GPIOA->CRH |= GPIO_CRH_CNF9_1;        // Alternate function push-pull
    GPIOA->CRH &= ~GPIO_CRH_CNF9_0;

    // Configure PA10 (RX) as Input Floating
    GPIOA->CRH &= ~GPIO_CRH_MODE10;       // Input mode
    GPIOA->CRH |= GPIO_CRH_CNF10_0;       // Input floating
    GPIOA->CRH &= ~GPIO_CRH_CNF10_1;

    USART1->BRR = 0xd05;                 // Baud rate 9600 (72 MHz clock)
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;  // Enable TX and USART
}

void uart1_tran_byte(uint8_t data) {
    while (!(USART1->SR & USART_SR_TXE));  // Wait until TXE (Transmit Data Register Empty)
    USART1->DR = data;                     // Transmit the byte
//    static counter
}

void uart1_tran_string(const char *str) {
    while (*str) {
        uart1_tran_byte((uint8_t)*str++);
    }
}

// UART2 Initialization (PA2: TX, PA3: RX)
void uart2_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;  // Enable USART2 clock
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;   // Enable GPIOA clock

    // Configure PA2 (TX) as Alternate Function Push-Pull
    GPIOA->CRL |= GPIO_CRL_MODE2;         // Output mode, max speed 50 MHz
    GPIOA->CRL |= GPIO_CRL_CNF2_1;        // Alternate function push-pull
    GPIOA->CRL &= ~GPIO_CRL_CNF2_0;

    // Configure PA3 (RX) as Input Floating
    GPIOA->CRL &= ~GPIO_CRL_MODE3;        // Input mode
    GPIOA->CRL |= GPIO_CRL_CNF3_0;        // Input floating
    GPIOA->CRL &= ~GPIO_CRL_CNF3_1;

    USART2->BRR = 0xd05;                 // Baud rate 9600 (72 MHz clock)
    USART2->CR1 = USART_CR1_RE | USART_CR1_UE | USART_CR1_RXNEIE;  // Enable RX, USART, and RXNE interrupt

    NVIC_SetPriority(USART2_IRQn, 0);     // Set high priority for USART2 interrupt
    NVIC_EnableIRQ(USART2_IRQn);          // Enable USART2 interrupt in NVIC
}

// UART2 Interrupt Handler
void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {  // Check if data is received
        uint8_t received_char = USART2->DR;  // Read received character

        // Store character in the circular buffer
        uint16_t next_head = (gps_head + 1) % GPS_BUFFER_SIZE;
        if (next_head != gps_tail) {  // Check for buffer overflow
            gps_buffer[gps_head] = received_char;
            gps_head = next_head;
        }
    }
}

// Read a character from the circular buffer (non-blocking)
uint8_t uart2_read_char(uint8_t *data) {
    if (gps_head == gps_tail) {
        return 0;  // Buffer is empty
    }

    *data = gps_buffer[gps_tail];
    gps_tail = (gps_tail + 1) % GPS_BUFFER_SIZE;
    return 1;  // Data successfully read
}

#define MAX_CHARS 5 // The number of characters to track

int func(char input) {
    static char buffer[MAX_CHARS + 1] = {0}; // Holds the last 5 characters, plus a null terminator
    static int count = 0; // Keeps track of how many characters have been entered

    // Add the new character to the buffer and shift the older ones if needed
    if (count < MAX_CHARS) {
        buffer[count++] = input;
    } else {
        memmove(buffer, buffer + 1, MAX_CHARS - 1); // Shift characters to the left
        buffer[MAX_CHARS - 1] = input;
    }

    // If we haven't collected 5 characters yet, do nothing
    if (count < MAX_CHARS) {
        return 0; // Indicating not ready
    }

    // Check the resulting string
    if (strcmp(buffer, "GPRMC") == 0) {
        return 1;
    } else if (strcmp(buffer, "GPVTG") == 0) {
        return 2;
    }
    // Add more cases as needed...

    return -1; // Default case for no match
}
