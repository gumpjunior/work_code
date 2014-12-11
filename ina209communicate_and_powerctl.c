/*******************************************************************************
 * Read me:
 *      Receive the command from the PC side via UART until a '\n' is received.
 *      Execute the corresponding command, and send the data/string/feedback 
 *      back to the PC side via the UART with '\n' as the last char.
 *      Command format: 0xEEcommand0xFF
 *      The definition of each command string please refer the python code for 
 *      PC side.
 ******************************************************************************/

#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#define uint    unsigned int
#define uchar   unsigned char

#define F_CPU   8000000UL    // MCU crystal Frequency
#define BAUD    4800
#define BAUD_NUM    F_CPU/16/BAUD-1
#define TRUE    1

#define SERIAL_BUFFER_SIZE 16
#define INPUT_STRING_SIZE  32

char ipstr[INPUT_STRING_SIZE];
char beginstr[5];      // should equal "0xEE"
char endstr[5];        // should equal "0xFF"
char beginstr_cmp[] = "0xEE";
char endstr_cmp[]   = "0xFF";
char sucsfeedback[] = "0xEEsucsfb0xFFfrommicro\n";
char failfeedback[] = "0xEEfailfb0xFFfrommicro\n";

int flag_0xEE = 0;
int flag_0xFF = 0;
int output    = 1;
int input     = 0;
int cnt       = 0;

/*******************************************************************************
 * Definitions below are for TWI
 ******************************************************************************/

// master transmitter mode (scmt) status code, from m328p spec
#define sctmSTART           0x08
#define sctmREPEATSTART     0x10
#define sctmSLAWACK         0x18    // SLA+W transmitted, ACK received
#define sctmSLAWNOACK       0x20    // SLA+W transmitted, NO ACK received
#define sctmDATAACK         0x28    // send data, ACK received
#define sctmDATANOACK       0x30    // send data, NO ACK received

// master receiver mode(scmr)  status code, from m328p spec
#define scrmSTART           0x08
#define scrmREPEATSTART     0x10
#define scrmSLARACK         0x40    // SLAR transmitted, ACK received
#define scrmSLARNOACK       0x48    // SLAR transmitted, NO ACK received
#define scrmDATAACK         0x50    // data received, ACK returned
#define scrmDATANOACK       0x58    // data received, NO ACK returned

// pointer address of ina209 registers, from INA209 spec
#define CONFIGREG           0x00
#define STATUSREG           0x01
#define SHUNTVOLT           0x03
#define BUSVOLT             0x04
#define POWER               0x05
#define CURRENT             0x06
#define SHUNTVOLTPOSPEAK    0x07
#define BUSVOLTMAXPEAK      0X09
#define POWERPEAK           0x0B
#define CALIBRATION         0x16

#define TWIREAD             0x01
#define TWIWRITE            0x00

/*******************************************************************************
 * Definitions below are for USART and LED control
 ******************************************************************************/

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

void LED_flash(int times, int internalt){

    for(cnt=0;cnt<times;cnt+=1){
        LED_on();
        _delay_ms(internalt);
        LED_off();
        _delay_ms(internalt);
    }

}

inline void storeChar(unsigned char c, struct ring_buffer *buffer){

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
        storeChar(c, &rx_buffer);
    }

}

int USART_available(void){

    // if buffer is empty, return 0
    return ((unsigned int)(SERIAL_BUFFER_SIZE + rx_buffer.head - 
                rx_buffer.tail)) % SERIAL_BUFFER_SIZE;

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

int USART_init(unsigned int ubrr){       //By default, no parity, async mode

    UBRR0H = (uchar)(ubrr>>8);           //Set baud rate
    UBRR0L = (uchar)ubrr;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);      //Enable receiver and transmitter
    UCSR0C |= (0<<USBS0);                //Set frame format: 1stop bit
    UCSR0C |= (1<<UCSZ00)|(1<<UCSZ01);   //Set frame format: 8-bit data
    UCSR0B |= (0<<UCSZ02);
    UCSR0B |= (1 << RXCIE0);             //enable interrupt

    return 0;

}

int USART_readUntil(void){

    // read until newline char from UART
    // append all char received to ipstr
    char rdBuff[] = " ";
    // ipstr[0] = '\0';
    memset(ipstr, '\0', sizeof(ipstr));
    do{
        while(USART_available() == 0);
        rdBuff[0] = USART_read();
        if( (rdBuff[0] != '\r') && (rdBuff[0] != '\n') ){
            strcat(ipstr, rdBuff);
            // following lines are ONLY for test
            // USART_write(rdBuff[0]);
            // above lines are ONLY for test
        }
    }while(rdBuff[0] != '\n');
    
    for(cnt=0;cnt<strlen(ipstr);cnt+=1){
        // following lines are ONLY for test
        // USART_write(ipstr[cnt]);
        // above lines are ONLY for test
    }
    
    return 0;

}

int checkHeadPkg(void){

    // check the chars received beginning with 0xEE
    for(cnt=0;cnt<4;cnt+=1){
        beginstr[cnt] = ipstr[cnt];
        // following lines are ONLY for test
        // USART_write(beginstr[cnt]);
        // above lines are ONLY for test
    }
    
    if( strncmp(beginstr, beginstr_cmp, 4) == 0 ){
        flag_0xEE = 1;
    }else{
        flag_0xEE = 0;
    }

    return flag_0xEE;

}

int checkTailPkg(void){

    // check the chars received ending with 0xFF
    for(cnt=3;cnt>=0;cnt-=1){
        endstr[cnt] = ipstr[(strlen(ipstr)-1)-(3-cnt)];
        //(strlen(&ipstr)-1) is the index of last elem
        // USART_write(endstr[cnt]);
    }
    
    
    if( strncmp(endstr, endstr_cmp, 4) == 0 ){
        flag_0xFF = 1;
    }else{
        flag_0xFF = 0;
    }

    return flag_0xFF;

}

int powerRegPinInit(void){

    DDRB |= (output<<0) | (output<<1);

    return 0;

}

int drive0_on(void){PORTB |= _BV(PB0); return 0;}
int drive0_off(void){PORTB &= ~_BV(PB0); return 0;}
int drive1_on(void){PORTB |= _BV(PB1); return 0;}
int drive1_off(void){PORTB &= ~_BV(PB1); return 0;}

/*******************************************************************************
 * Definitions below are for TWI
 ******************************************************************************/

/***************************************************
 * Initialize the TWI module 
 ***************************************************/

void TWI_init(void){
    // set bit rate(SCL) to 200 KHz, fast mode
    TWBR = 12;
    TWSR &= ~(3);
    // enable TWEA bit
    TWCR |= (1<<TWEA);
}

/***************************************************
 * Use USART port to send the string to PC side
 ***************************************************/

void USART_sendStr(char *str){

    for(cnt=0;cnt<strlen(str);cnt+=1){
        USART_write(str[cnt]);
    }

}

/***************************************************
 * This function applies to both uint8_t and uint16_t
 * as the parameter for "data".
 * NOTE:
 *      this func heavily depends upon the 
 *      communication protocol. Sometimes '\n' is 
 *      required at the end of the communication 
 *      string. But sometimes, it varies.
 *      May be some other things need to be considered
 ***************************************************/

void USART_sendNuminBin(uint16_t data, int bytenum){

    int sign = 0;
    for(cnt=8*bytenum-1;cnt>=0;cnt-=1){
        sign = ( data & (0x01<<cnt) )?1:0;
        if(sign){
            USART_write('1');
        }
        else{
            USART_write('0');            
        }
    }

}

/***************************************************
 * Read one byte from INA209
 * Read from TWDR register is after while loop.
 ***************************************************/

uint8_t TWI_recvOneByte(void){

    uint8_t temp = 0;
    uint8_t regtwcr;

    regtwcr = TWCR;
    // Set TWINT and TWEN bits high:
    regtwcr |= (1<<TWINT) | (1<<TWEN);
    // Set TWSTA and TWST0 bits low:
    regtwcr &= ~((1<<TWSTA) | (1<<TWSTO));
    TWCR = regtwcr;
    // Wait for TWINT bit to read high:
    while( (TWCR & (1<<TWINT)) == 0 );
    // Read from TWDR(after while loop):
    temp = TWDR;

    return temp;

}

/***************************************************
 * Write one byte to INA209
 * Write to TWDR register is before TWCR update.
 ***************************************************/

void TWI_sendOneByte(uint8_t senddata){

    uint8_t regtwcr;

    //Write to TWDR(before TWCR update):
    TWDR = senddata;
    regtwcr = TWCR;
    // Set TWINT and TWEN bits high:
    regtwcr |= (1<<TWINT) | (1<<TWEN);
    // Set TWSTA and TWST0 bits low:
    regtwcr &= ~((1<<TWSTA) | (1<<TWSTO));
    TWCR = regtwcr;
    // Wait for TWINT bit to read high:
    while( (TWCR & (1<<TWINT)) == 0 );

}

/***************************************************
 * issue START condition
 * works for both master transmitter and master 
 * receiver mode
 ***************************************************/

void TWI_start(void){

    //comments show how to use USART_sendStr()
    //and USART_sendNuminBin()
    //char str[] = "_sta";
    //uint8_t statusreg;
    uint8_t regtwcr;

    regtwcr = TWCR;
    regtwcr |= (1<<TWINT) | (1<<TWEN) | (1<<TWSTA);
    regtwcr &= ~(1<<TWSTO);
    TWCR = regtwcr;

    while( (TWCR & (1<<TWINT)) == 0 );
    //statusreg = TWSR;

    //USART_sendStr(str);
    //USART_sendNuminBin(TWSR, sizeof(TWSR));

}

/***************************************************
 * issue STOP condition
 * works for both master transmitter and master 
 * receiver mode
 ***************************************************/

void TWI_stop(void){

    uint8_t regtwcr;

    regtwcr = TWCR;
    regtwcr |= (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    regtwcr &= ~(1<<TWSTA);
    TWCR = regtwcr;

}

/***************************************************
 * slaveaddr is 7-bit width
 * R_W is high for master read mode
 * R_W is low  for master transmite mode
 ***************************************************/

void TWI_slaR_W(uint8_t slaveaddr, int r_w){

    slaveaddr = (slaveaddr<<1) | r_w;
    TWI_sendOneByte(slaveaddr);

}

/***************************************************
 * write new value to the register pointer is 
 * accomplished by issuing a slave address byte 
 * with the R/W bit LOW, followed by the register 
 * pointer byte. No additional data are required. 
 * parameters:
 *      slaveaddr: which device
 *      regaddr:   register pointer address
 * SLAW means Master Transmitter mode
 ***************************************************/

void INA209_updateRegPtr(uint8_t slaveaddr, uint8_t regaddr){

    TWI_start();
    TWI_slaR_W(slaveaddr, TWIWRITE);
    TWI_sendOneByte(regaddr);
    TWI_stop();

}

/***************************************************
 * SLAR means Master Receiver mode 
 ***************************************************/

uint16_t INA209_read(uint8_t slaveaddr){
    
    uint16_t datareadout = 0;
    uint8_t temp = 0;
    TWI_start();
    TWI_slaR_W(slaveaddr, TWIREAD);

    // read first 8-bit
    temp = TWI_recvOneByte();
    datareadout = (uint16_t)temp;
    datareadout = datareadout << 8;
    // read second 8-bit
    temp = TWI_recvOneByte();
    datareadout |= temp;
    TWI_stop();

    return datareadout;
}

/***************************************************
 * SLAW means Master Transmitter mode 
 ***************************************************/

void INA209_write(uint8_t slaveaddr, uint8_t regaddr, uint16_t data){
    
    TWI_start();
    // send slave address out
    TWI_slaR_W(slaveaddr, TWIWRITE);

    // update register pointer
    TWI_sendOneByte(regaddr);
    // send high byte
    TWI_sendOneByte((data & 0xff00)>>8);
    // send low byte
    TWI_sendOneByte((data & 0xff));

    TWI_stop();

}

/*******************************************************************************
 * Main Function
 ******************************************************************************/

int main(void){
    uint16_t datareadout;
    
    LED_pin_init();
    LED_off();
    sei();
    USART_init(BAUD_NUM);
    TWI_init();

    powerRegPinInit();
    drive0_on();
    drive1_off();
    
    while(TRUE){        
        USART_readUntil();
        checkHeadPkg();
        checkTailPkg();
        // Only both "flag_0xEE" and "flag_0xFF" are true,
        // means the command received is correct.
        if(flag_0xEE && flag_0xFF){
            flag_0xEE = 0;
            flag_0xFF = 0;
            // refer python code for the meaning of command string
            if( strncmp(ipstr, "0xEEpoweron0xFF", 15) == 0){    
                drive0_on();
                //drive1_on();
                USART_sendStr("power regulator is on.");
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEturnOFF0xFF", 15) == 0 ){
                drive0_off();
                //drive1_off();
                USART_sendStr("power regulator is off.");
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEreadconfig0xFF", 17) == 0 ){
                //comments show debug string
                //LED_flash(10, 200);
                INA209_updateRegPtr(0x40, CONFIGREG);
                datareadout = INA209_read(0x40);
                /*USART_write('_');    
                USART_write('v');
                USART_write('a');
                USART_write('l');*/
                USART_sendNuminBin(datareadout, sizeof(datareadout));
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEreadcalib0xFF", 17) == 0 ){
                INA209_updateRegPtr(0x40, CALIBRATION);
                datareadout = INA209_read(0x40);
                USART_sendNuminBin(datareadout, sizeof(datareadout));
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEshuntvolt0xFF", 17) == 0){
                INA209_updateRegPtr(0x40, SHUNTVOLT);
                datareadout = INA209_read(0x40);
                USART_sendNuminBin(datareadout, sizeof(datareadout));
                USART_write('\n');                
            }
            else if( strncmp(ipstr, "0xEEbusvolt0xFF", 15) == 0){
                INA209_updateRegPtr(0x40, BUSVOLT);
                datareadout = INA209_read(0x40);
                USART_sendNuminBin(datareadout, sizeof(datareadout));
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEpower0xFF", 13) == 0){
                INA209_updateRegPtr(0x40, POWER);
                datareadout = INA209_read(0x40);
                USART_sendNuminBin(datareadout, sizeof(datareadout));
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEcurrent0xFF", 15) == 0){
                INA209_updateRegPtr(0x40, CURRENT);
                datareadout = INA209_read(0x40);
                USART_sendNuminBin(datareadout, sizeof(datareadout));
                USART_write('\n');
            } 
            else if( strncmp(ipstr, "0xEEsetconfig0xFF", 17) == 0 ){
                // +/-80mv, 0x299F requires manually modification
                INA209_write(0x40, CONFIGREG, 0x299f);
                USART_sendStr("configuration register update is done.");
                USART_write('\n');
            }
            else if( strncmp(ipstr, "0xEEsetcalib0xFF", 16) == 0 ){
                // current LSB = 80uA, power LSB = 1.6mw
                // 0x4A12 requires manually modification
                INA209_write(0x40, CALIBRATION, 0x4A12);
                USART_sendStr("calibration register update is done.");
                USART_write('\n');                
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

