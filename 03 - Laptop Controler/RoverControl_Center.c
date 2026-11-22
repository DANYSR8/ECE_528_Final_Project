/*
Used the following tutorial to help create this code:
- https://developer.toradex.com/windows-ce/knowledge-base/how-to-use-uart-on-windows-compact-embedded/
- https://www.educative.io/answers/what-is-conioh-in-c
- https://learn.microsoft.com/en-us/windows/console/creation-of-a-console
- 

*/

#include <windows.h>     // Windows functions: (CreateFileA, WriteFile, ReadFile...)
#include <stdio.h>       // printf
#include <stdint.h>      // uint8_t, int8_t
#include <conio.h>       // Console Functions (kbhit, getch)

#define PKT_SYNC       0xA5
#define CMD_DRIVE      0x01
#define CMD_STOP       0x02
#define CMD_HEARTBEAT  0x03

static uint8_t seq = 0;  // sequence number, goes up by 1 every packet, to be used by the rover to detect lost packets 


// ============================ Packet Functions ============================

// -- Checksum Function -----------------------------------------------------
// XOR of n bytes. Must be the same as Packet_Checksum() on the rover. Otherwise check will show wrong all the time 
uint8_t Packet_Checksum(const uint8_t *p, int n)
{
    uint8_t checksum = 0;
    for (int i = 0; i < n; i++)
    {
        checksum = checksum ^ p[i];
    }
    return checksum;
}


// -- Send Packet Function --------------------------------------------------
// Builds one 6-byte packet and sends it. Returns 1 if sent, 0 if not.
int Send_Packet(HANDLE port, uint8_t cmd, int8_t arg1, int8_t arg2)
{
    uint8_t packet[6];

    packet[0] = PKT_SYNC;                        // 0xA5 Header for all packets 
    packet[1] = seq;                             // Sequece Number used to see if any packets were lost
    packet[2] = cmd;                             // Type of command being sent (drive, stop, heartbeat)
    packet[3] = (uint8_t)arg1;                   // Motor 1 PWM 
    packet[4] = (uint8_t)arg2;                   // Motor 2 PWM 
    packet[5] = Packet_Checksum(packet, 5);      // XOR of bytes 0-4

    seq = seq + 1;                               // increment sequence number for next packet

    DWORD written = 0;                                                          
    if (!WriteFile(port, packet, 6, &written, NULL) || written != 6)            // Function declartion has the following parameters: (Handle/Port to Write To , Data to Write , How Many Bytes to Write , Bytes Written Already,  Any overlapped data (not used here))
    {
        printf("Send failed (error %lu)\n", GetLastError());
        return 0;
    }

    printf("Sent: %02X %02X %02X %02X %02X %02X\n",
           packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
    return 1;
}





// ============================ Main Loop ============================

int main(int argc, char *argv[])
{

    // ---- Get the COM port name from the command line, e.g. "COM7" ----
    if (argc < 2)
    {
        printf("Usage: rover_ctl COM7\n");
        return 1;
    }

    // Windows needs the name written as \\.\COM7
    char path[32];
    snprintf(path, sizeof(path), "\\\\.\\%s", argv[1]);





    // ---- Open the port & Configure to be called by 'port' ----
    HANDLE port = CreateFileA(path,
                              GENERIC_READ | GENERIC_WRITE,   // Enable read and write
                              0, NULL,
                              OPEN_EXISTING,                  // port must already exist
                              0, NULL);

    if (port == INVALID_HANDLE_VALUE)
    {
        printf("Could not open %s (error %lu)\n", argv[1], GetLastError());
        return 1;
    }




    // ---- Configure UART Comms setting Like on Rover Set 9600 baud, 8 data bits, no parity, 1 stop bit ----
    DCB settings = {0};
    settings.DCBlength = sizeof(settings);
    GetCommState(port, &settings);          // start from the current settings

    settings.BaudRate = CBR_9600;
    settings.ByteSize = 8;
    settings.Parity   = NOPARITY;
    settings.StopBits = ONESTOPBIT;

    if (!SetCommState(port, &settings))
    {
        printf("Could not set 9600 8N1 (error %lu)\n", GetLastError());
        CloseHandle(port);
        return 1;
    }





    // ----  Make ReadFile return immediately instead of waiting ----
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout       = MAXDWORD;   // "return whatever is there, even nothing"
    timeouts.WriteTotalTimeoutConstant = 500;        // give up on a write after 0.5 s
    SetCommTimeouts(port, &timeouts);

    printf("Connected to %s at 9600 baud\n", argv[1]);





    // ---- Keyboard Control ----
    int8_t    left      = 0;        // current left wheel command (%)
    int8_t    right     = 0;        // current right wheel command (%)
    int       send_stop = 0;        // 1 = send one STOP on the next send
    ULONGLONG last_send = 0;

    printf("W/A/S/D = drive   Space = stop   Q = quit\n");

    while (1)
    {
        // ---- 1. Keyboard (doesn't wait) ----
        if (_kbhit())                                   // a key is waiting?
        {
            int key = _getch();                         // take it

            if      (key == 'w' || key == 'W') { left =  30; right =  30; }   // forward
            else if (key == 's' || key == 'S') { left = -30; right = -30; }   // backward
            else if (key == 'a' || key == 'A') { left = -20; right =  20; }   // spin left
            else if (key == 'd' || key == 'D') { left =  20; right = -20; }   // spin right
            else if (key == ' ')               { left = 0; right = 0; send_stop = 1; }  // stop
            else if (key == 'q' || key == 'Q') { break; }                     // quit

            printf("Key '%c'  ->  L %d%%  R %d%%\n", key, left, right);
        }

        // --- Send a packet every 100 ms ----
        ULONGLONG now = GetTickCount64();

        if (now - last_send >= 100)
        {
            last_send = now;

            if (send_stop == 1)                         // Space was pressed: one STOP
            {
                Send_Packet(port, CMD_STOP, 0, 0);
                send_stop = 0;
            }
            else if (left != 0 || right != 0)           // moving: keep sending DRIVE
            {
                Send_Packet(port, CMD_DRIVE, left, right);
            }
            else                                        // stopped: just "still here"
            {
                Send_Packet(port, CMD_HEARTBEAT, 0, 0);
            }
        }

        Sleep(5);
    }

    Send_Packet(port, CMD_STOP, 0, 0);                  // stop the robot before closing
    printf("Quit.\n");




    // ---- X. Close the port ----
    CloseHandle(port);
    return 0;
}