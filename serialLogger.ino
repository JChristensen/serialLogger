#include <Button.h>                 //http://github.com/JChristensen/Button
#include <SPI.h>                    //http://arduino.cc/en/Reference/SPI
#include <Streaming.h>              //http://arduiniana.org/libraries/streaming/
#include "buffer.h"
#include "heartbeat.h"

//set the incoming baud rate here
const uint32_t USART_BAUDRATE = 57600;

//pin assignments
const uint8_t HB_LED = 17;           //heartbeat LED
const uint8_t SD_LED = 16;           //SD activity LED
const uint8_t OVR_LED = 18;          //buffer overrun LED
const uint8_t BUTTON_PIN = 20;       //start/stop button

const int NBUF = 2;                  //number of receive buffers
buffer buf[NBUF];                    //the buffers
heartbeat hbLED(HB_LED);

const bool PULLUP = true;
const bool INVERT = true;
const unsigned long DEBOUNCE_MS = 25;
Button btn(BUTTON_PIN, PULLUP, INVERT, DEBOUNCE_MS);

void setup(void)
{
    //inits
    hbLED.begin(BLINK_IDLE);
    buf[0].assignLEDs(SD_LED, OVR_LED);
    
    //set up USART1
    const uint16_t UBRR_VALUE = ( (float)F_CPU / (float)(USART_BAUDRATE) / 16.0 + 0.5 ) - 1;
    UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);    //8-bit chars, async, 1 stop bit, no parity
    UBRR1 = UBRR_VALUE;                    //baud rate setting

//TEST ONLY -- set up USART0
    const uint32_t USART0_BAUDRATE = 115200;
    const uint16_t UBRR0_VALUE = ( (float)F_CPU / (float)(USART0_BAUDRATE) / 16.0 + 0.5 ) - 1;
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);      //enable receive and transmit
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);    //8-bit chars, async, 1 stop bit, no parity
    UBRR0 = UBRR0_VALUE;                    //baud rate setting
}

enum STATES_t { IDLE, LOGGING, STOP } STATE;

void loop(void)
{
    static uint8_t bufIdx;            //index to the current buffer
    buffer* bp;                       //buffer pointer
    
    btn.read();
    hbLED.run();

    switch (STATE)
    {
        
    case IDLE:
        if (btn.wasReleased()) {
            STATE = LOGGING;
            hbLED.mode(BLINK_RUN);
            writeSerial0((uint8_t*)"\r\nStart\r\n", 9);
            for (int i = 0; i < NBUF; i++) {
                buf[i].init();                    //clear the buffers
            }
            UCSR1B = _BV(RXCIE1) | _BV(RXEN1);    //enable rx & rx complete interrupt
        }
        break;
    
    case LOGGING:
        if (btn.wasReleased()) {                  //user wants to stop
            UCSR1B = 0;                           //disable usart rx & rx complete interrupt
            STATE = STOP;
        }
        else {                                    //watch for buffers that need to be written
            bp = &buf[bufIdx];                    //point to buffer
            bp -> write();                        //write it if it needs it
            if ( ++bufIdx >= NBUF ) bufIdx = 0;   //increment index to next buffer
        }
        break;
        
    case STOP:                                    //stop logging, flush buffers, etc.
        STATE = IDLE;
        bp -> ovrFlag = false;
        hbLED.mode(BLINK_IDLE);
        for (int i = 0; i < NBUF; i++) {
            bp = &buf[bufIdx];                    //point to buffer
            bp -> write(true);                    //flush any characters in the buffer
            if ( ++bufIdx >= NBUF ) bufIdx = 0;   //increment index to next buffer
        }
        writeSerial0((uint8_t*)"\r\nStop\r\n", 8);
        break;
        
    }
}

//handle the incoming characters
ISR(USART1_RX_vect)
{
    static bool overrun;
    static uint8_t bufIdx;                         //index to the current buffer
    buffer* bp = &buf[bufIdx];                     //pointer to current buffer
    uint8_t c = UDR1;                              //get the received character
    
    if ( !overrun ) {
        *(bp -> p++) = c;                          //put the character into the buffer
        if ( ++(bp -> nchar) >= BUFSIZE ) {        //buffer full?
            bp -> writeFlag = true;                //yes, set writeFlag for mainline code
            if ( ++bufIdx >= NBUF ) bufIdx = 0;    //increment index to next buffer
            bp = &buf[bufIdx];                     //point to next buffer
            bp -> p = bp -> buf;                   //initialize the character pointer
            if ( bp -> nchar != 0 ) {              //if mainline code has not zeroed the character count,
                overrun = true;                    //then we have an overrun situation
                bp -> ovrFlag = true;
            }
        }
    }
    else if ( bp -> nchar == 0 ) {                 //has previous overrun cleared?
        overrun = false;
        *(bp -> p++) = c;                          //put the character into the buffer
    }
}

