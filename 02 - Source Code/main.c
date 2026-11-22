#include "msp.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "inc/Clock.h"
#include "inc/CortexM.h"
#include "inc/GPIO.h"
#include "inc/EUSCI_A0_UART.h"
#include "inc/Motor.h"
#include "inc/HC05_UART.h"
#include "inc/Timer_A0_PWM.h"
#include "inc/Timer_A1_Interrupt.h"
#include "inc/Bumper_Switches.h"
#include "inc/SysTick_Interrupt.h"
#include "inc/Packet.h"




// ============ DEFINE CONSTANTS =====================================================================



// ====================================================================================================
/*
 *
 *
 *
 *
 */
// =============== Global Variables ==================================================================


// Global flag that gets set in Bumper_Switches_Handler.
// This is used to detect if any collisions occurred when any one of the bumper switches are pressed.
volatile uint8_t collision_detected = 0;



// Initialize a global variable for SysTick to keep track of elapsed time in milliseconds resets every 500ms
volatile uint32_t SysTick_ms_elapsed = 0;

// Initlaize global varible to keep track how many milliseconds have gone by since the loop has started
// Increments with SysTick Handaler the Excutes every ms
volatile uint32_t Elapsed_ms = 0;

// ====================================================================================================
/*
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
// =============== Additional Handler Functions =======================================================


void TX_Msg_Test(void)
{

    HC05_UART_OutChar(170);
    Clock_Delay1ms(1);
    HC05_UART_OutChar(85);
    Clock_Delay1ms(1);
    HC05_UART_OutChar(255);
    Clock_Delay1ms(1);
    HC05_UART_OutChar(0);
    Clock_Delay1ms(1);

}


/**
 * @brief Bumper switch interrupt handler function.
 *
 * This is the interrupt handler for the bumper switch interrupts. It is called when a falling edge event is detected on
 * any of the bumper switch pins. The function checks if a collision has already been detected; if not, it prints a collision
 * detection message along with the bumper switch state and sets the collision_detected flag to prevent further detections.
 *
 * @param bumper_switch_state An 8-bit unsigned integer representing the bumper switch states at the time of the interrupt.
 *
 * @return None
 */
void Bumper_Switches_Handler(uint8_t bumper_switch_state)
{
    if (collision_detected == 0)
    {
    printf("Collision Detected! Bumper Switch State: 0x%02X\n", bumper_switch_state);
    collision_detected = 1;

    // Temporray Motor Stop
    Motor_Stop();

    }
}


/**
 * @brief Interrupt service routine for the SysTick timer.
 *
 * The interrupt service routine for the SysTick timer increments the SysTick_ms_elapsed
 * global variable to keep track of the elapsed milliseconds. If collision_detected is 0, then
 * it checks if 500 milliseconds passed. It toggles the front yellow LEDs and turns off the back red LEDs
 * on the chassis board. Otherwise, if collision_detected is set, it turns off the front yellow LEDs
 * and turns on the back red LEDs on the chassis board.
 *
 * @param None
 *
 * @return None
 */
void SysTick_Handler(void)
{
    SysTick_ms_elapsed++;
    Elapsed_ms++;

    if (collision_detected == 0)
    {
        if (SysTick_ms_elapsed >= 500)
        {
            P8->OUT &= ~0xC0;
            P8->OUT ^= 0x21;
            SysTick_ms_elapsed = 0;
        }
    }

    else
    {
        P8->OUT |= 0xC0;
        P8->OUT &= ~0x21;
    }
}


/*
 *  Function aims to handle what to do when collision is detected, this is where we mostlty enter Safe mode
 */
void Handle_Collision()
{
    collision_detected = 0;
}




/*
 *  Function aims to check motor are functioning, can potentially modify as a BIT check
 */
void MotorCheck(void){


    Motor_Forward(PWM_NOMINAL, PWM_NOMINAL);
    Clock_Delay1ms(100);

    Motor_Stop();
    Clock_Delay1ms(1000);


    Motor_Backward(PWM_NOMINAL, PWM_NOMINAL);
    Clock_Delay1ms(100);

    Motor_Stop();
    Clock_Delay1ms(1000);


    Motor_Left(PWM_NOMINAL, PWM_NOMINAL);
    Clock_Delay1ms(100);

    Motor_Stop();
    Clock_Delay1ms(1000);


    Motor_Right(PWM_NOMINAL, PWM_NOMINAL);
    Clock_Delay1ms(100);

    Motor_Stop();
    Clock_Delay1ms(3000);


}



void Process_HC05_UART_Data(char HC05_UART_Buffer[])
{

    if(Check_HC05_UART_Data(HC05_UART_Buffer, "RGB LED GREEN"))
        {
            LED2_Output(RGB_LED_GREEN);

        }
    else if (Check_HC05_UART_Data(HC05_UART_Buffer,"RGB LED OFF"))
        {

            LED2_Output(RGB_LED_OFF);

        }


    else if(Check_HC05_UART_Data(HC05_UART_Buffer,"DANY"))
        {
                TX_Msg_Test();
                printf("Sending TX Msg A \n");
        }

    else
        {

        printf("HC05 UART Command Not Found \n");

        }

}




void Motor_Set(int8_t left_percent, int8_t right_precent) // Drives both motors from a percent value (-100 to 100). Negative = reverse.
{
    // Keeping values in range just in case accidental overshoot
    if (left_percent  >  100)
    {
        left_percent  =  100;
    }

    if (left_percent  < -100)
    {
        left_percent  = -100;
    }

    if (right_precent >  100)
    {
        right_precent =  100;
    }


    if (right_precent < -100){
        right_precent = -100;
    }


    // Percent to PWM: 100% = 14000 (period is 15000)
    uint16_t left_pwm  = abs(left_percent)  * 140;
    uint16_t right_pwm = abs(right_precent) * 140;


    if (left_percent == 0 && right_precent == 0)
    {
        Motor_Stop();
    }

    else if (left_percent >= 0 && right_precent >= 0)
    {
        Motor_Forward (left_pwm, right_pwm);
    }

    else if (left_percent <  0 && right_precent <  0)
    {
        Motor_Backward(left_pwm, right_pwm);
    }
    else if (left_percent <  0)
    {
        Motor_Left    (left_pwm, right_pwm);   // left back, right forward
    }

    else
    {
        Motor_Right(left_pwm, right_pwm);   // left forward, right back
    }
}




// ========================================================================================================
/*
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
// ================================ MAIN Loop ===========================================================
// ======================================================================================================
void main(void)
{

// Initialization

    DisableInterrupts();                                // Disables Interrupts from Cortex.c
    Clock_Init48MHz();                                  // Enables 48MHz Clock 'Master/System Clock'
    LED1_Init();                                        // Enables Red LED 'One Color'
    LED2_Init();                                        // Enables RGB LED 'MulitColor'
    EUSCI_A0_UART_Init_Printf();                        // Enables UART messages to be put in printf function

    Motor_Init();                                       // Enables Motors/Motor Driver to be used

    Bumper_Switches_Init(&Bumper_Switches_Handler);     // Enables Bumper Switch to be use as in Motor Control lab
    SysTick_Interrupt_Init(48000, 2);                   // Enables System Clock Interrupts for 48000 Clock Cycles priority level 2
    EnableInterrupts();                                 // ReEnables Interrupts

    HC05_UART_Init();                                   // Enables HC05 UART Bluetooth Comms


    char HC05_UART_Buffer[HC05_UART_BUFFER_SIZE] = {0};
    uint32_t Last_RX_ms = 0;
    uint8_t Link_Lost = 0;

    Command_t LaptopCMD;

    while(1)
    {

    // -- Motor Testing --
        //MotorCheck();
        //Handle_Collision();



    // -- UART with Laptop  --
    int PacketType = Packet_Poll(&LaptopCMD, HC05_UART_Buffer);

    if (PacketType == 1)                            // new 6-byte packet
    {
        Last_RX_ms = Elapsed_ms;
        Link_Lost = 0;

        if (LaptopCMD.cmd == CMD_DRIVE)
        {
            if (collision_detected == 0)
            {
                Motor_Set(LaptopCMD.arg1, LaptopCMD.arg2);
            }
        }
        else if (LaptopCMD.cmd == CMD_STOP)
        {
            Motor_Stop();
            if (collision_detected == 1 && Bumper_Read() == 0)   // clear bump only if no switch pressed
            {
                Handle_Collision();
                printf("Collision cleared \n");
            }
        }
        // CMD_HEARTBEAT: nothing else to do, the link timer was already reset



    }
    else if (PacketType == 2)                       // Old "!B" packet
    {
        Last_RX_ms = Elapsed_ms;
        Link_Lost = 0;

        if (collision_detected == 0)
        {
            Decode_Controller_Packet(HC05_UART_Buffer);
        }
        else if (HC05_UART_Buffer[3] == '0')
        {
            Handle_Collision();
            printf("Collision cleared \n");
        }
    }



    // -- Link lost (once per loss) --
    if (Link_Lost == 0 && (Elapsed_ms - Last_RX_ms) > 300)
    {
        Motor_Stop();
        printf("LINK LOST \n");
        Link_Lost = 1;
    }



    /* OLD CONTROL TEST METHOD USING TERRATERM
     * // -- UART --
    if (HC05_UART_Data_Ready() == 1){

        Last_RX_ms = Elapsed_ms ;
        Link_Lost = 0 ;
        HC05_UART_InPacket(HC05_UART_Buffer);


        // -- Bumper check --
        if (collision_detected == 0)
        {
            Decode_Controller_Packet(HC05_UART_Buffer);   // drive normally
        }
        else if (HC05_UART_Buffer[3] == '0')        // a stop packet (!B50x) clears the bump
        {
            Handle_Collision();
            printf("Collision cleared \n");
        }
    }
    */

    }

}
