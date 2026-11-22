/**
 * @file HC05_UART.h
 *
 * @brief Header file for the HC05_UART driver.
 *
 * This file contains the function definitions for the HC05_UART driver.
 *
 * It interfaces with the HC-05 Bluetooth Module with 6PIN Base Board, which uses the UART communication protocol.
 *  - Product Link: https://deshide.com/product-details_HC-05.html
 *
 *  COM Port for My Comupter Outgoing is COM6 / Incoming is COM7
 *
 * The following connections must be made (Orientation Front side with button, Pin Number Left to Right):
 *  - UART STATE  (Pin 1)     <-->  Not Connected
 *  - UART RXD    (Pin 2)     <-->  MSP432 LaunchPad Pin P3.2
 *  - UART TXD    (Pin 3)     <-->  MSP432 LaunchPad Pin P3.3
 *  - UART GND    (Pin 4)     <-->  MSP432 LaunchPad Pin GND
 *  - UART VCC    (Pin 5)     <-->  MSP432 LaunchPad PIN 5V
 *  - UART EN     (Pin 6)     <-->  Not Connected
 *
 * @note For more information regarding the Enhanced Universal Serial Communication Interface (eUSCI),
 * refer to the MSP432Pxx Microcontrollers Technical Reference Manual
 *
 * @author
 *
 */

#ifndef INC_HC05_UART_H_
#define INC_HC05_UART_H_

#define HC05_UART_BUFFER_SIZE 128                         // Complete Data Holder
#define CR 0x0D
#define LF 0x0A
#define BS 0x08
#define ESC 0x1B
#define SP 0x20
#define DEL 0x7F



#include <stdint.h>
#include <string.h>
#include "msp.h"
#include "Clock.h"
#include "Motor.h"

// Move when Decode Packet need to be moved
#define PWM_NOMINAL 1500 // Deadband NO LOAD ( L =550 R=550) ~  DeadBand with LOAD (L =575   R =575 )


void HC05_UART_Init();

uint8_t HC05_UART_InChar();

void HC05_UART_OutChar(uint8_t data);

int HC05_UART_InString(char *buffer_pointer, uint16_t buffer_size);

void HC05_UART_InPacket(char *buffer_pointer);

void Decode_Controller_Packet(char *buffer_pointer);

void HC05_UART_OutString(char *pt);

uint8_t Check_HC05_UART_Data(char HC05_UART_Data_Buffer[], char *data_string);

int HC05_UART_Data_Ready();

#endif /* INC_HC05_UART_H_ */
