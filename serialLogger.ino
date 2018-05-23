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

#include <avr/wdt.h>
#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <SdFat.h>              // https://github.com/greiman/SdFat
#include <SPI.h>                // https://arduino.cc/en/Reference/SPI
#include "buffer.h"
#include "heartbeat.h"

//BAUD RATE CODES (A3:A0)     0     1     2     3     4      5      6      7      8      9
const uint32_t baudRates[] = {1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200};

//pin assignments
//0, 1 USART
const uint8_t BUTTON_PIN = 5;        //start/stop button
const uint8_t OVR_LED = 6;           //buffer overrun LED
const uint8_t SD_LED = 7;            //SD activity LED
const uint8_t HB_LED = 8;            //heartbeat LED
const uint8_t CARD_DETECT = 9;
//10-13 SPI
const uint8_t BAUD_RATE_1 = A0;
const uint8_t BAUD_RATE_2 = A1;
const uint8_t BAUD_RATE_4 = A2;
const uint8_t BAUD_RATE_8 = A3;
const uint8_t UNUSED_PINS[] = { 2, 3, 4, A4, A5 };

bufferPool bp(SD_LED);
SdFat sd;
SdFile logFile;
heartbeat hbLED(HB_LED);

Button
    btn(BUTTON_PIN),
    cardDetect(CARD_DETECT);

enum STATES_t {LOG_IDLE, LOG_RUNNING, LOG_STOP, LOG_NO_CARD, LOG_ERROR};
STATES_t STATE;

//ensure the watchdog is disabled after a reset. this code does not work with a bootloader.
void wdt_init() __attribute__((naked)) __attribute__((used)) __attribute__((section(".init3")));

void wdt_init()
{
    MCUSR = 0;        //must clear WDRF in MCUSR in order to clear WDE in WDTCSR
    wdt_reset();
    wdt_disable();
}

void setup()
{
    //inits
    pinMode(OVR_LED, OUTPUT);
    pinMode(SD_LED, OUTPUT);
    pinMode(HB_LED, OUTPUT);
    pinMode(BAUD_RATE_1, INPUT_PULLUP);
    pinMode(BAUD_RATE_2, INPUT_PULLUP);
    pinMode(BAUD_RATE_4, INPUT_PULLUP);
    pinMode(BAUD_RATE_8, INPUT_PULLUP);
    btn.begin();
    cardDetect.begin();

    //enable pullups on unused pins for noise immunity
    for (uint8_t i = 0; i < sizeof(UNUSED_PINS) / sizeof(UNUSED_PINS[0]); i++) pinMode(i, INPUT_PULLUP);

    //LED test
    boolean ledState = false;
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(OVR_LED, ledState = !ledState);
        digitalWrite(SD_LED, ledState);
        digitalWrite(HB_LED, ledState);
        delay(100);
    }

    hbLED.begin(BLINK_IDLE);

    cardDetect.read();
    if ( cardDetect.isReleased() ) {
        STATE = LOG_NO_CARD;
        hbLED.mode(BLINK_NO_CARD);
    }
    else if ( !sd.begin(SS, SPI_FULL_SPEED) ) {    //initialize SD card
        STATE = LOG_ERROR;
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

void loop()
{
    switch (STATE)
    {
    int sdStat;

    case LOG_IDLE:
        if ( cardDetect.wasReleased() ) {        //released == no card detected
            STATE = LOG_NO_CARD;
            hbLED.mode(BLINK_NO_CARD);
        }
        if ( btn.wasReleased() ) {
            if ( !openFile() ) {
                STATE = LOG_ERROR;
                hbLED.mode(BLINK_ERROR);
            }
            else {
                STATE = LOG_RUNNING;
                hbLED.mode(BLINK_RUN);
                bp.init();                      //initialize the buffer pool
                UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);    //enable rx, tx & rx complete interrupt
            }
        }
        break;

    case LOG_RUNNING:
        if ( cardDetect.wasReleased() ) {        //released == no card detected
            STATE = LOG_NO_CARD;
            hbLED.mode(BLINK_NO_CARD);
        }
        if ( btn.wasReleased() ) {                //user wants to stop
            UCSR0B = _BV(TXEN0);                  //disable usart rx & rx complete interrupt, enable tx
            STATE = LOG_STOP;
        }
        else {                                    //watch for buffers that need to be written
            sdStat = bp.write(&logFile);          //write buffers if needed
            if (sdStat < 0) {
                logFile.close();
                STATE = LOG_ERROR;                //SD error
                hbLED.mode(BLINK_ERROR);
            }
            if (bp.overrun) digitalWrite(OVR_LED, HIGH);
        }
        break;

    case LOG_STOP:                                //stop logging, flush buffers, etc.
        STATE = LOG_IDLE;
        hbLED.mode(BLINK_IDLE);
        sdStat = bp.flush(&logFile);              //flush buffers if needed
        logFile.close();
        if (sdStat < 0) {
            STATE = LOG_ERROR;                    //SD error
            hbLED.mode(BLINK_ERROR);
        }
        bp.init();
        digitalWrite(OVR_LED, LOW);
        break;

    case LOG_NO_CARD:
        if ( cardDetect.wasPressed() ) {          //card was inserted, reset mcu for a clean start
            wdt_enable(WDTO_15MS);
            while (1);
        }
        break;

    case LOG_ERROR:                               //All hope abandon, ye who enter here (reset required)
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
bool openFile()
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

