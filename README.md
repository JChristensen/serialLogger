# Serial Data Logger #

An Arduino-compatible logger that writes all serial data input on digital pin 0 (RXD) to a micro SD card. Serial input is interrupt-driven and double-buffered for maximum throughput. Input baud rate is selected by a rotary switch. A custom PC board for the Serial Logger is at https://github.com/JChristensen/serialLogger_HW.

The heartbeat LED blinks in various patterns:
* Short blink: Idle mode.
* Steady blink on/off: Logging.
* Fast blink on/off: SD error.
* Slow blink on/off: No SD card inserted.

Press the Start/Stop button to start or stop logging. Each time logging is started, a new file is created.

If a buffer overrun occurs, the overrun LED stays on until logging is stopped or the MCU is reset. Logging continues after an overrun but stops after an SD error.

```c++
//BAUD RATE CODES (A3:A0)     0     1     2     3     4      5      6      7      8      9
const uint32_t baudRates[] = {1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200};
```

### Revision History ###

**05Nov2014 2.0.0** --  Revised code to support custom PC board.

**06Jul2014 1.1.0** --  Refactored buffer class into two classes, buffer and bufferPool. Increase buffers to 512 bytes. Direct port manipulation for LEDs. Change to SPI_FULL_SPEED. Drop 300 baud.

**02Jul2014 1.0.0** -- Initial release.
### CC BY-SA ###
Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/

![](https://github.com/JChristensen/serialLogger/blob/master/serial_logger_pcb.jpg)
