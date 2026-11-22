/*
 * Packet.c
 *
 *  Created on: Nov 20, 2026
 *      Author: danie
 */

#include <stdio.h>
#include "../inc/Packet.h"
#include "../inc/HC05_UART.h"


Packet_Stats_t pkt_stats = {0};


uint8_t Packet_Checksum(const uint8_t *p, int n)
{
    uint8_t checksum = 0;

    for (int i = 0; i < n; i++)
    {
        checksum = checksum ^ p[i];
    }

    return checksum;
}



int Packet_Poll(Command_t *out, char *buffer_pointer)
{
    static uint8_t packet[6];   // holds the bytes of the packet so far
    static uint8_t index = 0;         // how many bytes we have so far
    static uint8_t expected = 0;      // how many bytes this packet needs (6 or 5)



    while (HC05_UART_Data_Ready() == 1)
    {
        uint8_t character = HC05_UART_InChar();   // take one byte


        if (index == 0)
        {
            if (character == 0xA5)
            {
                expected = 6;       // 6
            }
            else if (character == '!')
            {
                expected = 5;      // 5
            }
            else
            {
                continue;                 // junk byte, skip it
            }
        }


        packet[index] = character;      // Place in Byte into packet
        index = index + 1;


        if (index < expected)          // Still fillup packet with bytes to complete the expected length
        {                              // Loops back to the beginning if true, other counitues foward
            continue;
        }


        index = 0;


        if (expected == 5)       // Checking for old 5 Byte structure
        {


            for (int i = 0; i < 5; i++)
            {
                buffer_pointer[i] = packet[i];
            }
            return 2;           // Telling us type of Packet coming out of Function
        }

        if (Packet_Checksum(packet, 5) != packet[5])    //
        {
            pkt_stats.bad_chk = pkt_stats.bad_chk + 1;
            continue;
        }


        out->seq  = packet[1];
        out->cmd  = packet[2];
        out->arg1 = (int8_t)packet[3];
        out->arg2 = (int8_t)packet[4];

        pkt_stats.ok = pkt_stats.ok + 1;
        return 1;
    }

    return 0;
}



