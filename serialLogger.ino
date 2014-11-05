/*-----------------------------------------------------------------------------*
 * Serial Data Logger by Jack Christensen                                      *
 *                                                                             *
 * 02Jul2014 v1 for Arduino Uno and Adafruit MicroSD card breakout             *
 *              board (product #254).                                          *
 * 05Nov2014 v2 for custom PC board.                                           *
 *                                                                             *
 * A logger that writes all serial data input on digital pin 0 (RXD) to        *
 * a micro SD card. Serial input is interrupt-driven and double-buffered       *
 * for maximum throughput. Input baud rate is selected by a rotary switch.     *
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
#include <SdFat.h>                  //http://github.com/greiman/SdFat
#include <SPI.h>                    //http://arduino.cc/en/Reference/SPI
#include "buffer.h"
#include "heartbeat.h"

//BAUD RATE CODES (A3:A0)      0     1     2     3     4      5      6      7      8      9
const uint32_t baudRates[] = { 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200 };

//pin assignments
const uint8_t BUTTON_PIN = 5;        //start/stop button
const uint8_t OVR_LED = 6;           //buffer overrun LED
const uint8_t SD_LED = 7;            //SD activity LED
const uint8_t HB_LED = 8;            //heartbeat LED
const uint8_t CARD_DETECT = 9;
const uint8_t BAUD_RATE_1 = A0;
const uint8_t BAUD_RATE_2 = A1;
const uint8_t BAUD_RATE_4 = A2;
const uint8_t BAUD_RATE_8 = A3;
const uint8_t UNUSED_PINS[] = { 2, 3, 4, A4, A5 };

bufferPool bp(SD_LED);
SdFat sd;
SdFile logFile;
heartbeat hbLED(HB_LED);

const bool PULLUP = true;
const bool INVERT = true;
const unsigned long DEBOUNCE_MS = 25;
Button btn(BUTTON_PIN, PULLUP, INVERT, DEBOUNCE_MS);
Button cardDetect(CARD_DETECT, PULLUP, INVERT, DEBOUNCE_MS);

enum STATES_t { IDLE, LOGGING, STOP, NO_CARD, ERROR } STATE;

void setup(void)
{
    //inits
    pinMode(OVR_LED, OUTPUT);
    pinMode(BAUD_RATE_1, INPUT_PULLUP);
    pinMode(BAUD_RATE_2, INPUT_PULLUP);
    pinMode(BAUD_RATE_4, INPUT_PULLUP);
    pinMode(BAUD_RATE_8, INPUT_PULLUP);
    //turn pullups on for unused pins for noise immunity
    for (uint8_t i = 0; i < sizeof(UNUSED_PINS) / sizeof(UNUSED_PINS[0]); i++) pinMode(i, INPUT_PULLUP);
    hbLED.begin(BLINK_IDLE);

    cardDetect.read();
    if ( cardDetect.isReleased() ) {
        STATE = NO_CARD;
        hbLED.mode(BLINK_NO_CARD);
    }
    else if ( !sd.begin(SS, SPI_FULL_SPEED) ) {    //initialize SD card
        STATE = ERROR;
        hbLED.mode(BLINK_ERROR);
    }

    //get the baud rate
    uint8_t baudIdx = ~PINC & 0x0F;
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
    switch (STATE)
    {
    int sdStat;

    case IDLE:
        if ( cardDetect.isReleased() ) {        //released == no card detected
            STATE = NO_CARD;
            hbLED.mode(BLINK_NO_CARD);
        }
        if ( btn.wasReleased() ) {
            if ( !openFile() ) {
                STATE = ERROR;
                hbLED.mode(BLINK_ERROR);
            }
            else {
                STATE = LOGGING;
                hbLED.mode(BLINK_RUN);
                bp.init();                      //initialize the buffer pool
                UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);    //enable rx, tx & rx complete interrupt
            }
        }
        break;

    case LOGGING:
        if ( cardDetect.isReleased() ) {        //released == no card detected
            STATE = NO_CARD;
            hbLED.mode(BLINK_NO_CARD);
        }
        if ( btn.wasReleased() ) {                //user wants to stop
            UCSR0B = _BV(TXEN0);                  //disable usart rx & rx complete interrupt, enable tx
            STATE = STOP;
        }
        else {                                    //watch for buffers that need to be written
            sdStat = bp.write(&logFile);          //write buffers if needed
            if (sdStat < 0) {
                logFile.close();
                STATE = ERROR;                    //SD error
                hbLED.mode(BLINK_ERROR);
            }
            if (bp.overrun) digitalWrite(OVR_LED, HIGH);
        }
        break;

    case STOP:                                    //stop logging, flush buffers, etc.
        STATE = IDLE;
        hbLED.mode(BLINK_IDLE);
        sdStat = bp.flush(&logFile);              //flush buffers if needed
        logFile.close();
        if (sdStat < 0) {
            STATE = ERROR;                        //SD error
            hbLED.mode(BLINK_ERROR);
        }
        bp.init();
        digitalWrite(OVR_LED, LOW);
        break;

    case NO_CARD:
        if ( cardDetect.isPressed() ) {
            if ( !sd.begin(SS, SPI_FULL_SPEED) ) {    //initialize SD card
                STATE = ERROR;
                hbLED.mode(BLINK_ERROR);
            }
            else {
                STATE = IDLE;
                hbLED.mode(BLINK_IDLE);
            }
        }
        break;

    case ERROR:                                   //All hope abandon, ye who enter here (reset required)
        break;

    }

    btn.read();
    cardDetect.read();
    hbLED.run();
}

//handle the incoming characters
ISR(USART_RX_vect)
{
    uint8_t c = UDR0;                   //get the received character
    bp.putch(c);                        //put it into the buffer
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
