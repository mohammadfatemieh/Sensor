/* 
 * File:   main.c
 * Author: pherget
 *
 * Created on June 3, 2015, 1:13 PM
 */

#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"

#define _XTAL_FREQ 8000000 // The speed of the internal(or)external oscillator
#define CHOLD 45           // Hold capacitor size in pF for CVD
#define MAX_RX_BUFFER 40   // Maximum size for the RX buffer
#define MAX_BUFFER 40      // Maximum size for the other buffer
// GardenSense Private Service UUID is ACD63933-AE9C-393D-A313-F75B3E5F9A8E
#define UUID_PREFIX "ACD63933AE9C393DA313F75B3E5F9A8"  // GardenSense Private Service UUID PREFIX
#define UUID_BTID "1"  // Last UUID character for The BTLE ID of the Connected Sensor
#define UUID_DATA "E"  // Last UUID character for Sensor Data Service
#define UUID_MODE "F"  // Last UUID character for Sensor Mode Service
#define UUID_BAT  "B"  // Last UUID character for Sensor Battery Service
#define UUID_POW  "C"  // Last UUID character for Sensor Power Service
#define UUID_TEMP "D"  // Last UUID character for Sensor Temperature Service

void putch(char data);
void SetupSleepTimer();
void SetupClock();
void SetupUSART();
void SetupRN4020();
void RN4020Config();
void WaitButton();
void PrintRXBuffer();
void PrintBuffer();

int ReadCap(unsigned char sensorNumber);
char * ReadLine(char * buffer);
void ClearRXBuffer();
char WaitRXBuffer(char ticks);
void PrintLines(void);

static int CVD_Init = 0;
static unsigned char RXbuffer[MAX_RX_BUFFER];
static unsigned char RXbufferStart;
static unsigned char RXbufferEnd;
static unsigned char RXbufferLastChar;
static unsigned char RXbufferCount;
static unsigned char RXbufferOverflow;
static unsigned char RXbufferHasNewline;

unsigned char buffer[MAX_BUFFER];
unsigned char BTA[14];

static int printcounts = 0;

static char timerCounts;

float batV = 0;

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned int cnt = 0;
    unsigned int i = 0;
    unsigned char temp;
    unsigned int readADC = 0;
    unsigned int readBat = 0;
    unsigned int readTemp = 0;
    unsigned char sleepCycles = 12;
    float voltageRatio;
    float voltage;
    float capacitance;

    SetupClock();
    SetupUSART();
    SetupSleepTimer();
    SetupRN4020();
    
    // Setup Port B LEDs
    PORTBbits.RB4 = 0;
    TRISBbits.TRISB4 = 0;
    ANSELBbits.ANSB4 = 0;       // Digital input
    PORTBbits.RB6 = 0;
    TRISBbits.TRISB6 = 0;
    ANSELBbits.ANSB6 = 0;       // Digital input
    // Setup Port A2 Switch
    TRISAbits.TRISA2 = 1;
    ANSELAbits.ANSA2 = 0;       // Digital input



    //putch('+');

    //Forever While Loop
    while(1){

        /*for(cnt = 1; cnt < 9; cnt = cnt +2)
        {
            readADC = ReadCap(cnt);
            voltageRatio = (readADC/1023.0);
            voltage = voltageRatio*3.3;
            capacitance = CHOLD*((1-voltageRatio)/voltageRatio);

            //sprintf(buffer, "S1+2: %d, %1.2fV, %.1fpF  ", readADC, voltage, capacitance);
            printf("S%d: %.1fpF  ", cnt, capacitance);
        }*/

        // Configure the Sensor if the button is pushed for 5 seconds
        if(PORTAbits.RA2 == 1)
        {
            temp = 0;
            PORTBbits.RB6 = 1;
            for(cnt = 0; cnt < 40 && temp == 0; cnt++)
            {
                if(PORTAbits.RA2 == 0){
                    temp = 1;
                }
                __delay_ms(125);

                PORTBbits.RB6 = ! PORTBbits.RB6;
            }

            PORTBbits.RB6 = 0;

            if(temp == 0){
                ClearRXBuffer();

                // Turn on LED
                PORTBbits.RB6 = 1;

                // WAKE_HW on RA5
                PORTAbits.RA5 = 1;          // Wakeup Hardware
                // WAKE_SW on RA4
                __delay_ms(100);
                PORTAbits.RA4 = 1;          // Wake Software
                //printf("Turning RN4020 On\n\r");

                // Wait for 'CMD', and check for shifted versions too
                //PORTCbits.RC7 = 0;
                temp = WaitRXBuffer(1);
                ReadLine(buffer);
                //printf(",");
                while(temp && !((buffer[0] == 'C' && buffer[1] == 'M' && buffer[2] == 'D') ||
                        (buffer[1] == 'C' && buffer[2] == 'M' && buffer[3] == 'D') ||
                        (buffer[2] == 'C' && buffer[3] == 'M' && buffer[4] == 'D')))
                {
                    //printf(",");
                    temp = WaitRXBuffer(1);
                    ReadLine(buffer);
                }
                if(temp == 0){
                    printf("TIMEOUT!\n");
                    PrintRXBuffer();
                }

                // Turn on the echo and check to make sure it's actually on
                printf("+\n");
                temp = WaitRXBuffer(1);
                ReadLine(buffer);

                // Wait for the correct response
                while(temp && !((buffer[0] == 'E' && buffer[1] == 'c' && buffer[2] == 'h')) )
                        //(RXbuffer[1] == 'E' && RXbuffer[2] == 'c' && RXbuffer[3] == 'h') ||
                        //(RXbuffer[2] == 'E' && RXbuffer[3] == 'c' && RXbuffer[4] == 'h')))
                {
                    //printf(",");
                    //putch('#');  printf(buffer);  putch('\n');
                    temp = WaitRXBuffer(1);
                    ReadLine(buffer);
                    //putch('#');  printf(buffer);  putch('\n');
                }
                if(temp == 0){
                    printf("#TIMEOUT!\n");
                }

                // if the echo is now off, turn it back on.
                if(buffer[5] == 'O' && buffer[6] == 'f' )
                {
                    printf("+\n");
                }

                __delay_ms(200);
                ClearRXBuffer();

                RN4020Config();

            }
            
        // Turn off LED
        PORTBbits.RB6 = 0;

        }

        while(1){

            ClearRXBuffer();

            // Turn on LED while serving data
            //PORTBbits.RB6 = 1;

            // WAKE_HW on RA5
            PORTAbits.RA5 = 1;          // Wakeup Hardware
            // WAKE_SW on RA4
            __delay_ms(100);
            PORTAbits.RA4 = 1;          // Wake Software
            //printf("Turning RN4020 On\n\r");

            // Wait for 'CMD', and check for shifted versions too
            temp = WaitRXBuffer(1);
            ReadLine(buffer);

            while(temp && !((buffer[0] == 'C' && buffer[1] == 'M' && buffer[2] == 'D') ||
                    (buffer[1] == 'C' && buffer[2] == 'M' && buffer[3] == 'D') ||
                    (buffer[2] == 'C' && buffer[3] == 'M' && buffer[4] == 'D')))
            {
                temp = WaitRXBuffer(1);
                ReadLine(buffer);
            }
            if(temp == 0){
                printf("TIMEOUT!\n");
                PrintRXBuffer();
            }

            // Wait a bit to make sure we have all extra data and then clear the buffer
            __delay_ms(200);
            ClearRXBuffer();

            // Turn on the echo and check to make sure it's actually on
            printf("+\n");
            temp = WaitRXBuffer(1);
            ReadLine(buffer);

            // Wait for the correct response
            while(temp && !(buffer[0] == 'E' && buffer[1] == 'c' && buffer[2] == 'h') )
            {
                //printf(",");
                //putch('#');  printf(buffer);  putch('\n');
                temp = WaitRXBuffer(1);
                ReadLine(buffer);
                //putch('#');  printf(buffer);  putch('\n');
            }
            if(temp == 0){
                printf("#TIMEOUT!\n");
            }

            // if the echo is now off, turn it back on.
            if(buffer[5] == 'O' && buffer[6] == 'f' )
            {
                printf("+\n");
            }

            __delay_ms(100);
            ClearRXBuffer();

            //printf("LS\n");
            //__delay_ms(100);

            //printf("GS\n");
            //__delay_ms(100);

            printf("V\n");
            __delay_ms(100);

            ClearRXBuffer();
            printf("D\n");
            //__delay_ms(200);

            temp = WaitRXBuffer(1);
            ReadLine(buffer);

            while(temp && !((buffer[0] == 'B' && buffer[1] == 'T' && buffer[2] == 'A') ||
                    (buffer[1] == 'B' && buffer[2] == 'T' && buffer[3] == 'A')))
            {
                //PrintRXBuffer();
                temp = WaitRXBuffer(1);
                ReadLine(buffer);
            }
            if(temp == 0){
                printf("TIMEOUT!\n");
                PrintRXBuffer();
            } else {
                __delay_ms(100);
                // Copy the Bluetooth Address into the BTA variable
                for(i=0; buffer[i+3] != NULL && i < 14; i++)
                    BTA[i] = buffer[i+4];

                //putch('#');
                //printf(BTA);
                //putch('\n');
            }

            ClearRXBuffer();
            printf("F\n");
            __delay_ms(500);
            // Read the echo
            temp = WaitRXBuffer(1);
            ReadLine(buffer);
            //putch('#');  printf(buffer);  putch('\n');

            // Read the response
            temp = WaitRXBuffer(1);
            ReadLine(buffer);
            //putch('#');  printf(buffer);  putch('\n');

            // If it's "ERR"
            if(temp && buffer[0] == 'E' && buffer[1] == 'R' && buffer[2] == 'R')
            {
                // Stop the current scan
                printf("K\n");
                __delay_ms(500);
            }

            printf("E,0,001EC022A8BA\n");

            temp = WaitRXBuffer(3);
            ReadLine(buffer);

            // Wait for the response "Connected"
            while(temp && !((buffer[0] == 'C' && buffer[1] == 'o' && buffer[2] == 'n')) )
                    //(RXbuffer[1] == 'E' && RXbuffer[2] == 'c' && RXbuffer[3] == 'h') ||
                    //(RXbuffer[2] == 'E' && RXbuffer[3] == 'c' && RXbuffer[4] == 'h')))
            {
                //printf(",");
                //putch('#');  printf(buffer);  putch('\n');
                temp = WaitRXBuffer(3);
                ReadLine(buffer);
                //putch('#');  printf(buffer);  putch('\n');
            }
            if(temp == 0){
                printf("#TIMEOUT!\n");
            }

            __delay_ms(200);
            ClearRXBuffer();

//            printf("LC\n");
//            __delay_ms(1000);

            // Put the device's BTLE ID on the BTLE server
            printf("CUWV,");
            printf(UUID_PREFIX);
            printf(UUID_BTID);
            printf(",");
            printf(BTA);
            printf("\n");
            __delay_ms(20);

            // Read the soil moisture and put it on the BTLE server
            printf("CUWV,");
            printf(UUID_PREFIX);
            printf(UUID_DATA);
            printf(",");
            readADC = ReadCap(1);
            printf("%04x",readADC);
            readADC = ReadCap(2);
            printf("%04x",readADC);
            readADC = ReadCap(3);
            printf("%04x",readADC);
            readADC = ReadCap(4);
            printf("%04x",readADC);
            readADC = ReadCap(5);
            printf("%04x",readADC);
            readADC = ReadCap(6);
            printf("%04x",readADC);
            readADC = ReadCap(7);
            printf("%04x",readADC);
            printf("\n");
            __delay_ms(20);

            // Read the battery voltage by reading the FVR
            ADCON1 = 0B11010000;    //VDD and VSS VREF  FOSC/16 = 8MHz/16 = 0.5MHz = 2us
            FVRCON = 0B10100001;
            AD2CON0 = 0B01111101;   //Select channel FVR and turn off ADC 2
            //AAD1ACQ = 10;           //Acquisition Timer 10 us
            __delay_ms(2);
            AD2CON0bits.GO = 1;
            while(AD2CON0bits.GO == 1){
            }
//                readBat = FVRCON;
            //__delay_ms(2);
            readBat = AD2RES0;

            // Read the temperature
            AD2CON0 = 0B01110101;   // Select channel temperature and turn on ADC
            __delay_ms(2);            // Delay at least 200us
            AD2CON0bits.GO = 1;
            while(AD2CON0bits.GO == 1){
            }
            readTemp = AD2RES0;

            AD2CON0 = 0B01110100;   // turn off ADC
            FVRCON = 0B00000000;    // turn off FVR and TSEN

            // Put the battery voltage on the BTLE server
            printf("CUWV,");
            printf(UUID_PREFIX);
            printf(UUID_BAT);
            printf(",");
            printf("%04x", readBat);
            printf("\n");
            __delay_ms(20);

            // Put the temperature on the BTLE server
            printf("CUWV,");
            printf(UUID_PREFIX);
            printf(UUID_TEMP);
            printf(",");
            printf("%04x", readTemp);
            printf("\n");

            // Put the sleep cycles on the BTLE server
            //printf("SUW,");
            //printf(UUID_PREFIX);
            //printf(UUID_MODE);
            //printf(",");
            //printf("%02x", sleepCycles);
            //printf("\n");

            __delay_ms(100);

            // Wait for a disconnect to indicate the server read the data
            // Or timeout after 10 seconds
            /*
            int disconnect = 0;
            ClearRXBuffer();
            while(disconnect == 0 && timerCounts < 2){
                // if there's data
                if(RXbufferHasNewline > 0){
                    // Check if it's "Connection End'
                    if(RXbuffer[0] == 'C' && RXbuffer[1] == 'o' && RXbuffer[11] == 'E' )
                        disconnect = 1;
                    if(RXbuffer[0] == 'W' && RXbuffer[1] == 'V')
                    {
                        if((RXbuffer[8] >= '0' && RXbuffer[8] <= '9') ||
                           (RXbuffer[8] >= 'A' && RXbuffer[8] <= 'F'))
                        {
                            if(RXbuffer[8] < 'A')
                                temp = RXbuffer[8]-'0';
                            else
                                temp = RXbuffer[8]-'A'+10;

                            sleepCycles = temp * 16;

                            if(RXbuffer[9] < 'A')
                                temp = RXbuffer[9]-'0';
                            else
                                temp = RXbuffer[9]-'A'+10;

                            sleepCycles = sleepCycles + temp;
                        }
                    }
                    // Clear the incomming line
                    ReadLine(NULL);
                }
            }
            */
            // Enter Deep Sleep Mode
            printf("O\n");
            //__delay_ms(100);


            // Turn off the Radio
            //printf("Turning RN4020 Off\n\r");
            // WAKE_HW on RA5
            PORTAbits.RA5 = 0;          // Wakeup Hardware
            // WAKE_SW on RA4
            __delay_ms(100);
            PORTAbits.RA4 = 0;          // Wake Software

            // Turn off LED while sleeping
            PORTBbits.RB6 = 0;

            //  Sleep for a total of sleepCycles * 5 seconds
            while(timerCounts < sleepCycles){
                //PORTBbits.RB4 = 0;
                SLEEP();
            }

            timerCounts = 0;

        }
        
    }
    return (EXIT_SUCCESS);
}

void RN4020Config(){

    // Set the name
    printf("SN,GardenSense\n");
    __delay_ms(500);

    // Set the device as a peripherial role, auto advertise
    printf("PZ\n");
    __delay_ms(500);

    // Set the device as a peripherial role, auto advertise
    printf("SR,00000000\n");
    __delay_ms(500);

    // Set the device to proved a privater service in the server role
    //   along with the 'Device Information' and 'Battery' services
    printf("SS,00000001\n");
    __delay_ms(500);

    // Set the UUID of the private service
    printf("PS,");
    printf(UUID_PREFIX);
    printf(UUID_DATA);
    printf("\n");
    __delay_ms(500);

    // reboot
    printf("R,1\n");
    __delay_ms(100);

}

int ReadCap(unsigned char sensorNumber)
{
    //  Sensors | Port
    // ---------+-------
    //    1&2   | RC0
    //    3&4   | RC1
    //    5&6   | RC2
    //    7&8   | RC3

    // There are only 4 sensors: 1,3,5,&7.
    if(sensorNumber < 1)
        return 0;
    if(sensorNumber > 7)
        return 0;

    //This code block configures the ADC
    //for polling, VDD and VSS references, Fosc/16
    //clock and AN0 input.

    //The Hardware CVD1 will perform an inverted
    //double conversion, Guard A and B drive are
    //both enabled.
    //Conversion start & polling for completion are included.


    if(CVD_Init == 0){
        // Setup Pins
        ANSELCbits.ANSC0 = 0;    // Set RC0-3 to digital
        ANSELCbits.ANSC1 = 0;
        ANSELCbits.ANSC2 = 0;
        ANSELCbits.ANSC3 = 0;
        LATCbits.LATC0 = 0;      // RC0-3 output low
        LATCbits.LATC1 = 0;
        LATCbits.LATC2 = 0;
        LATCbits.LATC3 = 0;
        TRISCbits.TRISC0 = 0;    // Set RC0-3 to output
        TRISCbits.TRISC1 = 0;
        TRISCbits.TRISC2 = 0;
        TRISCbits.TRISC3 = 0;
        //WPUAbits.WPUA0 = 0;    //Disable pull-up on RA0
        // NO PULLUPS ON PORT C

        //Initialize ADC and Hardware CVD
        AADCON1 = 0B11010000;     //VDD VREF  FOSC/16 = 8MHz/16 = 2us
        AAD1CH =  0B00000000;     //No secondary channel
        AAD2CH =  0B00000000;     //No secondary channel
        AAD1CON3= 0B01000010;   //Double and inverted
        AAD2CON3= 0B01000010;   //Double and inverted
        AAD1ACQ = 10;            //Acquisition Timer
        AAD2ACQ = 10;            //Acquisition Timer
        AAD1GRD = 0B00000000;    //Turn off Guard Rings
        AAD2GRD = 0B00000000;    //Turn off Guard Rings
        AAD1CAP = 0B00001111;    //30pF additional Capacitance
        AAD2CAP = 0B00001111;    //30pF additional Capacitance

        CVD_Init = 1;
    }

    // Select the sensor number
    if(sensorNumber == 1)
        AAD1CON0 = 0B00110001;   //Select channel AN12 (RC2)
    else if(sensorNumber == 2)
        AAD2CON0 = 0B01011101;   //Select channel AN23 (RC1)
    else if(sensorNumber == 3)
        AAD1CON0 = 0B00110101;   //Select channel AN13 (RC0)
    else if(sensorNumber == 4)
        AAD2CON0 = 0B01010101;   //Select channel AN21 (RC5)
    else if(sensorNumber == 5)
        AAD1CON0 = 0B00101101;   //Select channel AN11 (RC4)
    else if(sensorNumber == 6)
        AAD2CON0 = 0B01011001;   //Select channel AN22 (RC3)
    else if(sensorNumber == 7)
        AAD1CON0 = 0B00111001;   //Select channel AN14 (RC6)

    if(sensorNumber == 1 || sensorNumber == 3 || sensorNumber == 5 || sensorNumber == 7)
    {
        AAD1PRE = 10;            // Pre-charge Timer
        AD1CON0bits.GO = 1;
        // Wait for bit to clear
        while(AD1CON0bits.GO == 1){
        }
        AAD1PRE = 0;             // Disable Pre-charge Timer.  Needed for FVR measuremts to work correctly for some reason
        AAD1CON0 = 0B00110100;   // Turn off the ADC
        return AAD1RES0;

    } else {
        AAD2PRE = 10;            // Pre-charge Timer
        AD2CON0bits.GO = 1;
        // Wait for bit to clear
        while(AD2CON0bits.GO == 1){
        }
        AAD2PRE = 0;             // Disable Pre-charge Timer.  Needed for FVR measuremts to work correctly for some reason
        AAD2CON0 = 0B00110000;   // Turn off the ADC
        return AAD2RES0;

    }

}

void WaitButton()
{
     // Wait for button push
    while(PORTAbits.RA2 == 1){
        NOP();
    }
    __delay_ms(250);
    // Wait for release
    while(PORTAbits.RA2 == 0){
        NOP();
    }
    __delay_ms(250);
}

void ClearRXBuffer()
{
    unsigned char i = 0;

    // Clear the RX message buffer
    for(i=0; i<MAX_RX_BUFFER; i++)
    {
        RXbuffer[i] = NULL;
    }
    RXbufferOverflow = 0;   // Not overflowed
    RXbufferStart = 0;      // Set the start pointer to 0
    RXbufferEnd = 0;        // Set the end pointer to 0
    RXbufferLastChar = 0;   // Set the last character in the buffer to 0
    RXbufferCount = 0;      // Set number of chars in buffer to zero
    RXbufferHasNewline = 0; // The buffer contains no newlines

}

void SetupUSART()
{
    ClearRXBuffer();

    // Set TX (RB7) port to an output
    TRISBbits.TRISB7 = 0;
    // Set RX (RB5) port to an input
    TRISBbits.TRISB5 = 1;
    ANSELBbits.ANSB5 = 0;

    // Set the baud rate
    TXSTAbits.SYNC = 0;   // Asyncronous mode
    TXSTAbits.BRGH = 1;   // High baud rate
    BAUDCONbits.BRG16 = 1;// 16 bit counter
    SPBRG = 0x10;         // Sets the baud rate

    TXSTAbits.TXEN = 1;   // Enable transmit circuitry
    TXIE = 0;             // disable tx interrupts

    RCSTAbits.SPEN = 1;   // Enable serial port pins
    RCSTAbits.CREN = 1;   // Enable reception

    PIR1bits.RCIF = 0;    // Clear any existing rx interrupt flags
    RCIE = 1;             // enable rx interrupts
    PEIE = 1;             // enable peripheral interrupts
    GIE = 1;              // enable global interrupts

    //return RCREG; 

}

void PrintLines(void)
{
    while(RXbufferHasNewline > 0)
    {
        // Print the first message in the buffer
        printf("=> %s\n\r", RXbuffer);

        // Remove one message from the buffer
        ReadLine(NULL);
    }

}

char WaitRXBuffer(char ticks)
{
    // Set a timeout goal to ticks + 1 so we will wait at least ticks time
    unsigned char timerGoal = timerCounts + ticks + 1;

    while(RXbufferHasNewline == 0 && timerCounts < timerGoal)
    {
        NOP();
    }

    if(timerCounts == timerGoal)
        return(0);      // Timed out
    else
        return(1);      // No Timeout
}

char * ReadLine(char * buffer)
{
    unsigned char i, j;

    //printf("\n\rA");
    //PrintRXBuffer();

    if(RXbufferHasNewline > 0)
    {
        // Find the end of string and copy the string into a buffer if it exists
        for(i=0; RXbuffer[RXbufferStart] != NULL;)
        {
            if(buffer != NULL)
                buffer[i] = RXbuffer[RXbufferStart];
            RXbufferStart++;
            if(RXbufferStart == MAX_RX_BUFFER)
                RXbufferStart = 0;
            RXbufferCount--;

            i++;
            if(i == MAX_BUFFER)
                i = MAX_BUFFER - 1;
        }
        // Copy the null at the end
        buffer[i] = RXbuffer[RXbufferStart];
        // Advance RXbufferStart past the null at the end of the string
        RXbufferStart = RXbufferStart + 1;
        if(RXbufferStart == MAX_RX_BUFFER)
            RXbufferStart = 0;
        RXbufferCount--;

        // There's now one message less in the buffer
        RXbufferHasNewline = RXbufferHasNewline - 1;
    }

    // Return the string that was passed in
    return buffer;
}

void SetupClock()
{
    // Set the oscillator for 8 MHz
    OSCCONbits.SPLLEN = 0;
    OSCCONbits.IRCF3 = 1;
    OSCCONbits.IRCF2 = 1;
    OSCCONbits.IRCF1 = 1;
    OSCCONbits.IRCF0 = 0;
}

void SetupSleepTimer()
{
    T1CON = 0xFF;           // Low Frequency Oscillator 31 kHz, Prescaler 1:8
    PIE1bits.TMR1IE = 1;
    INTCONbits.PEIE = 1;
    timerCounts = 0;

    // 3875 * 8 = 31,000 Cycles = 1 second delay
    // Longest Sleep 0xFFFF = 524,244 / 3875 = 16.91 seconds
    // 15 seconds is 0xE30D
    //  5 seconds if 0x4BAF
    // TMR1 = 0xFFFF - E30D;
    TMR1 = 0xFFFF - 0x4BAF;

}

void SetupRN4020()
{
    // WAKE_HW on RA5
    ANSELAbits.ANSA5 = 0;       // Set to digital
    PORTAbits.RA5 = 0;          // Hardware Sleep
    TRISAbits.TRISA5 = 0;       // Set to output
    // WAKE_SW on RA4
    ANSELAbits.ANSA4 = 0;       // Set to digital
    PORTAbits.RA4 = 0;          // Software Sleep
    TRISAbits.TRISA4 = 0;       // Set to output
   
}

// Defines stdout for use with printf class of functions that output to stdout
void putch(char data) {
   while(!TXIF)
     continue;
   TXREG = data;
}

void interrupt SerialRxPinInterrupt()
{
    char c;
    PORTBbits.RB4 = 1;

    if(PIR1bits.TMR1IF == 1){
        // Toggle LED
        //if(PORTBbits.RB4 == 0)
            //PORTBbits.RB4 = 1;
        //else
        //    PORTCbits.RC6 = 0;

        timerCounts = timerCounts + 1;

        // 3875 * 8 = 31,000 Cycles = 1 second delay
        // Longest Sleep 0xFFFF = 524,244 / 31,000 = 16.91 seconds
        //  5 seconds if 0x4BAF
        // 15 seconds is 0xE30D
        TMR1 = 0xFFFF - 0x4BAF;

        //putch('X');
        //putch('\n');
        //putch('\r');
        PIR1bits.TMR1IF = 0;

    }

    // If it's a serial port RX interrupt
    if(PIR1bits.RCIF == 1){
        //putch('^');

        // Read out any availible serial port data
        //   which automatically clears the Interrupt Flag
        while(PIR1bits.RCIF == 1)
        {
            c = RCREG;          // Read the byte from rx register

            // Only populate the buffer if there's space in the buffer
            //   if it's full RXbufferIndex points past the end!
            if(RXbufferCount < MAX_RX_BUFFER)
            {
                if(c != NULL){      // Don't accept NULLs
                    RXbuffer[RXbufferEnd] = c;
                    //putch(RXbuffer[RXbufferIndex]);
                    // Turn end of line, new line, and last buffer characters into a NULL
                    if(RXbuffer[RXbufferEnd]=='\r' || RXbuffer[RXbufferEnd]=='\n'
                            || RXbufferCount == MAX_RX_BUFFER - 1 ){
                        // Turn it into a null so the buffer can be used as a string
                        RXbuffer[RXbufferEnd] = NULL;
                    }
                    // Don't double NULL!
                    // Don't advance the index on a NULL, unless
                    //   we're past the start, and the previous charater isn't null
                    //   Another way of saying this is only advance one past a null
                    if(RXbuffer[RXbufferEnd] != NULL || (RXbufferCount > 0 && RXbuffer[RXbufferLastChar] != NULL)){  // Don't write two NULLs in a row
                        // Mark the last character stored in the buffer
                        RXbufferLastChar = RXbufferEnd;
                        // Advance the count by one
                        RXbufferCount++;
                        // Advance the pointer to the buffer end
                        RXbufferEnd++;
                        if(RXbufferEnd == MAX_RX_BUFFER)
                            RXbufferEnd = 0;
                        // If we just advanced the index and the last character was a null,
                        //    we have an additional string in the buffer.
                        if(RXbuffer[RXbufferLastChar] == NULL)
                            RXbufferHasNewline += 1;
                    }
                }
            } else {
                RXbufferOverflow = 1;
                //printf("OVERFLOW!!\n\r");
            }
        }

        //printf("\n\r");
        //PrintRXBuffer();


    }

    PORTBbits.RB4 = 0;

}

void PrintRXBuffer()
{
    unsigned char i=0;

    putch('#');

    // Print Debug info
    for(i=0; i < MAX_RX_BUFFER; i++){
        printf("%d-%x%c,", i, RXbuffer[i], RXbuffer[i]);
        printcounts++;
    }
    printf("\n\r# s=%d e=%d c=%d", RXbufferStart, RXbufferEnd, RXbufferCount);
    printf(" n=%d", RXbufferHasNewline);
    printf(" pc=%d\n\r", printcounts);
}

void PrintBuffer()
{
    unsigned char i=0;

    putch('#');

    // Print Debug info
    for(i=0; i < MAX_BUFFER; i++){
        printf("%d-%x%c,", i, buffer[i], buffer[i]);
        printcounts++;
    }
    putch('\n');
}