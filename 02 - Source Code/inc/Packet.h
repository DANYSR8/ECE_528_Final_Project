/*
 * Packet.h
 *
 *  Created on: Nov 20, 2026
 *      Author: danie
 */

#ifndef INC_PACKET_H_
#define INC_PACKET_H_

#include <stdint.h>

#define PACK_SYNC     0xA5
#define PACK_LEN      6          // [A5][SEQ][CMD][ARG1][ARG2][CHK]
#define BANG_LEN     5          // old "!B51x" packets



//================= Laptop Command Types =========== ============================
typedef enum { CMD_DRIVE = 0x01,
                CMD_STOP = 0x02,
                CMD_HEARTBEAT = 0x03

             } Cmd_Type;


// ===================== Packet Structure Definitions ============================
typedef struct { uint8_t seq; uint8_t cmd; int8_t arg1; int8_t arg2; } Command_t;




// ===================== Packet Status Definitions ================================
typedef struct { uint32_t ok; uint32_t bad_chk; uint32_t dropped; } Packet_Stats_t;
extern Packet_Stats_t pkt_stats;


// =================== Function Declaration =======================================
uint8_t Packet_Checksum(const uint8_t *p, int n);   // XOR of n bytes
int Packet_Poll(Command_t *out, char *buffer_pointer); // 1 = new packet, 2 = "!B" packet, 0 = nothing yet



#endif /* INC_PACKET_H_ */
