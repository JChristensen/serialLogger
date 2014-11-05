# Serial Data Logger #

An Arduino-compatible logger that writes all serial data input on digital pin 0 (RXD) to a micro SD card. Serial input is interrupt-driven and double-buffered for maximum throughput. Input baud rate is selected by a rotary switch.

The heartbeat LED blinks in various patterns:
* Short blink: Idle mode.
* Steady blink on/off: Logging.
* Fast blink on/off: Error.
* Slow blink on/off: No SD card inserted (Reset the MCU after inserting card).

Press the button to start or stop logging. Each time logging is started, a new file is created.

If a buffer overrun occurs, the overrun LED stays on until logging is stopped or the MCU is reset.

```c++
//BAUD RATE CODES (A3:A0)      0     1     2     3     4      5      6      7      8      9
const uint32_t baudRates[] = { 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200 };
```

### Revision History ###

**05Nov2014 v2.0** --  Revised code to support custom PC board.

**06Jul2014 v1.1** --  Refactored buffer class into two classes, buffer and bufferPool. Increase buffers to 512 bytes. Direct port manipulation for LEDs. Change to SPI_FULL_SPEED. Drop 300 baud.

**02Jul2014 v1.0** -- Initial release.
### CC BY-SA ###
Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/

![](https://raw.githubusercontent.com/JChristensen/serialLogger/v2/serial_logger_pcb.jpg)
