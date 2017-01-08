#ifndef ASF_H
#define ASF_H

// From module: Common SAM0 compiler driver
#include <compiler.h>
#include <status_codes.h>

// From module: Delay routines
#include <delay.h>

// From module: EVSYS - Event System Common
#include <events.h>

// From module: EVSYS - Event System with interupt hooks support
#include <events_hooks.h>

// From module: Generic board support
#include <board.h>

// From module: Interrupt management - SAM implementation
#include <interrupt.h>


// From module: NVM - Non-Volatile Memory
#include <nvm.h>

// From module: PORT - GPIO Pin Control
#include <port.h>

// From module: EEPROM Emulator Service
#include <eeprom.h>

#include <extint.h>

// From module: Part identification macros
#include <parts.h>

// From module: SERCOM Callback API
#include <sercom.h>
#include <sercom_pinout.h>
#include <sercom_interrupt.h>

// From module: SERCOM SPI - Serial Peripheral Interface (Callback APIs)
#include <spi.h>

// From module: SERCOM USART - Serial Communications (Callback APIs)
#include <usart.h>
#include <usart_interrupt.h>

// From module: SYSTEM - Clock Management for SAMD20
#include <clock.h>
#include <gclk.h>

// From module: RTC - Real Time Counter in Count Mode (Callback APIs)
#include <rtc_count.h>
#include <rtc_count_interrupt.h>

// From module: SYSTEM - Core System Driver
#include <system.h>

// From module: SYSTEM - I/O Pin Multiplexer
#include <pinmux.h>

// From module: SYSTEM - Interrupt Driver
#include <system_interrupt.h>

// From module: Standard serial I/O (stdio)
#include <stdio_serial.h>

// From module: USART - Serial interface- SAM implementation for devices with only USART
#include <serial.h>

// From module: SYSTEM - Power Management for SAM D20/D21/R21/D09/D10/D11/DA0/DA1
#include <power.h>

// From module: SYSTEM - Reset Management for SAM D20/D21/R21/D09/D10/D11/DA0/DA1
#include <reset.h>

// From module: TC - Timer Counter (Polled APIs)
#include <tc.h>

#endif // ASF_H
