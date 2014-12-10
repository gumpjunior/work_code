
/* Blinker Demo */
/* Include useful pre-defined functions */

#define F_CPU 8000000UL      // MCU Frequency
#include <avr/interrupt.h>   // Defines pins, ports, etc to make programs easier to read
#include <util/delay.h>

int output = 1;

int LED_pin_init(void){
    DDRD |= (output<<4);
    return 0;
}

int LED_on(void){PORTD |= _BV(PD4); return 0;}
int LED_off(void){PORTD &= ~_BV(PD4); return 0;}

int main(void){
    LED_pin_init();
    while(1){
        LED_on();
        _delay_ms(1000);

        LED_off();
        _delay_ms(1000);
    }
}
