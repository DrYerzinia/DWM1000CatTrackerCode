
#include <asf.h>

#include <system_interrupt.h>

#include <sercom.h>
#include <sercom_interrupt.h>

#include <usart.h>
#include <usart_interrupt.h>

#include <stdio_serial.h>
#include <serial.h>

#if (__SAMD20E17__)

#define CONSOLE_SERCOM		SERCOM2
#define CONSOLE_MUX_SETTING	USART_RX_3_TX_2_XCK_3
#define CONSOLE_MUX_P0		PINMUX_UNUSED
#define CONSOLE_MUX_P1		PINMUX_UNUSED
#define CONSOLE_MUX_P2		PINMUX_PA14C_SERCOM2_PAD2
#define CONSOLE_MUX_P3		PINMUX_PA15C_SERCOM2_PAD3

#elif (__SAMD11D14AM__)

#define CONSOLE_SERCOM		SERCOM2
#define CONSOLE_MUX_SETTING	USART_RX_1_TX_0_XCK_1
#define CONSOLE_MUX_P0		PINMUX_PA22D_SERCOM2_PAD0
#define CONSOLE_MUX_P1		PINMUX_PA15D_SERCOM2_PAD1
#define CONSOLE_MUX_P2		PINMUX_UNUSED
#define CONSOLE_MUX_P3		PINMUX_UNUSED

#endif

#ifndef TINYROM
  #define PRINT(a) printf a
#else
  #define PRINT(a) (void)0
#endif

void console_sleep(void);
void console_init(void *rxcb);
int console_process(void);
