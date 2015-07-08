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

#define _XTAL_FREQ 8000000  // The speed of the internal(or)external oscillator
#define CHOLD 45            // Hold capacitor size in pF for CVD
#define MAX_RX_BUFFER 40   // Maximum size for the RX buffer
// GardenSense Private Service UUID is ACD63933-AE9C-393D-A313-F75B3E5F9A8E
#define UUID "ACD63933AE9C393DA313F75B3E5F9A8E"  // GardenSense Private Service UUID

void putch(char data);
void SetupSleepTimer();
void SetupClock();
void SetupUSART();
void SetupRN4020();
void RN4020Config();
void WaitButton();
void PrintBuffer();

int ReadCap(unsigned char sensorNumber);
char * ReadLine(char * buffer);
void PrintLines(void);

static int CVD_Init = 0;
static char RXbuffer[MAX_RX_BUFFER];
static unsigned char RXbufferIndex;
static unsigned char RXbufferOverflow;
static unsigned char RXbufferHasNewline;

static int printcounts = 0;

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned int cnt = 0;
    unsigned int readADC = 0;
    unsigned char buffer[30];
    float voltageRatio;
    float voltage;
    float capacitance;

    SetupClock();
    SetupUSART();
    SetupSleepTimer();
    SetupRN4020();
    
    // Setup Port C LEDs
    PORTCbits.RC6 = 0;
    TRISCbits.TRISC6 = 0;
    ANSELCbits.ANSC6 = 0;       // Digital input
    PORTCbits.RC7 = 0;
    TRISCbits.TRISC7 = 0;
    ANSELCbits.ANSC7 = 0;       // Digital input
    // Setup Port A2 Switch
    TRISAbits.TRISA2 = 1;
    ANSELAbits.ANSA2 = 0;       // Digital input


    putch('+');

    //Forever While Loop
    while(1){

        PORTCbits.RC6 = 1;
        __delay_ms(50);
        PORTCbits.RC6 = 0;
        __delay_ms(200);

        /*for(cnt = 1; cnt < 9; cnt = cnt +2)
        {
            readADC = ReadCap(cnt);
            voltageRatio = (readADC/1023.0);
            voltage = voltageRatio*3.3;
            capacitance = CHOLD*((1-voltageRatio)/voltageRatio);

            //sprintf(buffer, "S1+2: %d, %1.2fV, %.1fpF  ", readADC, voltage, capacitance);
            printf("S%d: %.1fpF  ", cnt, capacitance);
        }*/

        printf("Wait Button\n\r");

        WaitButton();

        printf("Turning RN4020 On\n\r");
        __delay_ms(500);
        // WAKE_HW on RC5
        PORTCbits.RC5 = 1;          // Wakeup Hardware
        // WAKE_SW on RC4
        __delay_ms(200);
        PORTCbits.RC4 = 1;          // Wake Software
        // CTS on RB6
        PORTBbits.RB6 = 0;          // Clear to send
        // MLDP on RA4 with yellow wire
        PORTAbits.RA4 = 0;          // Set low for CMD mode

        __delay_ms(500);
        //PrintLines();

        printf("+\n");
        __delay_ms(100);

        //RN4020Config();

        //printf("U\n");
        //__delay_ms(100);

        //printf("X\n");
        //__delay_ms(100);

        //printf("Z\n");
        //__delay_ms(100);

        //WaitButton();
        printf("GN\n");
        __delay_ms(100);
        //PrintLines();

        //WaitButton();
        printf("LS\n");
        __delay_ms(100);

        //WaitButton();
        printf("GS\n");
        __delay_ms(100);

        //WaitButton();
        printf("D\n");
        __delay_ms(100);

        //cnt = 0;
        while(1)
        {
            //WaitButton();

            printf("SUW,");
            printf(UUID);
            printf(",");
            readADC = ReadCap(1);
            printf("%04x",readADC);
            readADC = ReadCap(3);
            printf("%04x",readADC);
            readADC = ReadCap(5);
            printf("%04x",readADC);
            readADC = ReadCap(7);
            printf("%04x",readADC);
            printf("\n");
            __delay_ms(1000);

            //printf("SUR,");
            //printf(UUID);
            //printf("\n");

            //cnt++;
        }

        // Wait for response
        while(RXbufferHasNewline == 0)
        {
            NOP();
        }

//        SLEEP();

    }
    
    return (EXIT_SUCCESS);
}

void RN4020Config(){

    WaitButton();
    // Set the device as a peripherial role, auto advertise
    printf("PZ\n");
    __delay_ms(500);

    // Set the device as a peripherial role, auto advertise
    printf("SR,20000000\n");
    __delay_ms(500);

    // Set the device to proved a privater service in the server role
    printf("SS,00000001\n");
    __delay_ms(500);

    // Set the UUID of the private service
    printf("PS,");
    printf(UUID);
    printf("\n");
    __delay_ms(500);

    // Define a private characterisitc having 2 bytes
    printf("PC,");
    printf(UUID);
    printf(",02,08\n");
    __delay_ms(500);

    // reboot
    printf("R,1\n");
    __delay_ms(2000);

    printf("+\n");
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
    if(sensorNumber == 2)
        sensorNumber = 1;
    if(sensorNumber == 4)
        sensorNumber = 3;
    if(sensorNumber == 6)
        sensorNumber = 5;
    if(sensorNumber == 8)
        sensorNumber = 7;
    if(sensorNumber > 8)
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
        AADCON1 = 0B1001000;     //VDD and VSS VREF
        AAD1CH = 0B00000000;     //No secondary channel
        AAD2CH = 0B00000000;     //No secondary channel
        AAD1CON3 = 0B01000011;   //Double and inverted
        AAD2CON3 = 0B01000011;   //Double and inverted
        AAD1PRE = 10;            //Pre-charge Timer
        AAD2PRE = 10;            //Pre-charge Timer
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
        AAD1CON0 = 0B00110101;   //Select channel AN13 (RC0)
    else if(sensorNumber == 3)
        AAD2CON0 = 0B01011101;   //Select channel AN23 (RC1)
    else if(sensorNumber == 5)
        AAD1CON0 = 0B00110001;   //Select channel AN12 (RC2)
    else if(sensorNumber == 7)
        AAD2CON0 = 0B01011001;   //Select channel AN22 (RC3)

    if(sensorNumber == 1 || sensorNumber == 5)
    {
        AD1CON0bits.GO = 1;
        // Wait for bit to clear
        while(AD1CON0bits.GO == 1){
        }
        return AAD1RES0>>6;
        
    } else {
        AD2CON0bits.GO = 1;
        // Wait for bit to clear
        while(AD2CON0bits.GO == 1){
        }
        return AAD2RES0>>6;

    }
}

void WaitButton()
{
     // Wait for button push
    while(PORTAbits.RA2 == 0){
        NOP();
    }
    __delay_ms(250);
    // Wait for release
    while(PORTAbits.RA2 == 1){
        NOP();
    }
    __delay_ms(250);
}

void SetupUSART()
{
    unsigned char i = 0;
    // Clear the RX message buffer
    for(i=0; i<MAX_RX_BUFFER; i++)
    {
        RXbuffer[i] = NULL;
    }
    RXbufferOverflow = 0;   // Not overflowed
    RXbufferIndex = 0;      // Set index to the start
    RXbufferHasNewline = 0; // The buffer contains no newlines

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

char * ReadLine(char * buffer)
{
    unsigned char i, j;

    //printf("\n\rA");
    //PrintBuffer();

    // Turn off RX interrupts
    RCIE = 0;

    // Find the end of string and copy the string into a buffer if it exists
    for(i=0; RXbuffer[i] != NULL && i < MAX_RX_BUFFER; i++)
    {
        //if(buffer != NULL)
        //    buffer[i] = RXbuffer[i];
    }
    // Advance i past the null at the end of the string
    i = i + 1;

    // If there is more than one item in the buffer
    if(RXbufferIndex > i )
        RXbufferIndex = RXbufferIndex - i;
    else
        RXbufferIndex = 0;

    //printf("\n\rB");
    //PrintBuffer();

    // Erase the string by copying the buffer over.
    for(j=i; j < MAX_RX_BUFFER; j++){
        RXbuffer[j-i] = RXbuffer[j];
    }

    //printf("\n\rC");
    //PrintBuffer();

    // Fill in the end of the buffer with NULLs
    for(j=j-i+1; j < MAX_RX_BUFFER; j++){
        RXbuffer[j] = NULL;
    }

    // There's now one message less in the buffer
    RXbufferHasNewline = RXbufferHasNewline - 1;

    //printf("\n\rD");
    //PrintBuffer();

    // Turn on RX interrupts
    RCIE = 1;

    // Return the string that was passed in
    return buffer;
}

void SetupClock()
{
    // Set the oscillator for 500 kHz
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

    // 3875 * 8 = 31,000 Cycles = 1 second delay
    // Longest Sleep 0xFFFF = 524,244 / 31,000 = 16.91 seconds
    TMR1 = 0xFFFF - 3875;

}

void SetupRN4020()
{
    // WAKE_HW on RC5
    PORTCbits.RC5 = 0;          // Hardware Sleep
    TRISCbits.TRISC5 = 0;       // Set to output
    ANSELCbits.ANSC5 = 0;       // Set to digital
    // WAKE_SW on RC4
    PORTCbits.RC4 = 0;          // Software Sleep
    TRISCbits.TRISC4 = 0;       // Set to output
    ANSELCbits.ANSC4 = 0;       // Set to digital
    // CTS on RB6
    PORTBbits.RB6 = 0;          // Clear to send 0 to save power
    TRISBbits.TRISB6 = 0;       // Set to output
    ANSELBbits.ANSB6 = 0;       // Set to digital
    // RTS on RB4
    TRISBbits.TRISB4 = 1;       // Set to input
    ANSELBbits.ANSB4 = 0;       // Digital input
    // MLDP on RA4 with yellow wire
    TRISAbits.TRISA4 = 0;       // Set to output
    PORTAbits.RA4 = 0;          // Set low to save power
    ANSELAbits.ANSA4 = 0;       // Set to digital
   
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

    if(PIR1bits.TMR1IF == 1){
        // 3875 * 8 = 31,000 Cycles = 1 second delay
        // Longest Sleep 0xFFFF = 524,244 / 31,000 = 16.91 seconds
        TMR1 = 0xFFFF - 3875;

        //putch('X');
        //putch('\n');
        //putch('\r');
        PIR1bits.TMR1IF = 0;
    }

    // If it's a serial port RX interrupt
    if(PIR1bits.RCIF == 1){
        //putch('-');
        PORTCbits.RC7 = 0;

        // Read out any availible serial port data
        //   which automatically clears the Interrupt Flag
        while(PIR1bits.RCIF == 1)
        {
            PORTCbits.RC7 = 1;
            c = RCREG;          // Read the byte from rx register
            if(c != NULL){      // Don't accept NULLs
                RXbuffer[RXbufferIndex] = c;
                //putch(RXbuffer[RXbufferIndex]);
                if(RXbuffer[RXbufferIndex]=='\r' || RXbuffer[RXbufferIndex]=='\n'){
                    // Turn it into a null so the buffer can be used as a string
                    RXbuffer[RXbufferIndex]=0;
                    RXbufferHasNewline += 1;
                }
                // Don't advance the index on a NULL, unless 
                //   we're past the start, and the previous charater isn't null
                if(RXbuffer[RXbufferIndex] != NULL || (RXbufferIndex > 0 && RXbuffer[RXbufferIndex-1] != NULL)){  // Don't write two NULLs in a row
                    if(RXbufferIndex < MAX_RX_BUFFER - 1)      // Only advance index to end
                        RXbufferIndex++;
                    else
                        RXbufferOverflow = 1;
                }
            }
        }

        //printf("\n\r");
        //PrintBuffer();

    }

}

void PrintBuffer()
{
    unsigned char i=0;

    // Print Debug info
    for(i=0; i < MAX_RX_BUFFER; i++){
        printf("%d-%x%c,",i ,RXbuffer[i], RXbuffer[i]);
        printcounts++;
    }
    printf("\n\ri%d", RXbufferIndex);
    printf(" n%d ", RXbufferHasNewline);
    printf(" pc%d ", printcounts);
}