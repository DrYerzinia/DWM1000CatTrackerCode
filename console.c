#include <asf.h>

#include "console.h"

#define CONF_USART_BAUDRATE 115200

static struct usart_module console_usart_instance;

static uint16_t rx_data;

// Need to be long enough for a 255 byte long Base64 encoded data packet
#define CONSOLE_BUFFER_SIZE 400

static uint16_t console_buf_rpos = 0;
static uint16_t console_buf_wpos = 0;
static uint16_t console_buf[CONSOLE_BUFFER_SIZE];


static void (*rx_callback)(char) = NULL;

void console_sleep(void){
	usart_disable(&console_usart_instance);
}

int console_process(){

	if(console_buf_wpos != console_buf_rpos){
		if(usart_get_job_status(&console_usart_instance, USART_TRANSCEIVER_TX) != STATUS_BUSY){
			usart_write_wait(&console_usart_instance, console_buf[console_buf_rpos]);
			console_buf_rpos++;
			if(console_buf_rpos >= CONSOLE_BUFFER_SIZE)
				console_buf_rpos = 0;
		}
	} else {

		return 0;

	}

	return 1;

}

static int ble_uart_put(void volatile * dummy, char c){

	// if(console_buf_wpos == console_buf_rpos) buffer overrun error

	console_buf[console_buf_wpos++] = c;

	if(console_buf_wpos >= CONSOLE_BUFFER_SIZE)
		console_buf_wpos = 0;

	// If we are not TXing start UART TX interupt cycle
	//attempt_write();

	return 1;

}

static void usart_read_callback(struct usart_module *const usart_module){

	rx_callback(rx_data);
	usart_read_job(&console_usart_instance, &rx_data);

}

static void configure_usart_callbacks(void){

	usart_register_callback(&console_usart_instance, usart_read_callback, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback(&console_usart_instance, USART_CALLBACK_BUFFER_RECEIVED);
}

static void configure_usart(void){

	struct usart_config config_usart;

	usart_get_config_defaults(&config_usart);

	config_usart.baudrate    = CONF_USART_BAUDRATE;
	config_usart.mux_setting = CONSOLE_MUX_SETTING;
	config_usart.pinmux_pad0 = CONSOLE_MUX_P0;
	config_usart.pinmux_pad1 = CONSOLE_MUX_P1;
	config_usart.pinmux_pad2 = CONSOLE_MUX_P2;
	config_usart.pinmux_pad3 = CONSOLE_MUX_P3;

	while(usart_init(&console_usart_instance, CONSOLE_SERCOM, &config_usart) != STATUS_OK);

	stdio_serial_init(&console_usart_instance, CONSOLE_SERCOM, &config_usart);

	usart_enable(&console_usart_instance);
}

void console_init(void *rxcb){

	rx_callback = rxcb;

	configure_usart();
	configure_usart_callbacks();

	ptr_put = ble_uart_put;

	usart_read_job(&console_usart_instance, &rx_data);

}
