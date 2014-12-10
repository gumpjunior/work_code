
#include <string.h>
#include <avr/interrupt.h>    // Defines pins, ports, etc to make programs easier to read
#include <util/delay.h>
#define uint    unsigned int
#define uchar   unsigned char

#define F_CPU   8000000UL // MCU Frequency
#define BAUD    4800
#define BAUD_NUM    F_CPU/16/BAUD-1
#define TRUE    1

#define SERIAL_BUFFER_SIZE 16
#define INPUT_STRING_SIZE  16
//int num = (int)"a";
//int num = 0b10101010;
int num = 0b01010101;
//unsigned char ipstr[INPUT_STRING_SIZE];
char ipstr[INPUT_STRING_SIZE];
char beginstr[5];      // should equal "0xEE"
char endstr[5];        // should equal "0xFF"
char beginstr_cmp[] = "0xEE";
char endstr_cmp[] = "0xFF";
char sucsfeedback[] = "0xEEsucsfb0xFF\n";
char failfeedback[] = "0xEEfailfb0xFF\n";

int flag_0xEE = 0;
int flag_0xFF = 0;
int output = 1;
int input  = 0;
int cnt = 0;


struct ring_buffer{
    unsigned char buffer[SERIAL_BUFFER_SIZE];
    volatile unsigned int head;
    volatile unsigned int tail;
};

struct ring_buffer rx_buffer  =  { { 0 }, 0, 0 };

int LED_pin_init(void){
    DDRD |= (output<<4);
    return 0;
}



int LED_off(void){PORTD |= _BV(PD4); return 0;}
int LED_on(void){PORTD &= ~_BV(PD4); return 0;}

inline void store_char(unsigned char c, struct ring_buffer *buffer){
    // head will add 1 when one new element pushed in;
    int i = (unsigned int)(buffer->head + 1) % SERIAL_BUFFER_SIZE;

    // if we should be storing the received character into the location
    // just before the tail (meaning that the head would advance to the
    // current location of the tail), we're about to overflow the buffer
    // and so we don't write the character or advance the head.
    if (i != buffer->tail) {
        buffer->buffer[buffer->head] = c;
        buffer->head = i;
    }
}


ISR(USART_RX_vect){
    //while( (UCSR0A & (1<<RXC0)) == 0 ){};
    if ((UCSR0A & (1<<RXC0)) != 0) {
        unsigned char c = UDR0;
        store_char(c, &rx_buffer);
    }
}

int USART_available(void){
    // if buffer is empty, return 0
    return ((unsigned int)(SERIAL_BUFFER_SIZE + rx_buffer.head - rx_buffer.tail)) % SERIAL_BUFFER_SIZE;
}

int USART_read(void){
    // if the head isn't ahead of the tail, we don't have any characters
    if (rx_buffer.head == rx_buffer.tail) {
        return -1;
    } else {
        unsigned char c = rx_buffer.buffer[rx_buffer.tail];
        rx_buffer.tail = (unsigned int)(rx_buffer.tail + 1) % SERIAL_BUFFER_SIZE;
        return c;
    }
}

int USART_write(char wr_byte){
    while( (UCSR0A & (1 << UDRE0)) == 0 ) {};
    UDR0 = wr_byte;
    return 0;
}

int USART_init(unsigned int ubrr){           //By default, no parity, async mode
    UBRR0H = (uchar)(ubrr>>8);               //Set baud rate
    UBRR0L = (uchar)ubrr;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);         //Enable receiver and transmitter
    UCSR0C |= (0<<USBS0);                    //Set frame format: 1stop bit
    UCSR0C |= (1<<UCSZ00)|(1<<UCSZ01);      //Set frame format: 8-bit data
    UCSR0B |= (0<<UCSZ02);
    UCSR0B |= (1 << RXCIE0);                 //enable interrupt
    return 0;
}

int LED_flash(int looptimes, int delayms){            
    int cnt = 0;
    LED_off();
    _delay_ms(500);
    for(cnt=0;cnt<looptimes;cnt+=1){
        LED_on();
        _delay_ms(delayms);
        LED_off();
        _delay_ms(delayms);
    }
    return 0;
}

int readuntil(void){
    // read until newline char from UART
    // append all char received to ipstr
    char rdBuff[] = " ";
    ipstr[0] = '\0';
    do{
        while(USART_available() == 0);
        rdBuff[0] = USART_read();
        if( (rdBuff[0] != '\r') && (rdBuff[0] != '\n') ){
            strcat(ipstr, rdBuff);
            // USART_write(rdBuff[0]);
        }
    }while(rdBuff[0] != '\n');                  // '\r' is ONLY for debug
    
    for(cnt=0;cnt<strlen(ipstr);cnt+=1){      // otherwise will output strlen() times the whole string;
        //USART_write(ipstr[cnt]);
    }
    
    return 0;
}

int checkheadpkg(void){
    // check the chars received beginning with 0xEE
    for(cnt=0;cnt<4;cnt+=1){
        beginstr[cnt] = ipstr[cnt];
        //USART_write(beginstr[cnt]);
    }
    
    if( strncmp(beginstr, beginstr_cmp, 4) == 0 ){
        flag_0xEE = 1;
    }else{
        flag_0xEE = 0;
    }
    return flag_0xEE;
}

int checktailpkg(void){
    // check the chars received ending with 0xFF
    for(cnt=3;cnt>=0;cnt-=1){
        endstr[cnt] = ipstr[(strlen(ipstr)-1)-(3-cnt)];
        //(strlen(&ipstr)-1) is the index of last elem
        //USART_write(endstr[cnt]);
    }
    
    
    if( strncmp(endstr, endstr_cmp, 4) == 0 ){
        flag_0xFF = 1;
    }else{
        flag_0xFF = 0;
    }
    return flag_0xFF;
}

int powerregpin_init(void){
    DDRB |= (output<<0) | (output<<1);
    return 0;
}

int drive0_on(void){PORTB |= _BV(PB0); return 0;}
int drive0_off(void){PORTB &= ~_BV(PB0); return 0;}
int drive1_on(void){PORTB |= _BV(PB1); return 0;}
int drive1_off(void){PORTB &= ~_BV(PB1); return 0;}

int main(void){
    
    LED_pin_init();
    LED_off();
    sei();
    USART_init(BAUD_NUM);
    
    powerregpin_init();
    //drive0_off();
    //drive1_off();
    
    while(TRUE){        
        readuntil();
        checkheadpkg();
        checktailpkg();
        // only both "flag_0xEE" and "flag_0xFF" are true
        if(flag_0xEE && flag_0xFF){
            flag_0xEE = 0;
            flag_0xFF = 0;
            if( strncmp(ipstr, "0xEEpoweron0xFF", 15) == 0){
                drive0_on();
                //drive1_on();
                for(cnt=0;cnt<strlen(sucsfeedback);cnt+=1){
                    USART_write(sucsfeedback[cnt]);
                }
            }
            else if( strncmp(ipstr, "0xEEturnOFF0xFF", 15) == 0 ){
                drive0_off();
                //drive1_off();
                for(cnt=0;cnt<strlen(sucsfeedback);cnt+=1){
                    USART_write(sucsfeedback[cnt]);
                }
            }
            else{
                // receive incorrect cmd
                for(cnt=0;cnt<strlen(failfeedback);cnt+=1){
                    USART_write(failfeedback[cnt]);
                }
            }
        }
    }
    
    
    
    return 0;
    
}