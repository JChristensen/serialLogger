Serial Data Logger for Arduino Uno and [Adafruit MicroSD card breakout board](https://www.adafruit.com/products/254) (product #254).

Input baud rate is set by selectively grounding pins A5:A2 per the codes below. Leaving a pin open is a one (high), grounding a pin is a zero (low). E.g. for 115200 ground A5 and A3, for 57600 ground A5 and A2, for 9600 ground A5, A4, A2.

The heartbeat LED blinks in various patterns:
* Short blink: Idle mode.
* Steady blink on/off: Logging.
* Fast blink on/off: Error.
* Slow on/off: No SD card inserted (Reset the MCU after inserting card).

Press the button to start or stop logging. Each time logging is started, a new file is created.

If a buffer overrun occurs, the overrun LED stays on until logging is stopped or the MCU is reset.

```c++
//BAUD RATE CODES (A5:A2)      0    1     2     3     4     5      6      7      8      9      10
const uint32_t baudRates[] = { 300, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200 };
```

Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/
