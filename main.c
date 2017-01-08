#include <asf.h>

#include <stdio.h>

#include <string.h>
#include <ctype.h>

#include <stdbool.h>

#include "console.h"

#include "deca_spi.h"
#include "deca_regs.h"
#include "deca_sleep.h"
#include "deca_device_api.h"

#define PRO_BOARD

#ifdef PRO_BOARD
#define DECA_RESET_PIN PIN_PA07
#else
#define DECA_RESET_PIN PIN_PA00
#endif

#ifndef TINY_ROM
#define COMMAND_BUFFER_SIZE 100

static char command_buffer[COMMAND_BUFFER_SIZE];
static uint8_t command_buffer_position = 0;
#endif

static uint16 pan_id = 0xDECA; // 0xCA75

static uint16 my_addr = 0xCAFE;
static uint16 master_ancor_addr = 0xBEEF;
static uint16 ancor1_addr = 0xBEEF;
static uint16 ancor2_addr = 0xBA5E;
static uint16 ancor3_addr = 0xBA11;

static uint16 adly = 100;

static uint8 tx_pwr = 0x22;


/* Default communication configuration. We use here EVK1000's default mode (mode 3). */
static dwt_config_t config = {
    2,               /* Channel number. */
    DWT_PRF_64M,     /* Pulse repetition frequency. */
    DWT_PLEN_1024,   /* Preamble length. */
    DWT_PAC32,       /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* Use non-standard SFD (Boolean) */
    DWT_BR_110K,     /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    (1025 + 64 - 32) /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
};

/* Buffer to store received frame. See NOTE 1 below. */
#define FRAME_LEN_MAX 127
#define RX_BUF_LEN 24
static uint8 rx_buffer[FRAME_LEN_MAX];

/* Hold copy of status register state here for reference, so reader can examine it at a breakpoint. */
static uint32 status_reg = 0;

static uint8 tx_msg[] = {0xC5, 0, 'D', 'E', 'C', 'A', 'W', 'A', 'V', 'E', 0, 0};
/* Index to access to sequence number of the blink frame in the tx_msg array. */
#define BLINK_FRAME_SN_IDX 1

/* Inter-frame delay period, in milliseconds. */
#define TX_DELAY_MS 1000

///// DS TWR INIT

/* Inter-ranging delay period, in milliseconds. */
#define RNG_DELAY_MS 1000

/* Default antenna delay values for 64 MHz PRF. See NOTE 1 below. */
#define TX_ANT_DLY 16436
#define RX_ANT_DLY 16436

/* Frames used in the ranging process. See NOTE 2 below. */
static uint8 tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x21, 0, 0};
//static uint8 rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x10, 0x02, 0, 0, 0, 0};
static uint8 tx_final_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//static uint8 rx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x21, 0, 0};
static uint8 tx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x10, 0x02, 0, 0, 0, 0};
//static uint8 rx_final_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static uint8 range_report[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x6A, 0, 0, 0, 0, 0, 0, 0, 0};

/* Length of the common part of the message (up to and including the function code, see NOTE 2 below). */
#define ALL_MSG_COMMON_LEN 10
/* Indexes to access some of the fields in the frames defined above. */
#define ALL_MSG_SN_IDX 2
#define ADDRESS_IDX 3
#define FINAL_MSG_POLL_TX_TS_IDX 10
#define FINAL_MSG_RESP_RX_TS_IDX 14
#define FINAL_MSG_FINAL_TX_TS_IDX 18
#define FINAL_MSG_TS_LEN 4
/* Frame sequence number, incremented after each transmission. */
static uint8 frame_seq_nb = 0;

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 µs and 1 µs = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Delay between frames, in UWB microseconds. See NOTE 4 below. */
/* This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature. */
#define POLL_TX_TO_RESP_RX_DLY_UUS 150
/* This is the delay from Frame RX timestamp to TX reply timestamp used for calculating/setting the DW1000's delayed TX function. This includes the
 * frame length of approximately 2.66 ms with above configuration. */
#define RESP_RX_TO_FINAL_TX_DLY_UUS 4100
/* Receive response timeout. See NOTE 5 below. */
#define RESP_RX_TIMEOUT_UUS 8000

/* Delay between frames, in UWB microseconds. See NOTE 4 below. */
/* This is the delay from Frame RX timestamp to TX reply timestamp used for calculating/setting the DW1000's delayed TX function. This includes the
 * frame length of approximately 2.46 ms with above configuration. */
#define POLL_RX_TO_RESP_TX_DLY_UUS 5600
/* This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature. */
#define RESP_TX_TO_FINAL_RX_DLY_UUS 500
/* Receive final timeout. See NOTE 5 below. */
 #define FINAL_RX_TIMEOUT_UUS 5300

/* Time-stamps of frames transmission/reception, expressed in device time units.
 * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
typedef unsigned long long uint64;

/* Declaration of static functions. */
static uint64 get_tx_timestamp_u64(void);
static uint64 get_rx_timestamp_u64(void);
static void final_msg_set_ts(uint8 *ts_field, uint64 ts);
static void final_msg_get_ts(const uint8 *ts_field, uint32 *ts);
static void set_tof_dtu(uint8 *tof_dtu_field, uint64 tof_dtu);
static void set_addresses(uint8 *address_field, uint16 pan_id, uint16 source_addr, uint16 dest_addr);

// DS TWR RESP

/* Timestamps of frames transmission/reception.
 * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
typedef signed long long int64;
static uint64 poll_rx_ts;
static uint64 resp_tx_ts;
static uint64 final_rx_ts;

/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

/* Hold copies of computed time of flight and distance here for reference, so reader can examine it at a breakpoint. */
static double tof;
static double distance;

void simple_tx(void);
void simple_rx(void);

void send_range_report(uint64 tof_dtu, uint16 tag_addr);

void ds_twr_resp(uint64 * tof_dtu_e, uint16 * tag_addr_e);
void ds_twr_init(uint16 ancor_addr);

bool run_ds_twr_resp = false;
bool run_range_init = false;
bool goto_sleep = false;

#define MODE_NONE		0
#define MODE_TAG		1
#define MODE_ANCOR_MAIN 	2
#define MODE_ANCOR_SLAVE	3

uint8 config_mode = MODE_NONE;
uint8 mode = MODE_NONE;

static bool check_range_resp(uint8 * rxb, uint16 len){

	if(
	// match FRAME TYPE
	rxb[0] == 0x41
	&& rxb[1] == 0x88
	// match PAN
	&&  rxb[3] == ((uint8)pan_id)
	&&  rxb[4] == ((uint8)(pan_id >> 8))
	// match Destination Adderss
	&& rxb[5] == ((uint8)my_addr)
	&& rxb[6] == ((uint8)(my_addr >> 8))
	// match message type
	&& rxb[9] == 0x6A
	){

		uint16 anc_addr = (((uint16)rx_buffer[8]) << 8) + rx_buffer[7];
		uint16 tag_addr = (((uint16)rx_buffer[15]) << 8) + rx_buffer[14];
		PRINT(("RANGE %04X %04X %02X%02X%02X%02X\n\r", anc_addr, tag_addr, rxb[10], rxb[11], rxb[12], rxb[13]));

		return true;

	}

	return false;

}

#ifndef TINY_ROM

static void print_data(uint8 * rxb, uint16 len){

	PRINT(("\nData: \n"));

	int i = 0;
	while(i < len){
		int j;
		for(j = 0; j < 20; j++){
			if(i < len){
				if(isprint(rxb[i])) putchar(rxb[i]);
				else putchar('.');
			} else putchar(' ');
			i++;
		}
		i -= 20;
		PRINT(("  |  "));
		for(j = 0; j < 20; j++){
			if(i < len){
				PRINT((" %02X", rxb[i]));
				i++;
			}
		}
		PRINT(("\n\r"));
	}

}

#endif

#ifndef TINY_ROM
static void print_float(float f){

        int i1 = (int)f;
        int i2 = (int) ((f-((float)i1))*1000.0);
        PRINT(("%d.%03d", i1, i2));

}
#endif

#ifndef TINY_ROM
uint8_t page_data[EEPROM_PAGE_SIZE];
#endif

#ifndef TINY_ROM
static void save_config(void){

	page_data[0] = (uint8)(pan_id >> 8);
	page_data[1] = (uint8)(pan_id);

	page_data[2] = (uint8)(my_addr >> 8);
	page_data[3] = (uint8)(my_addr);

	page_data[4] = (uint8)(master_ancor_addr >> 8);
	page_data[5] = (uint8)(master_ancor_addr);

	page_data[6] = (uint8)(ancor1_addr >> 8);
	page_data[7] = (uint8)(ancor1_addr);

	page_data[8] = (uint8)(ancor2_addr >> 8);
	page_data[9] = (uint8)(ancor2_addr);

	page_data[10] = (uint8)(ancor3_addr >> 8);
	page_data[11] = (uint8)(ancor3_addr);

	page_data[12] = (uint8)(adly >> 8);
	page_data[13] = (uint8)(adly);

	page_data[14] = config_mode;

	page_data[15] = tx_pwr;

	eeprom_emulator_write_page(0, page_data);
	eeprom_emulator_commit_page_buffer();

}
#endif

#ifndef TINY_ROM
static void load_config(void){

	eeprom_emulator_read_page(0, page_data);

	pan_id = (page_data[0] << 8) + page_data[1];
	my_addr = (page_data[2] << 8) + page_data[3];
	master_ancor_addr = (page_data[4] << 8) + page_data[5];
	ancor1_addr = (page_data[6] << 8) + page_data[7];
	ancor2_addr = (page_data[8] << 8) + page_data[9];
	ancor3_addr = (page_data[10] << 8) + page_data[11];
	adly = (page_data[12] << 8) + page_data[13];
	config_mode = page_data[14];
	mode = config_mode = page_data[14];
	tx_pwr = page_data[15];

}
#endif

#ifndef TINY_ROM
static void print_mode(uint8 m){

	switch(m){
		case MODE_NONE:
			PRINT(("None\n\r"));
			break;
		case MODE_TAG:
			PRINT(("Tag\n\r"));
			break;
		case MODE_ANCOR_MAIN:
			PRINT(("Main Ancor\n\r"));
			break;
		case MODE_ANCOR_SLAVE:
			PRINT(("Secondary Ancor\n\r"));
			break;
		default:
			PRINT(("Unknown!\n\r"));
			break;
	}

}
#endif

static void set_tx_pwr(void){

	uint32 txp = tx_pwr + (tx_pwr << 8) + (tx_pwr << 16) + (tx_pwr << 24);
	dwt_write32bitreg(TX_POWER_ID, txp);

}

#ifndef TINY_ROM
bool boot_counter = true;
#endif

static void reenable_dwt(void){

	setbaud(2000000);

	dwt_configure(&config);

	set_tx_pwr();

	dwt_setrxantennadelay(RX_ANT_DLY);
	dwt_settxantennadelay(TX_ANT_DLY);

	dwt_enableframefilter(DWT_FF_DATA_EN);
	dwt_setpanid(pan_id);
	dwt_setaddress16(my_addr);

}

#ifndef TINY_ROM
static void print_config(void){

	PRINT(("PAN ID: %04X\n\r", pan_id));
	PRINT(("My Address: %04X\n\r", my_addr));
	PRINT(("Master Ancor Address: %04X\n\r", master_ancor_addr));
	PRINT(("Ancor 1 Address: %04X\n\r", ancor1_addr));
	PRINT(("Ancor 2 Address: %04X\n\r", ancor2_addr));
	PRINT(("Ancor 3 Address: %04X\n\r", ancor3_addr));
	PRINT(("Range Report Delay: %d\n\r", adly));

	PRINT(("TX Power: %02X\n\r", tx_pwr));

	PRINT(("Boot Mode: "));
	print_mode(config_mode);

}
#endif

#ifndef TINY_ROM
static void console_input(char c){

	if(boot_counter){
		if(c == 'c'){
			mode = MODE_NONE;
			boot_counter = false;
		}
		return;
	}

	switch(c){
		case 0x0A:	// New Line
		case 0x0D:	// Carriage Return

			PRINT(("\n\r"));
			command_buffer[command_buffer_position] = '\0';

			if(strcmp(command_buffer, "hello") == 0){

				PRINT(("HI\n\r"));

			} else if(strcmp(command_buffer, "clk") == 0){

				PRINT(("%lu\n\r", system_cpu_clock_get_hz()));

			} else if(strcmp(command_buffer, "spiw") == 0){

				writetospi(2, (uint8*)"hi", 5, (uint8*)"hello");

			} else if(strcmp(command_buffer, "devid") == 0){

				uint32 devid = dwt_readdevid();
				PRINT(("%08lx\n\r", devid));

			} else if(strcmp(command_buffer, "eui") == 0){

				uint8 eui[8];
				dwt_geteui(eui);

				int i = 0;
				for(i = 0; i < 8; i++) PRINT(("%02X", eui[i]));
				PRINT(("\n\r"));

			} else if(strcmp(command_buffer, "tx") == 0){

				simple_tx();

			} else if(strcmp(command_buffer, "rx") == 0){

				simple_rx();

			} else if(strcmp(command_buffer, "range_init") == 0){

				run_range_init = true;

			} else if(strcmp(command_buffer, "range_resp") == 0){

				run_ds_twr_resp = true;

			} else if(strcmp(command_buffer, "spi1") == 0){

				PRINT(("setbaud: %d\n\r", setbaud(1000000)));

			} else if(strcmp(command_buffer, "spi2") == 0){

				PRINT(("setbaud: %d\n\r", setbaud(2000000)));

			} else if(strcmp(command_buffer, "spi8") == 0){

				PRINT(("setbaud: %d\n\r", setbaud(8000000)));

			} else if(strcmp(command_buffer, "sleep") == 0){

				goto_sleep = true;

			} else if(strncmp(command_buffer, "set_address", 11) == 0){

				my_addr = strtol(command_buffer + 12, NULL, 16);
				PRINT(("My Address: %04X\n\r", my_addr));

				dwt_enableframefilter(DWT_FF_DATA_EN);
				dwt_setpanid(pan_id);
				dwt_setaddress16(my_addr);

			} else if(strncmp(command_buffer, "set_maa", 7) == 0){

				master_ancor_addr = strtol(command_buffer + 8, NULL, 16);
				PRINT(("Master Ancor Address: %04X\n\r", master_ancor_addr));

			} else if(strncmp(command_buffer, "set_panid", 9) == 0){

				pan_id = strtol(command_buffer + 10, NULL, 16);
				PRINT(("PANID: %04X\n\r", pan_id));

			} else if(strncmp(command_buffer, "set_a1a", 7) == 0){

				ancor1_addr = strtol(command_buffer + 8, NULL, 16);
				PRINT(("Ancor 1 Address: %04X\n\r", ancor1_addr));

			} else if(strncmp(command_buffer, "set_a2a", 7) == 0){

				ancor2_addr = strtol(command_buffer + 8, NULL, 16);
				PRINT(("Ancor 2 Address: %04X\n\r", ancor2_addr));

			} else if(strncmp(command_buffer, "set_a3a", 7) == 0){

				ancor3_addr = strtol(command_buffer + 8, NULL, 16);
				PRINT(("Ancor 3 Address: %04X\n\r", ancor3_addr));

			} else if(strncmp(command_buffer, "set_adly", 7) == 0){

				adly = strtol(command_buffer + 8, NULL, 16);
				PRINT(("Ancor delay: %d ms\n\r", adly));

			} else if(strncmp(command_buffer, "tx_pwr", 6) == 0){

				// PG_DELAY channel 2 0xC2
				//TX POWER
				//31:24		BOOST_0.125ms_PWR
				//23:16		BOOST_0.25ms_PWR-TX_SHR_PWR
				//15:8		BOOST_0.5ms_PWR-TX_PHR_PWR
				//7:0		DEFAULT_PWR-TX_DATA_PWR
				// Default 0x1E080222
				//           3dB 0.5dB
				// MAX POWER 000 11111 0x1F

				tx_pwr = strtol(command_buffer + 7, NULL, 16);
				PRINT(("txpwr: %02X\n\r", tx_pwr));
				set_tx_pwr();


			} else if(strcmp(command_buffer, "test_range") == 0){

				send_range_report(10895, 0xCAFE);

			} else if(strcmp(command_buffer, "mode_none") == 0){

				config_mode = MODE_NONE;
				PRINT(("mode_none\n\r"));
				PRINT(("save config and reboot to enable\n\r"));

			} else if(strcmp(command_buffer, "mode_tag") == 0){

				config_mode = MODE_TAG;
				PRINT(("mode_tag\n\r"));
				PRINT(("save config and reboot to enable\n\r"));

			} else if(strcmp(command_buffer, "reboot") == 0){

				system_reset();

			} else if(strcmp(command_buffer, "mode_ancor_main") == 0){

				config_mode = MODE_ANCOR_MAIN;
				PRINT(("mode_ancor_main\n\r"));
				PRINT(("save config and reboot to enable\n\r"));

			} else if(strcmp(command_buffer, "mode_ancor_slave") == 0){

				config_mode = MODE_ANCOR_SLAVE;
				PRINT(("mode_ancor_slave\n\r"));
				PRINT(("save config and reboot to enable\n\r"));

			} else if(strcmp(command_buffer, "save_config") == 0){

				save_config();

			} else if(strcmp(command_buffer, "load_config") == 0){

				load_config();

			#ifndef TINY_ROM

			} else if(strcmp(command_buffer, "print_config") == 0){

				print_config();

			#endif

			} else if(strcmp(command_buffer, "reset_radio") == 0){

				port_pin_set_output_level(DECA_RESET_PIN, true);
				deca_sleep(1000);
				port_pin_set_output_level(DECA_RESET_PIN, false);

				dwt_initialise(DWT_LOADUCODE); // LOADUCODE DS TWR // DWT_LOADNONE
				reenable_dwt();

			} else {

				PRINT(("?\n\r"));

			}

			command_buffer_position = 0;
			break;

		case 0x7F:	// Delete
		case 0x08:	// Backspace
			if(command_buffer_position > 0){
				command_buffer_position--;
				putchar(c);
			}
			break;

		default:

			command_buffer[command_buffer_position] = c;
			command_buffer_position++;

			if(command_buffer_position == COMMAND_BUFFER_SIZE) command_buffer_position--;
			else
				putchar(c);

			break;

	}

}
#endif

struct rtc_module rtc_instance;

bool sleeping = false;
bool check_sleep = false;

int period_count = 0;

static void rtc_overflow_callback(void){

	if(!sleeping && check_sleep){

		// Radio error in tag mode should be sleeping when this triggers
		system_reset(); // This will restart the radio and the processor

	}
	if(mode == MODE_ANCOR_SLAVE || mode == MODE_ANCOR_MAIN){
		period_count++;
		if(period_count > 3){
			system_reset();
		}
	}

}

/**
 * \brief rtc module initialization.
*/
static void configure_rtc_count(void){
	struct rtc_count_config config_rtc_count;

	rtc_count_get_config_defaults(&config_rtc_count);

	config_rtc_count.prescaler         = RTC_COUNT_PRESCALER_DIV_1024;
	config_rtc_count.mode              = RTC_COUNT_MODE_32BIT;
	config_rtc_count.clear_on_match    = true;
	config_rtc_count.compare_values[0] = 30; // every 30 seconds

	rtc_count_init(&rtc_instance, RTC, &config_rtc_count);
	rtc_count_enable(&rtc_instance);
}

static void configure_rtc_callbacks(void){

	rtc_count_register_callback(&rtc_instance, rtc_overflow_callback, RTC_COUNT_CALLBACK_COMPARE_0);
	rtc_count_enable_callback(&rtc_instance, RTC_COUNT_CALLBACK_COMPARE_0);

}

#ifndef TINY_ROM
bool eeprom_good = false;
#endif

#ifndef TINY_ROM
static void configure_eeprom(void){

	/* Setup EEPROM emulator service */
	enum status_code error_code = eeprom_emulator_init();
	if(error_code == STATUS_ERR_NO_MEMORY){

		PRINT(("What?! No EEPROM!\n\r"));

	} else if (error_code != STATUS_OK) {

		/* Erase the emulated EEPROM memory (assume it is unformatted or
		 * irrecoverably corrupt) */

		PRINT(("EEPROM set up for first time.\n\r"));

		eeprom_emulator_erase_memory();
		eeprom_emulator_init();

		eeprom_good = true;

	} else {

		eeprom_good = true;

		PRINT(("Loading config...\n\r"));
		load_config();

		#ifndef TINY_ROM
		print_config();
		#endif

	}

}
#endif

int main(void){

	system_init();
	delay_init();

	#ifndef TINY_ROM
	console_init(console_input);
	#endif

	openspi();

	configure_rtc_count();
	configure_rtc_callbacks();

	#ifndef TINY_ROM
	configure_eeprom();
	#endif

	system_interrupt_enable_global();

	#ifndef TINY_ROM
	enum system_reset_cause reset_cause = system_get_reset_cause();
	PRINT(("Reset Cause: %d\n\r", reset_cause));

	PRINT(("DECAWAVE Module up\n\r"));

	int i, j;
	PRINT(("Press c to enter console mode...\n\r"));
	while(console_process() == 1);
	for(j = 5; j > 0; j--){
		PRINT(("%d ", j));
		while(console_process() == 1);
		for(i = 0; i < 10; i++){
			delay_ms(100);
			if(boot_counter == false) break;
		}
		if(boot_counter == false) break;
	}
	PRINT(("\n\r"));
	boot_counter = false;
	#endif

	// Reset and initialise DW1000.
	// For initialisation, DW1000 clocks must be temporarily set to crystal speed. After initialisation SPI rate can be increased for optimum
	// performance.
	//reset_DW1000(); // Pull reset low, not hooked up Target specific drive of RSTn line into DW1000 low for a period.

	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(DECA_RESET_PIN, &pin_conf);
	port_pin_set_output_level(DECA_RESET_PIN, true);
	deca_sleep(2);
	port_pin_set_output_level(DECA_RESET_PIN, false);
	deca_sleep(5);

	//spi_set_rate_low(); // less than 3 MHz
	dwt_initialise(DWT_LOADUCODE); // LOADUCODE DS TWR // DWT_LOADNONE
	//spi_set_rate_high(); // 20 MHz

	reenable_dwt();

	// DS TWR ///////

	while(true){

		#ifndef TINY_ROM
		if(run_range_init){
			run_range_init = false;
			ds_twr_init(ancor1_addr);
			//delay_ms(10);
			//ds_twr_init(ancor2_addr);
			//delay_ms(10);
			//ds_twr_init(ancor3_addr);
		}
		if(run_ds_twr_resp){

			run_ds_twr_resp = false;

			uint64 tof_dtu;
			uint16 tag_addr;

			ds_twr_resp(&tof_dtu, &tag_addr);

		}
		#endif
		if(goto_sleep){

			goto_sleep = false;

			dwt_configuresleep(DWT_PRESRV_SLEEP | DWT_CONFIG, DWT_WAKE_CS | DWT_SLP_EN);
			dwt_entersleep();

			#ifndef TINY_ROM
			console_sleep();
			#endif
			closespi();

			system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);
			system_sleep();

			#ifndef TINY_ROM
			console_init(console_input);
			#endif
			openspi();

			sleeping = false;
			PRINT(("AWAKE!\r\n"));

			port_pin_set_output_level(PIN_PA25, false);
			delay_ms(1);
			port_pin_set_output_level(PIN_PA25, true);

		}

		if(mode == MODE_TAG){

			check_sleep = true;

			delay_ms(500);

			reenable_dwt();

			// TODO retry up to 3 times if ranging fails
			ds_twr_init(ancor1_addr);
			delay_ms(10);
			ds_twr_init(ancor2_addr);
			delay_ms(10);
			ds_twr_init(ancor3_addr);
			goto_sleep = true;
			sleeping = true;

		}
		#ifndef TINY_ROM
		else if(mode == MODE_ANCOR_MAIN){

			period_count = 0;

			uint64 tof_dtu = 0;
			uint16 tag_addr = 0;

			ds_twr_resp(&tof_dtu, &tag_addr);
			if(tof_dtu != 0){
				// TODO get tag addr
				PRINT(("RANGE %04X %04X %08lX\n\r", my_addr, tag_addr, (uint32)(tof_dtu)));
			}

		} else if(mode == MODE_ANCOR_SLAVE){

			period_count = 0;

			uint64 tof_dtu = 0;
			uint16 tag_addr = 0;

			ds_twr_resp(&tof_dtu, &tag_addr);
			if(tof_dtu != 0){
				delay_ms(adly);
				send_range_report(tof_dtu, tag_addr);
			}

		}
		#endif

		#ifndef TINY_ROM
		while(console_process() == 1);
		#endif

	}

	return 0;

}

#define RANGE_REPORT_TOF_DTU_IDX 10

void send_range_report(uint64 tof_dtu, uint16 tag_addr){

	range_report[ALL_MSG_SN_IDX] = frame_seq_nb;
	set_addresses(&range_report[ADDRESS_IDX], pan_id, my_addr, master_ancor_addr);
	set_tof_dtu(&range_report[RANGE_REPORT_TOF_DTU_IDX], tof_dtu);
	range_report[14] = (uint8)(tag_addr);
	range_report[15] = (uint8)(tag_addr >> 8);
	dwt_writetxdata(sizeof(range_report), range_report, 0);
	dwt_writetxfctrl(sizeof(range_report), 0);

	dwt_starttx(DWT_START_TX_IMMEDIATE);
	while(!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS));
	frame_seq_nb++;
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

	PRINT(("RR SENT\n\r"));

}

#ifndef TINY_ROM
void simple_tx(void){

	// Write frame data to DW1000 and prepare transmission. See NOTE 3 below.
	dwt_writetxdata(sizeof(tx_msg), tx_msg, 0);
	dwt_writetxfctrl(sizeof(tx_msg), 0);

	// Start transmission.
	dwt_starttx(DWT_START_TX_IMMEDIATE);

	// Poll DW1000 until TX frame sent event set. See NOTE 4 below.
	// STATUS register is 5 bytes long but, as the event we are looking at is in the first byte of the register,
	// we can use this simplest API
	// function to access it.
	while(!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS));

	// Clear TX frame sent event.
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

	// Execute a delay between transmissions.
	deca_sleep(TX_DELAY_MS);

	// Increment the blink frame sequence number (modulo 256).
	tx_msg[BLINK_FRAME_SN_IDX]++;

	PRINT(("OK\n\r"));

}
#endif

#ifndef TINY_ROM
void simple_rx(){

	dwt_setrxtimeout(0); // No timeout on RX

	uint16 frame_len = 0;

	int i;

	// Clear local RX buffer to avoid having leftovers from previous receptions  This is not necessary but
	// is included here to aid reading the RX buffer.  This is a good place to put a breakpoint. Here
	// (after first time through the loop) the local status register will be set for last event and if
	// a good receive has happened the data buffer will have the data in it, and frame_len will be set
	// to the length of the RX frame.
	for(i = 0; i < FRAME_LEN_MAX; i++){
		rx_buffer[i] = 0;
	}

	// Activate reception immediately. See NOTE 2 below.
	dwt_rxenable(0);

	// Poll until a frame is properly received or an error/timeout occurs. See NOTE 3 below.  STATUS
	// register is 5 bytes long but, as the event we are looking at is in the first byte of the
	// register, we can use this simplest API function to access it.
	while(!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR)));

	if(status_reg & SYS_STATUS_RXFCG){

		// A frame has been received, copy it to our local buffer.
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
		if(frame_len <= FRAME_LEN_MAX){

			dwt_readrxdata(rx_buffer, frame_len, 0);

			#ifndef TINY_ROM
			print_data(rx_buffer, frame_len);
			#endif

		}

		// Clear good RX frame event in the DW1000 status register.
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

	} else {

		PRINT(("ERROR status: %08lx\n\r", status_reg));

		// Clear RX error events in the DW1000 status register.
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);

	}

	PRINT(("OK\n\r"));

}
#endif

void ds_twr_init(uint16 ancor_addr){

	// Set expected response's delay and timeout. See NOTE 4 and 5 below.  As this example only handles one
	// incoming frame with always the same delay and timeout, those values can be set here once for all.
	dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
	dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

	// Write frame data to DW1000 and prepare transmission. See NOTE 7 below.
	tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
	set_addresses(&tx_poll_msg[ADDRESS_IDX], pan_id, my_addr, ancor_addr);
	dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
	dwt_writetxfctrl(sizeof(tx_poll_msg), 0);

	// Start transmission, indicating that a response is expected so that reception is enabled automatically after the
	// frame is sent and the delay set by dwt_setrxaftertxdelay() has elapsed.
	dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

	// We assume that the transmission is achieved correctly, poll for reception of a frame or error/timeout. See NOTE 8 below.
	while(!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR)));

	// Increment frame sequence number after transmission of the poll message (modulo 256).
	frame_seq_nb++;

	if(status_reg & SYS_STATUS_RXFCG){

		uint32 frame_len;

		// Clear good RX frame event and TX frame sent in the DW1000 status register.
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG | SYS_STATUS_TXFRS);

		// A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;
		if(frame_len <= RX_BUF_LEN){
			dwt_readrxdata(rx_buffer, frame_len, 0);
		}

		//static uint8 rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x10, 0x02, 0, 0, 0, 0};
		bool match = true;
		if(
		// match FRAME TYPE
		rx_buffer[0] != 0x41
		|| rx_buffer[1] != 0x88
		// match PAN
		||  rx_buffer[3] != ((uint8)pan_id)
		||  rx_buffer[4] != ((uint8)(pan_id >> 8))
		// match Destination Adderss
		|| rx_buffer[5] != ((uint8)my_addr)
		|| rx_buffer[6] != ((uint8)(my_addr >> 8))
		// match Source Address
		|| rx_buffer[7] != ((uint8)ancor_addr)
		|| rx_buffer[8] != ((uint8)(ancor_addr >> 8))
		// match message type
		|| rx_buffer[9] != 0x10
		// match activity type
		|| rx_buffer[10] != 0x02
		){
			match = false;
			#ifndef TINY_ROM
			PRINT(("Match failed\n\r"));
			print_data(rx_buffer, frame_len);
			#endif

		}

		if(match){

			uint32 final_tx_time;

			uint64 poll_tx_ts, resp_rx_ts, final_tx_ts;

			// Retrieve poll transmission and response reception timestamp.
			poll_tx_ts = get_tx_timestamp_u64();
			resp_rx_ts = get_rx_timestamp_u64();

			// Compute final message transmission time. See NOTE 9 below.
			final_tx_time = (resp_rx_ts + (RESP_RX_TO_FINAL_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
			dwt_setdelayedtrxtime(final_tx_time);

			// Final TX timestamp is the transmission time we programmed plus the TX antenna delay.
			final_tx_ts = (((uint64)(final_tx_time & 0xFFFFFFFE)) << 8) + TX_ANT_DLY;

			// Write all timestamps in the final message. See NOTE 10 below.
			final_msg_set_ts(&tx_final_msg[FINAL_MSG_POLL_TX_TS_IDX], poll_tx_ts);
			final_msg_set_ts(&tx_final_msg[FINAL_MSG_RESP_RX_TS_IDX], resp_rx_ts);
			final_msg_set_ts(&tx_final_msg[FINAL_MSG_FINAL_TX_TS_IDX], final_tx_ts);

			// Write and send final message. See NOTE 7 below.
			tx_final_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
			set_addresses(&tx_final_msg[ADDRESS_IDX], pan_id, my_addr, ancor_addr);
			dwt_writetxdata(sizeof(tx_final_msg), tx_final_msg, 0);
			dwt_writetxfctrl(sizeof(tx_final_msg), 0);
			int status = dwt_starttx(DWT_START_TX_DELAYED);

			if(status == DWT_ERROR){
				PRINT(("Error in Delayed TX\n\r"));
				return;
			}


			// Poll DW1000 until TX frame sent event set. See NOTE 8 below.
			while(!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS));

			// Clear TXFRS event.
			dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

			// Increment frame sequence number after transmission of the final message (modulo 256).
			frame_seq_nb++;
		}

	} else {

		PRINT(("ERROR status: %08lx\n\r", status_reg));

		// Clear RX error events in the DW1000 status register.
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);

	}

	PRINT(("OK\n\r"));

}

void ds_twr_resp(uint64 * tof_dtu_e, uint16 * tag_addr_e){

	// Clear reception timeout to start next ranging process.
	dwt_setrxtimeout(0);

	// Activate reception immediately.
	dwt_rxenable(0);

	// Poll for reception of a frame or error/timeout. See NOTE 7 below.
	while(!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR)));

	if(status_reg & SYS_STATUS_RXFCG){

		uint32 frame_len;

		// Clear good RX frame event in the DW1000 status register.
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		// A frame has been received, read it into the local buffer.
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
		if(frame_len <= RX_BUFFER_LEN){
			dwt_readrxdata(rx_buffer, frame_len, 0);
		}

		//static uint8 rx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x21, 0, 0};
		bool match = true;
		if(
		// match FRAME TYPE
		rx_buffer[0] != 0x41
		|| rx_buffer[1] != 0x88
		// match PAN
		||  rx_buffer[3] != ((uint8)pan_id)
		||  rx_buffer[4] != ((uint8)(pan_id >> 8))
		// match Destination Adderss
		|| rx_buffer[5] != ((uint8)my_addr)
		|| rx_buffer[6] != ((uint8)(my_addr >> 8))
		// match message type
		|| rx_buffer[9] != 0x21
		){
			match = false;
			PRINT(("Match failed\n\r"));

		}

		if(match){

			uint16 tag_addr = (((uint16)rx_buffer[8]) << 8) + rx_buffer[7];
			(*tag_addr_e) = tag_addr;
			PRINT(("Tag Address: %04X\n\r", tag_addr));

			uint32 resp_tx_time;

			// Retrieve poll reception timestamp.
			poll_rx_ts = get_rx_timestamp_u64();

			// Set send time for response. See NOTE 8 below.
			resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
			dwt_setdelayedtrxtime(resp_tx_time);

			// Set expected delay and timeout for final message reception.
			dwt_setrxaftertxdelay(RESP_TX_TO_FINAL_RX_DLY_UUS);
			dwt_setrxtimeout(FINAL_RX_TIMEOUT_UUS);

			// Write and send the response message. See NOTE 9 below.
			tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
			set_addresses(&tx_resp_msg[ADDRESS_IDX], pan_id, my_addr, tag_addr);
			dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0);
			dwt_writetxfctrl(sizeof(tx_resp_msg), 0);
			int status = dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);

			if(status == DWT_ERROR){
				PRINT(("Error in Delayed TX\n\r"));
				return;
			}

			// We assume that the transmission is achieved correctly, now poll for reception of expected
			// "final" frame or error/timeout.  See NOTE 7 below.
			while(!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR)));

			// Increment frame sequence number after transmission of the response message (modulo 256).
			frame_seq_nb++;

			if(status_reg & SYS_STATUS_RXFCG){

				// Clear good RX frame event and TX frame sent in the DW1000 status register.
				dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG | SYS_STATUS_TXFRS);

				// A frame has been received, read it into the local buffer.
				frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;
				if(frame_len <= RX_BUF_LEN){
					dwt_readrxdata(rx_buffer, frame_len, 0);
				}

				//{0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

				match = true;
				if(
				// match FRAME TYPE
				rx_buffer[0] != 0x41
				|| rx_buffer[1] != 0x88
				// match PAN
				||  rx_buffer[3] != ((uint8)pan_id)
				||  rx_buffer[4] != ((uint8)(pan_id >> 8))
				// match Destination Adderss
				|| rx_buffer[5] != ((uint8)my_addr)
				|| rx_buffer[6] != ((uint8)(my_addr >> 8))
				// match Source Address
				|| rx_buffer[7] != ((uint8)tag_addr)
				|| rx_buffer[8] != ((uint8)(tag_addr >> 8))
				// match message type
				|| rx_buffer[9] != 0x23
				){
					match = false;
					PRINT(("Match failed\n\r"));

				}

				if(match){

					uint32 poll_tx_ts, resp_rx_ts, final_tx_ts;
					uint32 poll_rx_ts_32, resp_tx_ts_32, final_rx_ts_32;
					double Ra, Rb, Da, Db;
					int64 tof_dtu;

					// Retrieve response transmission and final reception timestamps.
					resp_tx_ts = get_tx_timestamp_u64();
					final_rx_ts = get_rx_timestamp_u64();

					// Get timestamps embedded in the final message.
					final_msg_get_ts(&rx_buffer[FINAL_MSG_POLL_TX_TS_IDX], &poll_tx_ts);
					final_msg_get_ts(&rx_buffer[FINAL_MSG_RESP_RX_TS_IDX], &resp_rx_ts);
					final_msg_get_ts(&rx_buffer[FINAL_MSG_FINAL_TX_TS_IDX], &final_tx_ts);

					// Compute time of flight. 32-bit subtractions give correct answers even if clock has wrapped. See NOTE 10 below.
					poll_rx_ts_32 = (uint32)poll_rx_ts;
					resp_tx_ts_32 = (uint32)resp_tx_ts;
					final_rx_ts_32 = (uint32)final_rx_ts;
					Ra = (double)(resp_rx_ts - poll_tx_ts);
					Rb = (double)(final_rx_ts_32 - resp_tx_ts_32);
					Da = (double)(final_tx_ts - resp_rx_ts);
					Db = (double)(resp_tx_ts_32 - poll_rx_ts_32);
					tof_dtu = (int64)((Ra * Rb - Da * Db) / (Ra + Rb + Da + Db));

					tof = tof_dtu * DWT_TIME_UNITS;
					distance = tof * SPEED_OF_LIGHT;

					// Display computed distance
					#ifndef TINY_ROM
					PRINT(("DIST: "));
					print_float(distance);
					PRINT(("m\n\r"));
					#endif

					(*tof_dtu_e) = tof_dtu;

				} else {

					// check if range response
					if(!check_range_resp(rx_buffer, frame_len)){

						// some other message
						#ifndef TINY_ROM
						PRINT(("Other 2:\n\r"));
						print_data(rx_buffer, frame_len);
						#endif

					}

				}

			} else {

				PRINT(("2nd RX ERROR status: %08lx\n\r", status_reg));

				// Clear RX error events in the DW1000 status register.
				dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);

			}

		} else {

			// check if range response
			if(!check_range_resp(rx_buffer, frame_len)){

				// Some other message
				#ifndef TINY_ROM
				PRINT(("Other 1:\n\r"));
				print_data(rx_buffer, frame_len);
				#endif

			}

		}

        } else {

		PRINT(("1st RX ERROR status: %08lx\n\r", status_reg));

		// Clear RX error events in the DW1000 status register.
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);

	}

	PRINT(("OK\n\r"));

}

static void set_addresses(uint8 *address_field, uint16 pid, uint16 source_addr, uint16 dest_addr){

	int i;
	for(i = 0; i < 2; i++){
		address_field[i] = (uint8) pid;
		pid >>= 8;
	}
	for(i = 0; i < 2; i++){
		address_field[i+2] = (uint8) dest_addr;
		dest_addr >>= 8;
	}
	for(i = 0; i < 2; i++){
		address_field[i+4] = (uint8) source_addr;
		source_addr >>= 8;
	}

}

#define TOF_DTU_LEN 4

static void set_tof_dtu(uint8 *tof_dtu_field, uint64 tof_dtu){

	tof_dtu_field[0] = (uint8)(tof_dtu>>24);
	tof_dtu_field[1] = (uint8)(tof_dtu>>16);
	tof_dtu_field[2] = (uint8)(tof_dtu>>8);
	tof_dtu_field[3] = (uint8)(tof_dtu);

}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_tx_timestamp_u64()
 *
 * @brief Get the TX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_tx_timestamp_u64(void){

	uint8 ts_tab[5];
	uint64 ts = 0;
	int i;
	dwt_readtxtimestamp(ts_tab);

	for(i = 4; i >= 0; i--){
		ts <<= 8;
		ts |= ts_tab[i];
	}

	return ts;

}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_timestamp_u64()
 *
 * @brief Get the RX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_rx_timestamp_u64(void){

	uint8 ts_tab[5];
	uint64 ts = 0;
	int i;
	dwt_readrxtimestamp(ts_tab);

	for(i = 4; i >= 0; i--){
		ts <<= 8;
		ts |= ts_tab[i];
	}

	return ts;

}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_set_ts()
 *
 * @brief Fill a given timestamp field in the final message with the given value. In the timestamp fields of the final
 *        message, the least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to fill
 *         ts  timestamp value
 *
 * @return none
 */
static void final_msg_set_ts(uint8 *ts_field, uint64 ts){

	int i;
	for(i = 0; i < FINAL_MSG_TS_LEN; i++){
		ts_field[i] = (uint8) ts;
		ts >>= 8;
	}

}


/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_get_ts()
 *
 * @brief Read a given timestamp value from the final message. In the timestamp fields of the final message, the least
 *        significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to read
 *         ts  timestamp value
 *
 * @return none
 */
static void final_msg_get_ts(const uint8 *ts_field, uint32 *ts){

	int i;
	*ts = 0;
	for(i = 0; i < FINAL_MSG_TS_LEN; i++){
		*ts += ts_field[i] << (i * 8);
	}

}
