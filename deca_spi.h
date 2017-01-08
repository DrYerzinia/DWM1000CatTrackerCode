#ifndef _DECA_SPI_H_
#define _DECA_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <asf.h>

#include "deca_types.h"

// SPI_SIGNAL_MUX_SETTING_N 22 0 MOSI, 23 1 SCK, 24 2 MISO
// SPI_SIGNAL_MUX_SETTING_O PA04 MOSI 0, PA06 MISO 2, PA07 SCLK 3

#if (__SAMD20E17__)

#define DECA_SPI_SERCOM		SERCOM3
#define DECA_SPI_CS		PIN_PA25
#define DECA_SPI_MUX_SETTING	SPI_SIGNAL_MUX_SETTING_C
#define DECA_SPI_MUX_P0		PINMUX_PA22C_SERCOM3_PAD0
#define DECA_SPI_MUX_P1		PINMUX_PA23C_SERCOM3_PAD1
#define DECA_SPI_MUX_P2		PINMUX_PA24C_SERCOM3_PAD2
#define DECA_SPI_MUX_P3		PINMUX_UNUSED

#elif (__SAMD11D14AM__)

#define DECA_SPI_SERCOM		SERCOM0
#define DECA_SPI_CS		PIN_PA03
#define DECA_SPI_MUX_SETTING	SPI_SIGNAL_MUX_SETTING_O
#define DECA_SPI_MUX_P0		PINMUX_PA04D_SERCOM0_PAD0
#define DECA_SPI_MUX_P1		PINMUX_UNUSED
#define DECA_SPI_MUX_P2		PINMUX_PA06D_SERCOM0_PAD2
#define DECA_SPI_MUX_P3		PINMUX_PA07D_SERCOM0_PAD3

#endif

#define DECA_MAX_SPI_HEADER_LENGTH      (3)                     // max number of bytes in header (for formating & sizing)

int setbaud(uint32_t baudrate);

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(void) ;

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void) ;

#ifdef __cplusplus
}
#endif

#endif /* _DECA_SPI_H_ */
