/*-----------------------------------------------------------------------------*
 * Serial Data Logger for Arduino Uno and Adafruit MicroSD card breakout       *
 * board (product #254).                                                       *
 *                                                                             *
 * A logger that writes all serial data input on digital pin 0 (RXD) to        *
 * a micro SD card. Serial input is interrupt-driven and double-buffered       *
 * for maximum throughput.                                                     *
 *                                                                             *
 * Input baud rate is selected by grounding pins A5:A2 according to the        *
 * codes below. Leaving a pin open is a one (high), grounding a pin is         *
 * a zero (low). E.g. for 115200 ground A5 and A3, for 57600 ground A5 and     *
 * A2, for 9600 ground A5, A4, A2.                                             *
 *                                                                             *
 * The heartbeat LED blinks in various patterns:                               *
 *   Short blink: Idle mode.                                                   *
 *   Steady blink on/off: Logging.                                             *
 *   Fast blink on/off: Error.                                                 *
 *   Slow on/off: No SD card inserted (Reset the MCU after inserting card).    *
 *                                                                             *
 * Press the button to start or stop logging. Each time logging is started,    *
 * a new file is created.                                                      *
 *                                                                             *
 * If a buffer overrun occurs, the overrun LED stays on until logging is       *
 * stopped or the MCU is reset.                                                *
 *                                                                             *
 * Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0,      *
 * http://creativecommons.org/licenses/by-sa/4.0/                              *
 *-----------------------------------------------------------------------------*/

#include <Button.h>                 //http://github.com/JChristensen/Button
#include <SdFat.h>                  //http://code.google.com/p/sdfatlib/
#include <SPI.h>                    //http://arduino.cc/en/Reference/SPI
#include "buffer.h"
#include "heartbeat.h"

//BAUD RATE CODES (A5:A2)      0    1     2     3     4     5      6      7      8      9      10
const uint32_t baudRates[] = { 300, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200 };

//pin assignments
const uint8_t SD_LED = 5;            //SD activity LED
const uint8_t HB_LED = 6;            //heartbeat LED
const uint8_t OVR_LED = 7;           //buffer overrun LED
const uint8_t BUTTON_PIN = 8;        //start/stop button
const uint8_t CARD_DETECT = 9;
const uint8_t BAUD_RATE_0 = A2;
const uint8_t BAUD_RATE_1 = A3;
const uint8_t BAUD_RATE_2 = A4;
const uint8_t BAUD_RATE_3 = A5;

const int NBUF = 2;                  //number of receive buffers
buffer buf[NBUF];                    //the buffers
heartbeat hbLED(HB_LED);

const bool PULLUP = true;
const bool INVERT = true;
const unsigned long DEBOUNCE_MS = 25;
Button btn(BUTTON_PIN, PULLUP, INVERT, DEBOUNCE_MS);

SdFat sd;
SdFile logFile;

enum STATES_t { IDLE, LOGGING, STOP, ERROR } STATE;

void setup(void)
{
    //inits
    pinMode(CARD_DETECT, INPUT_PULLUP);
    pinMode(BAUD_RATE_0, INPUT_PULLUP);
    pinMode(BAUD_RATE_1, INPUT_PULLUP);
    pinMode(BAUD_RATE_2, INPUT_PULLUP);
    pinMode(BAUD_RATE_3, INPUT_PULLUP);
    buf[0].assignLEDs(SD_LED, OVR_LED);
    hbLED.begin(BLINK_IDLE);

    if ( digitalRead(CARD_DETECT) ) {
        STATE = ERROR;
        hbLED.mode(BLINK_NO_CARD);
    }
    else if ( !sd.begin(SS, SPI_HALF_SPEED) ) {    //initialize SD card
        STATE = ERROR;
        hbLED.mode(BLINK_ERROR);
    }

    //get the baud rate
    uint8_t baudIdx = ( PINC >> 2 ) & 0x0F;
    uint32_t USART0_BAUDRATE = baudRates[baudIdx];

    //set up USART0
    uint32_t DIVISOR = 8;
    if ( USART0_BAUDRATE == 115200 ) {
        UCSR0A |= _BV(U2X0);
    }
    else {
        DIVISOR = 16;
    }
    const uint16_t UBRR0_VALUE = ( (float)(F_CPU / DIVISOR) / (float)(USART0_BAUDRATE) + 0.5 ) - 1;
    UCSR0B = _BV(TXEN0);                   //enable tx
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);    //8-bit chars, async, 1 stop bit, no parity
    UBRR0 = UBRR0_VALUE;                   //baud rate setting
}

void loop(void)
{
    static uint8_t bufIdx;            //index to the current buffer
    buffer* bp;                       //buffer pointer

    btn.read();
    hbLED.run();

    switch (STATE)
    {
    int stat;

    case IDLE:
        if (btn.wasReleased()) {
            if ( !openFile() ) {
                STATE = ERROR;
                hbLED.mode(BLINK_ERROR);
            }
            else {
                STATE = LOGGING;
                hbLED.mode(BLINK_RUN);
                for (int i = 0; i < NBUF; i++) {
                    buf[i].init();                    //clear the buffers
                }
                UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);    //enable rx, tx & rx complete interrupt
            }
        }
        break;

    case LOGGING:
        if (btn.wasReleased()) {                  //user wants to stop
            UCSR0B = 0;                           //disable usart rx & rx complete interrupt
            UCSR0B = _BV(TXEN0);                  //disable rx & rx complete interrupt
            STATE = STOP;
        }
        else {                                    //watch for buffers that need to be written
            bp = &buf[bufIdx];                    //point to buffer
            stat = bp -> write(&logFile);         //write buffer if needed
            if ( ++bufIdx >= NBUF ) bufIdx = 0;   //increment index to next buffer
            if (stat < 0) STATE = ERROR;
        }
        break;

    case STOP:                                    //stop logging, flush buffers, etc.
        STATE = IDLE;
        bp -> ovrFlag = false;
        hbLED.mode(BLINK_IDLE);
        for (int i = 0; i < NBUF; i++) {
            bp = &buf[bufIdx];                    //point to buffer
            stat = bp -> write(&logFile, true);   //flush any characters in the buffer
            if ( ++bufIdx >= NBUF ) bufIdx = 0;   //increment index to next buffer
            if (stat < 0) STATE = ERROR;
        }
        logFile.sync();
        logFile.close();
        break;

    case ERROR:                                   //All hope abandon, ye who enter here
        break;

    }
}

//create a new file and open for write. return true if successful, else false.
bool openFile(void)
{
    char filename[] = "LOGnnn.TXT";

    for (int i = 1; i < 1000; i++) {
        filename[3] = i / 100 + '0';
        filename[4] = ( i / 10 ) % 10 + '0';
        filename[5] = i % 10 + '0';
        if (logFile.open(filename, O_CREAT | O_EXCL | O_WRITE)) break;
    }
    return logFile.isOpen();
}

//handle the incoming characters
ISR(USART_RX_vect)
{
    static bool overrun;
    static uint8_t bufIdx;                         //index to the current buffer
    buffer* bp = &buf[bufIdx];                     //pointer to current buffer
    uint8_t c = UDR0;                              //get the received character

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
