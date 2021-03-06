/*
 * File:   relayonpress.c
 * Author: Christian Roring
 *
 * Created on 24/01/2018
 * 
 * Based on code by: Coda Effects (Benoit M)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <xc.h>
#include "header.h"

volatile uint32_t tick_count; // Make it available for use in interrupt

void InitTimer0(void) {
    // Timer0 is a 8bit timer, select T0CS and PSA to be one
    OPTION_REG &= 0x83; // Make Prescaler 1:16 (1 MIPS: 256 (8bit) * 16) +/- 0.004 sec

    T0IE = 1; // Enable Timer0 interrupt
    GIE = 1; // Enable global interrupts
}

// Interrupt function
void interrupt ISR(void) {
    if (T0IF) // If Timer0 interrupt
    {
        ++tick_count; // Add 1 to the tick count (+/- 1024 usec))
        T0IF = 0; // Clear the interrupt flag
    }
}

// Initialization
void main(void) {
    ANSEL = 0; // no analog GPIOs
    CMCON = 0x07; // comparator off
    ADCON0 = 0; // Analog to Digital and Digital to Analog convertors off
    TRISIO0 = 0; // output LED -> GP0
    TRISIO1 = 1; // input footswtich -> GP1
    TRISIO2 = 0; // output TGP222A -> GP2
    TRISIO5 = 0; // output activation relay -> GP3
    TRISIO4 = 0; // output ground relay -> GP4
    TRISIO3 = 1; // input temporary switch -> GP5

    GPIO = 0; // set outputs as low level (0V)

    // Variables definition
    tick_count = 0; // tick count for the interrupt timer

    uint8_t state = 0; // on-off state of the pedal (1 = on, 0 = off)
    uint8_t button_state = 1; // Debounce state for the button
    uint8_t hold_mode = 0; // State for the hold mode indicator
    
    uint8_t change_state; // to change status of the pedal
    if (eeprom_read(0) == 0xFF) { // Read from 0 address location, 0x00 = off, oxFF = on
        change_state = 1; // If previous state was on
    } else {
        change_state = 0; // If previous state was off
    }

    InitTimer0(); // Setup the interrupt timer0

    // Main loop
    while (1) {
        // If the switch is pressed
        if (GP1 == 0) {
            __delay_ms(10); // Debouncing
            if (GP1 == 0) {
                if (GP1 != button_state) {
                    button_state = GP1;
                    change_state = 1;

                    if (state == 0) {
                        tick_count = 0; // Reset the tick counter to start counting for hold effect if the pedal will become 'on' in the next state change
                    }
                } else {
                    // This is where experimentation comes into play
                    // 1000 is a number chosen on excellent guestimation practices, developed for many years by yours truly :)
                    // The number (hopefully) relates to the number of milliseconds/4  of the interrupt timer, the time elapsed after the switch war pressed
                    if (state == 1 && hold_mode == 0 && tick_count > 250) {
                        hold_mode = 1;
                    }
                }
            }  
        }
        
        if(GP1 == 1) {
            __delay_ms(10); // Debouncing
            if(GP1 == 1 && GP1 != button_state) {
                button_state = GP1;

                if (GP1 == 1 && hold_mode == 1) {
                    change_state = 1;
                    hold_mode = 0;
                }
            }
        }
        
        // Changing state of the pedal
        if (change_state == 1) {
            if (state == 0) { 
                // change to on
                GP2 = 1; // photoFET on
                __delay_ms(20);
                GP0 = 1; // LED on
                GP5 = 1; // relay on
                GP4 = 0;
                __delay_ms(20);
                GP2 = 0; // photoFET off
               
                // Write the state to EEPROM.
                eeprom_write(0, 0xFF); // Write 0xFF at 0 address location
                
                // Set state to on
                state = 1;
            } else { 
                // change to off
                GP2 = 1; // photoFET on
                __delay_ms(20);
                GP0 = 0; // LED off
                GP5 = 0; // relay off
                GP4 = 0;
                __delay_ms(20);
                GP2 = 0; // photoFET off
               
                // Write the state to EEPROM.
                eeprom_write(0, 0x00); // Write 0x00 at 0 address location
                
                // Set state to off
                state = 0;
            }
            change_state = 0; // reset change_state
        }
    }
}