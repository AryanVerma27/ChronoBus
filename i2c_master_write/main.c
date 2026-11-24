/*
 * ChronoBus — I2C Communication: MSP432 (master) -> PCF8523 RTC (slave) : WRITE
 *
 * Board : MSP-EXP432P401R (Code Composer Studio + SimpleLink MSP432 SDK)
 * Slave : Adafruit PCF8523 RTC breakout, 7-bit address 0x68
 *
 * Wiring: P1.6 -> SDA, P1.7 -> SCL, 5V -> VIN, GND -> GND
 *         (pull-up resistors are on the PCF8523 breakout)
 *
 * Clock : DCO = 12 MHz, SMCLK = DCO/2 = 6 MHz, BRW = 60 -> SCL = 100 kHz
 *
 * Repeatedly sends the 3-byte packet {0x03, 0xAA, 0xBB} (register address 0x03
 * followed by two data bytes). Auto-STOP is generated after TBCNT bytes.
 */

#include "msp.h"
#include <stdint.h>

static const uint8_t Packet[] = {0x03, 0xAA, 0xBB};
static volatile int  Data_Cnt = 0;

static void clock_init(void)
{
    CS->KEY  = CS_KEY_VAL;                                   // unlock CS
    CS->CTL0 = CS_CTL0_DCORSEL_3;                            // DCO = 12 MHz
    CS->CTL1 = CS_CTL1_SELM_3 | CS_CTL1_SELS_3 | CS_CTL1_DIVS_1; // MCLK = DCO, SMCLK = DCO/2
    CS->KEY  = 0;                                            // lock CS
}

void main(void)
{
    int i;

    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;              // stop watchdog
    clock_init();

    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_SWRST;                  // hold eUSCI_B0 in reset
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_UCSSEL_2;               // clock = SMCLK
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_MODE_3;                 // I2C mode
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_MST;                    // master
    EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_TR;                     // transmitter
    EUSCI_B0->BRW    = 60;                                   // 6 MHz / 60 = 100 kHz
    EUSCI_B0->I2CSA  = 0x68;                                 // PCF8523 address
    EUSCI_B0->CTLW1 |= EUSCI_B_CTLW1_ASTP_2;                 // auto STOP after TBCNT
    EUSCI_B0->TBCNT  = sizeof(Packet);                       // 3 bytes per transfer

    P1->SEL0 |=  (BIT6 | BIT7);                              // P1.6 SDA, P1.7 SCL
    P1->SEL1 &= ~(BIT6 | BIT7);

    EUSCI_B0->CTLW0 &= ~EUSCI_B_CTLW0_SWRST;                 // release reset
    EUSCI_B0->IE    |= EUSCI_B_IE_TXIE0;                     // TX buffer empty interrupt

    NVIC_EnableIRQ(EUSCIB0_IRQn);
    __enable_irq();

    while (1)
    {
        EUSCI_B0->CTLW0 |= EUSCI_B_CTLW0_TXSTT;              // START + address (write)
        for (i = 0; i < 300; i++) { }                        // gap between transfers
    }
}

void EUSCIB0_IRQHandler(void)
{
    EUSCI_B0->TXBUF = Packet[Data_Cnt];                      // load next byte
    Data_Cnt++;
    if (Data_Cnt >= (int)sizeof(Packet))
        Data_Cnt = 0;
}
