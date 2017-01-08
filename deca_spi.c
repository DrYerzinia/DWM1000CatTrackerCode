#include "deca_spi.h"
#include "deca_sleep.h"
#include "deca_device_api.h"

#include <spi.h>

static struct spi_module spi_instance;

//int writetospi(uint16 headerLength, const uint8 *headerBuffer, uint32 bodylength, const uint8 *bodyBuffer);
//int readfromspi(uint16	headerLength, const uint8 *headerBuffer, uint32 readlength, uint8 *readBuffer );

int setbaud(uint32_t baudrate){

	return spi_set_baudrate(&spi_instance, baudrate);

}

static void spi_tx_wait(struct spi_module *const module, uint8_t *tx_data, uint16_t length){

	SercomSpi *const spi_module = &(module->hw->SPI);

	uint16_t pos = 0;
	uint8_t dummy;

	while(length--){

		while(!(spi_module->INTFLAG.reg & SERCOM_SPI_INTFLAG_DRE));
		spi_module->DATA.reg = tx_data[pos++] & SERCOM_SPI_DATA_MASK; // Remove mask probably

		while (!(spi_module->INTFLAG.reg & SERCOM_SPI_INTFLAG_RXC));
		dummy = (uint8_t)spi_module->DATA.reg;

	}

	while(!(spi_module->INTFLAG.reg & SERCOM_SPI_INTFLAG_TXC));

}

static void spi_rx_wait(struct spi_module *const module, uint8_t *rx_data, uint16_t length){

	SercomSpi *const spi_module = &(module->hw->SPI);

	uint16_t pos = 0;

	while(length--){

		while (!(spi_module->INTFLAG.reg & SERCOM_SPI_INTFLAG_DRE));
		spi_module->DATA.reg = 0;

		while (!(spi_module->INTFLAG.reg & SERCOM_SPI_INTFLAG_RXC));
		rx_data[pos++] = (uint8_t)spi_module->DATA.reg;

	}

	while(!(spi_module->INTFLAG.reg & SERCOM_SPI_INTFLAG_TXC));

}

int openspi(){

	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(DECA_SPI_CS, &pin_conf);
	port_pin_set_output_level(DECA_SPI_CS, true);

	struct spi_config config_spi;

	spi_get_config_defaults(&config_spi);

	config_spi.mode = SPI_MODE_MASTER;
	config_spi.mode_specific.master.baudrate = 1000000;
	config_spi.mux_setting = DECA_SPI_MUX_SETTING;
	config_spi.pinmux_pad0 = DECA_SPI_MUX_P0;
	config_spi.pinmux_pad1 = DECA_SPI_MUX_P1;
	config_spi.pinmux_pad2 = DECA_SPI_MUX_P2;
	config_spi.pinmux_pad3 = DECA_SPI_MUX_P3;

	spi_init(&spi_instance, DECA_SPI_SERCOM, &config_spi);
	spi_enable(&spi_instance);

	return 0;

}

int closespi(void){

	while(!spi_is_write_complete(&spi_instance)); // wait for tx buffer to empty
		spi_disable(&spi_instance);

	return 0;

}

#pragma GCC optimize ("O3")
int writetospi(uint16 headerLength, const uint8 *headerBuffer, uint32 bodylength, const uint8 *bodyBuffer){

    decaIrqStatus_t  stat;

    stat = decamutexon() ;

    port_pin_set_output_level(DECA_SPI_CS, false);

    spi_tx_wait(&spi_instance, headerBuffer, headerLength);
    spi_tx_wait(&spi_instance, bodyBuffer, bodylength);

    port_pin_set_output_level(DECA_SPI_CS, true);

    decamutexoff(stat) ;

    return 0;
}

#pragma GCC optimize ("O3")
int readfromspi(uint16 headerLength, const uint8 *headerBuffer, uint32 readlength, uint8 *readBuffer){

    decaIrqStatus_t  stat;

    stat = decamutexon();

    port_pin_set_output_level(DECA_SPI_CS, false);

    spi_tx_wait(&spi_instance, headerBuffer, headerLength);
    spi_rx_wait(&spi_instance, readBuffer, readlength);

    port_pin_set_output_level(DECA_SPI_CS, true);

    decamutexoff(stat) ;

    return 0;
}
