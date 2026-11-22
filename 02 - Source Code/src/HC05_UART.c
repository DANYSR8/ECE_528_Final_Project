/**
 * @file HC05_UART.c
 *
 * @brief Source code for the HC05_UART driver.
 *
 * This file contains the function definitions for the HC05_UART driver.
 *
 * It interfaces with the HC-05 Bluetooth module (6-pin base board), which uses the UART communication protocol.
 *  - Product Link: https://deshide.com/product-details_HC-05.html
 *
 * COM ports on my computer: Outgoing = COM6 / Incoming = COM7
 *
 * The following connections must be made (front side with button facing you, pins numbered left to right):
 *  - HC-05 STATE  (Pin 1)    <-->  Not Connected
 *  - HC-05 RXD    (Pin 2)    <-->  MSP432 LaunchPad Pin P3.3 (UCA2TXD)
 *  - HC-05 TXD    (Pin 3)    <-->  MSP432 LaunchPad Pin P3.2 (UCA2RXD)
 *  - HC-05 GND    (Pin 4)    <-->  MSP432 LaunchPad GND
 *  - HC-05 VCC    (Pin 5)    <-->  MSP432 LaunchPad 5V
 *  - HC-05 EN     (Pin 6)    <-->  Not Connected
 *
 * @note For more information regarding the Enhanced Universal Serial Communication Interface (eUSCI),
 *       refer to the MSP432P4xx Microcontrollers Technical Reference Manual.
 *
 * @author
 *
 */

#include <inc/HC05_UART.h>
#include <stdint.h>
#include <stdio.h>


// ============================== RX RING BUFFER ==============================

#define HC05_RX_SIZE 64                                     // Ring buffer size in bytes

static volatile uint8_t ring_buffer[HC05_RX_SIZE];          // Received bytes waiting to be read
static volatile uint8_t ring_buff_head = 0;                 // Next location written by the ISR
static volatile uint8_t ring_buff_tail = 0;                 // Next location read by InChar


// ============================== INITIALIZATION ==============================

/*
 * Initializes eUSCI_A2 in UART mode to talk to the HC-05 Bluetooth module.
 *
 * Sets P3.2 and P3.3 to their primary UART function.
 * Configures the UART for 8 data bits, no parity, 1 stop bit, LSB first, clocked from SMCLK at 9600 baud.
 * Enables the RX interrupt so received bytes are stored in the ring buffer.
 *
 * -- GENERAL PIN SETUP (SEL) --
 *    PM_UCA2: set pins to primary function (SEL1 = 0 / SEL0 = 1)
 *    Bit mask for P3.3 and P3.2 --> 0x0C (0000_1100b)
 *
 * -- UART STRUCTURE SETUP (CTLW0) --
 *    No Parity          -->  Bit 15     = 0b
 *    LSB First          -->  Bit 13     = 0b
 *    8-Bit Data         -->  Bit 12     = 0b
 *    1 Stop Bit         -->  Bit 11     = 0b
 *    UART Mode          -->  Bits 10-9  = 00b
 *    Async Mode         -->  Bit 8      = 0b
 *    SMCLK Source       -->  Bits 7-6   = 11b
 *    Software Reset     -->  Bit 0      = 1b during setup (holds the module in reset), 0b to run
 *    Bit mask --> 0x00C1
 *
 * -- MODULATION SETUP (MCTLW) --
 *    No oversampling / modulation --> entire register cleared
 *
 * -- BAUD RATE (BRW) --
 *    Clock = 12 MHz (SMCLK), Baud = 9600
 *    BRW = 12 MHz / 9600 = 1250
 *
 * -- INTERRUPT SETUP (IFG & IE) --
 *    Clear the RX flag (Bit 0) only; the TX flag must stay set for OutChar
 *    Enable the RX interrupt only (after releasing reset)
 *
 * -- INTERRUPT CONTROLLER SETUP (NVIC) --
 *    Priority level = 1
 *    Enable the eUSCI_A2 interrupt
 */
void HC05_UART_Init()
{
    // Pin setup: P3.2 = UCA2RXD, P3.3 = UCA2TXD
    P3->SEL0 |=  0x0C;
    P3->SEL1 &= ~0x0C;

    // UART setup (module held in reset while configuring)
    EUSCI_A2->CTLW0 |=  0x0001;                 // Hold eUSCI_A2 in reset for configuration
    EUSCI_A2->MCTLW &= ~0xFF;                   // Clear modulation control word
    EUSCI_A2->CTLW0 |=  0x00C1;                 // 8N1, LSB first, UART, async, SMCLK

    EUSCI_A2->BRW = 1250;                       // 9600 baud from 12 MHz SMCLK

    EUSCI_A2->CTLW0 &= ~0x01;                   // Release reset

    // Interrupt setup
    EUSCI_A2->IFG &= ~0x01;                     // Clear RX flag
    EUSCI_A2->IE  |=  0x01;                     // Enable RX interrupt

    NVIC_SetPriority(EUSCIA2_IRQn, 1);          // Set interrupt priority level
    NVIC_EnableIRQ(EUSCIA2_IRQn);               // Enable eUSCI_A2 interrupt in the NVIC
}



// ============================== INTERRUPT HANDLER ===========================

/*
 * eUSCI_A2 interrupt handler, runs each time a byte is received.
 *
 * Reads the byte from RXBUF and stores it at the head of the ring buffer.
 * If the buffer is full (next head location equals tail), the byte is dropped.
 */
void EUSCIA2_IRQHandler(void)
{
    if (EUSCI_A2->IFG & 0x0001)
    {
        uint8_t byte = (uint8_t)EUSCI_A2->RXBUF;                                    // Reading RXBUF clears UCRXIFG

        uint8_t next_byte_location = (ring_buff_head + 1) & (HC05_RX_SIZE - 1);     // Next head location (wraps to 0)

        if (next_byte_location != ring_buff_tail)                                   // Not full: next location is not the tail
        {
            ring_buffer[ring_buff_head] = byte;                                     // Store data in the ring buffer
            ring_buff_head = next_byte_location;                                    // Then move the head forward
        }
    }
}


// ============================== RECEIVE / TRANSMIT ==========================

/*
 * Waits until a byte is available in the ring buffer, then returns it.
 * Returns the 8-bit character read from the tail of the ring buffer.
 */
uint8_t HC05_UART_InChar()
{
    while (ring_buff_head == ring_buff_tail);                                       // Wait while the buffer is empty

    uint8_t byte = ring_buffer[ring_buff_tail];                                     // Read data at the tail
    ring_buff_tail = (ring_buff_tail + 1) & (HC05_RX_SIZE - 1);                     // Move the tail forward (wraps to 0)

    return byte;
}


/*
 * Waits until the eUSCI_A2 transmit buffer is ready, then sends one byte to the HC-05 module.
 * Takes in the 8-bit character to transmit.
 */
void HC05_UART_OutChar(uint8_t data)
{
    while ((EUSCI_A2->IFG & 0x02) == 0);                                            // Wait for TX buffer empty
    EUSCI_A2->TXBUF = data;
}


/*
 * Sends a null-terminated string to the HC-05 module one character at a time.
 * Takes in a pointer to the string to transmit.
 */
void HC05_UART_OutString(char *pt)
{
    while (*pt)
    {
        HC05_UART_OutChar(*pt);
        pt++;
    }
}


// ============================== TEXT COMMANDS ===============================

/*
 * Reads characters from the HC-05 module into a buffer until a line feed (LF) is received.
 *
 * Handles backspace (BS) by removing the last character, ignores characters once the buffer is full,
 * and null-terminates the string.
 *
 * Takes a pointer to the character buffer that stores the received string,
 * and the maximum number of characters the buffer can hold.
 *
 * Returns the number of characters stored in the buffer.
 */
int HC05_UART_InString(char *buffer_pointer, uint16_t buffer_size)
{
    int length = 0;
    int string_size = 0;

    char character = HC05_UART_InChar();

    while (character != LF)
    {
        if (character == BS)
        {
            if (length)
            {
                buffer_pointer--;
                length--;
                HC05_UART_OutChar(BS);
            }
        }
        else if (length < buffer_size)
        {
            *buffer_pointer = character;
            buffer_pointer++;
            length++;
            string_size++;
        }

        character = HC05_UART_InChar();
    }

    *buffer_pointer = 0;
    return string_size;
}


/*
 * Checks whether a given string appears anywhere in the HC-05 UART data buffer.
 *
 * Takes in the buffer holding the received HC-05 data and the string to search for.
 * Returns 0x01 if found, 0x00 if not.
 */
uint8_t Check_HC05_UART_Data(char HC05_UART_Data_Buffer[], char *data_string)
{
    if (strstr(HC05_UART_Data_Buffer, data_string) != NULL)
    {
        return 0x01;
    }
    else
    {
        return 0x00;
    }
}


// ============================== CONTROLLER PACKETS ==========================

/*
 * Discards incoming characters until the '!' header is received, then stores the header and the next 4 bytes
 * (packet type, button number, pressed/released, checksum) in the buffer without waiting for a line feed.
 */
void HC05_UART_InPacket(char *buffer_pointer)
{
    char character;

    // Discard incoming characters until the '!' (0x21) packet header is received
    do
    {
        character = HC05_UART_InChar();
    } while (character != '!');

    buffer_pointer[0] = character;

    for (int i = 1; i < 5; i++)
    {
        buffer_pointer[i] = HC05_UART_InChar();
    }
}


/*
 * Decodes a controller button packet ("!B") and drives the robot:
 * buttons 5-8 move forward, backward, left, or right while pressed,
 * and any button release stops the motors.
 */
void Decode_Controller_Packet(char *buffer_pointer)
{
    if (buffer_pointer[0] != 0x21 || buffer_pointer[1] != 0x42)                     // Not a "!B" packet
    {
        return;
    }

    if (buffer_pointer[3] == 0x30)                                                  // '0' = button released
    {
        printf("Motor Stop \n");
        Motor_Stop();
        return;
    }

    if (buffer_pointer[2] == 0x35)                                                  // Button '5'
    {
        printf("Motor Forward \n");
        Motor_Forward(PWM_NOMINAL, PWM_NOMINAL);
    }
    else if (buffer_pointer[2] == 0x36)                                             // Button '6'
    {
        printf("Motor Backward \n");
        Motor_Backward(PWM_NOMINAL, PWM_NOMINAL);
    }
    else if (buffer_pointer[2] == 0x37)                                             // Button '7'
    {
        printf("Motor Left \n");
        Motor_Left(PWM_NOMINAL, PWM_NOMINAL);
    }
    else if (buffer_pointer[2] == 0x38)                                             // Button '8'
    {
        printf("Motor Right \n");
        Motor_Right(PWM_NOMINAL, PWM_NOMINAL);
    }
}





// ============================== DATA IN BUFFER =============================



int HC05_UART_Data_Ready(void)
{

  return ring_buff_head != ring_buff_tail;                                          // Simple check to see if Data is in Buffer (not match data is in there)

}

