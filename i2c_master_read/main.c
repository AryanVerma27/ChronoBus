/*
 * ChronoBus — I2C Communication: PCF8523 RTC (slave) -> MSP432 (master) : READ
 *
 * Board : MSP-EXP432P401R (Code Composer Studio + SimpleLink MSP432 SDK)
 * Slave : Adafruit PCF8523 RTC breakout, 7-bit address 0x68
 *
 * Wiring: P1.6 -> SDA, P1.7 -> SCL, 5V -> VIN, GND -> GND
 * Clock : DCO = 12 MHz, SMCLK = 6 MHz, BRW = 60 -> SCL = 100 kHz
 *
 * The master repeatedly issues a read of TBCNT (4) bytes from the slave.
 * Each received byte is stored in Data_In by the interrupt handler
 * (inspect it in the CCS debugger or on a logic analyzer).
 */

#include "msp.h"
#include <stdint.h>

volatile uint8_t Data_In = 0;

static void clock_init(void)
{
    CS->KEY  = CS_KEY_VAL;
    CS->CTL0 = CS_CTL0_DCORSEL_3;                            // DCO = 12 MHz
    CS->CTL1 = CS_CTL1_SELM_3 | CS_CTL1_SELS_3 | CS_CTL1_DIVS_1; // SMCLK = 6 MHz
    CS->KEY  = 0;
}

void main(void)
{
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;              // stop watchdog
    clock_init();

    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_SWRST;
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_UCSSEL_2;               // clock = SMCLK
    EUSCI_B0->BRW    = 60;                                   // 100 kHz
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_MODE_3;                 // I2C mode
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_MST;                    // master
    EUSCI_B0->CTLW0 &= ~EUSCI_B_CTLW0_TR;                    // receiver
    EUSCI_B0->I2CSA  = 0x68;                                 // PCF8523 address
    EUSCI_B0->CTLW1 |= EUSCI_B_CTLW1_ASTP_2;                 // auto STOP after TBCNT
    EUSCI_B0->TBCNT  = 4;                                    // bytes per read

    P1->SEL0 |=  (BIT6 | BIT7);                              // P1.6 SDA, P1.7 SCL
    P1->SEL1 &= ~(BIT6 | BIT7);

    EUSCI_B0->CTLW0 &= ~EUSCI_B_CTLW0_SWRST;
    EUSCI_B0->IE    |= EUSCI_B_IE_RXIE0;                     // RX interrupt

    __enable_irq();
    NVIC_EnableIRQ(EUSCIB0_IRQn);

    while (1)
    {
        EUSCI_B0->CTLW0 &= ~EUSCI_B_CTLW0_TR;                // receive mode
        EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_TXSTT;              // START + address (read)
        while ((EUSCI_B0->IFG & EUSCI_B_IFG_STPIFG) == 0) { } // wait for STOP
        EUSCI_B0->IFG &= ~EUSCI_B_IFG_STPIFG;
    }
}

void EUSCIB0_IRQHandler(void)
{
    switch (EUSCI_B0->IV)
    {
        case 0x16:                      // UCRXIFG0 - byte received
            Data_In = EUSCI_B0->RXBUF;
            break;
        case 0x18:                      // UCTXIFG0 - transmit buffer empty
            EUSCI_B0->TXBUF = 0x03;     // register address (used if TR is set)
            break;
        default:
            break;
    }
}
