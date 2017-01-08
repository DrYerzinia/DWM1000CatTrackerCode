/*! ----------------------------------------------------------------------------
 * @file	deca_mutex.c
 * @brief	IRQ interface / mutex implementation
 *
 * @attention
 *
 * Copyright 2013 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 */

#include <asf.h>

#include "deca_device_api.h"

#if (__SAMD20E17__)

#define DECA_PORT_PIN_IRQ	PIN_PA19
#define DECA_IRQ_PIN		PIN_PA19A_EIC_EXTINT3
#define DECA_IRQ_MUX		MUX_PA19A_EIC_EXTINT3

#elif (__SAMD11D14AM__)

#define DECA_PORT_PIN_IRQ	PIN_PA09
#define DECA_IRQ_PIN		PIN_PA09A_EIC_EXTINT7
#define DECA_IRQ_MUX		MUX_PA09A_EIC_EXTINT7

#endif

struct extint_chan_conf config_extint_chan;

static void configure_extint_channel(void){

	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	port_pin_set_config(DECA_PORT_PIN_IRQ, &pin_conf);

	extint_chan_get_config_defaults(&config_extint_chan);
	config_extint_chan.gpio_pin = DECA_IRQ_PIN;
	config_extint_chan.gpio_pin_mux = DECA_IRQ_MUX;
	config_extint_chan.gpio_pin_pull = EXTINT_PULL_NONE;
	config_extint_chan.detection_criteria = EXTINT_DETECT_LOW;
	extint_chan_set_config(0, &config_extint_chan);

}

static void extint_detection_callback(void){

	//

}

static void configure_extint_callbacks(void){

	extint_register_callback(extint_detection_callback, 0, EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_enable_callback(0, EXTINT_CALLBACK_TYPE_DETECT);

}

void mutex_enable(void){

	configure_extint_channel();
	configure_extint_callbacks();

}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: decamutexon()
 *
 * Description: This function should disable interrupts. This is called at the start of a critical section
 * It returns the irq state before disable, this value is used to re-enable in decamutexoff call
 *
 * Note: The body of this function is defined in deca_mutex.c and is platform specific
 *
 * input parameters:	
 *
 * output parameters
 *
 * returns the state of the DW1000 interrupt
 */
decaIrqStatus_t decamutexon(void){

    decaIrqStatus_t s = port_pin_get_input_level(DECA_PORT_PIN_IRQ);

    if(s){
        extint_chan_disable_callback(0, EXTINT_CALLBACK_TYPE_DETECT);
    }

    return s; // return state before disable, value is used to re-enable in decamutexoff call

return 0;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: decamutexoff()
 *
 * Description: This function should re-enable interrupts, or at least restore their state as returned(&saved) by decamutexon 
 * This is called at the end of a critical section
 *
 * Note: The body of this function is defined in deca_mutex.c and is platform specific
 *
 * input parameters:	
 * @param s - the state of the DW1000 interrupt as returned by decamutexon
 *
 * output parameters
 *
 * returns the state of the DW1000 interrupt
 */
void decamutexoff(decaIrqStatus_t s){        // put a function here that re-enables the interrupt at the end of the critical section

    if(s){ //need to check the port state as we can't use level sensitive interrupt on the STM ARM
        extint_chan_enable_callback(0, EXTINT_CALLBACK_TYPE_DETECT);
    }

}
