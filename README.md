# Serial Data Logger
https://github.com/JChristensen/serialLogger  
README file  
Jack Christensen

## License
Serial Data Logger Copyright (C) 2018 Jack Christensen GNU GPL v3.0

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License v3.0 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/gpl.html>

## Introduction

An Arduino-compatible logger that writes all serial data input on digital pin 0 (RXD) to a micro SD card. Serial input is interrupt-driven and double-buffered for maximum throughput. Input baud rate is selected by a rotary switch. A custom PC board for the Serial Logger is at https://github.com/JChristensen/serialLogger_HW.

The heartbeat LED blinks in various patterns:

* Short blink: Idle mode.
* Steady blink on/off: Logging.
* Fast blink on/off: SD error.
* Slow blink on/off: No SD card inserted.

Press the Start/Stop button to start or stop logging. Each time logging is started, a new file is created.

If a buffer overrun occurs, the overrun LED stays on until logging is stopped or the MCU is reset. Logging continues after an overrun but stops after an SD error.

## Baud rate switch settings

```c++
// BAUD RATE CODES (A3:A0)    0     1     2     3     4      5      6      7      8      9
const uint32_t baudRates[] = {1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200};
```

![](https://github.com/JChristensen/serialLogger/blob/master/serial_logger_pcb.jpg)
