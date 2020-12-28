#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <espressif/esp_common.h>
#include "FreeRTOS.h"
#include "task.h"
#include <timers.h>
#include <esp/uart.h>
#include "lwip/init.h"
#include "wifi.h"
#include "mqtt.h"
#include "debug.h"
#include "rboot-ota/ota-tftp.h"
#include "sml_server.h"
#include "light.h"

//*****************************************************************************
// Configuration
//*****************************************************************************

#define DEBUG


//*****************************************************************************
// Local function prototypes
//*****************************************************************************

static void wifi_init_callback( void );

#ifdef DEBUG
	#define main_debug_print(fmt, ...)			debug_print(fmt, ##__VA_ARGS__)
#else	
	#define main_debug_print(fmt, ...)
#endif



//*****************************************************************************
// Function code
//*****************************************************************************
static void wifi_init_callback( void )
{
	bool success;
	
	debug_print( "%s: *** Init wifi ***\n", __FUNCTION__ );

	main_debug_print( "%s: Init WIFI debug\n", __FUNCTION__ );
	success = debug_wifi_init();
	if (success == false)
	{	
		main_debug_print( "%s: *** WIFI debug init failed ***\n", __FUNCTION__ );
	}
	
	main_debug_print( "%s: Init TFTP server\n", __FUNCTION__ );
	success = ota_tftp_init_server();
	if (success == false)
	{	
		main_debug_print( "%s: *** TFTP server init failed ***\n", __FUNCTION__ );
	}
	
	main_debug_print( "%s: Init MQTT\n", __FUNCTION__ );
	success = mqtt_init();
	if (success == false)
	{	
		main_debug_print( "%s: *** Mqtt init failed ***\n", __FUNCTION__ );
	}
}	
	

enum sleep_type {
    NONE_SLEEP_T    = 0,
    LIGHT_SLEEP_T,
    MODEM_SLEEP_T
};

void wifi_fpm_set_sleep_type(enum sleep_type type);
 
void user_init(void)
{
	bool success;
	
	// SML receiver needs 9600 baud, 7 data, even, 1 stop
	uart_set_baud(0, 9600);
	//uart_set_byte_length(0, UART_BYTELENGTH_8);
	//uart_set_parity(0, UART_PARITY_EVEN);
	//uart_set_parity_enabled(0, true);

	debug_init();

	// Wait some time to allow user to  connect and see initalization over uart debugging
	for( int16_t i=0; i<4000; i++) sdk_os_delay_us(1000);		
	
	#ifdef DEBUG
		debug_print( "\n\n\n" );		// Some new lines after boot to see restart	
		debug_print( "%s: Firmware build %s\n", __FUNCTION__, __DATE__ );
		debug_print( "%s: *** Starting user init ***\n", __FUNCTION__ );
		debug_print( "%s: Free heap memory: %d\n", __FUNCTION__, sdk_system_get_free_heap_size() );
		//stats_print_sysparams();
		ota_print_info();
	#endif	
		
	main_debug_print( "%s: *** Initializing WIFI ***\n", __FUNCTION__ );
	success = wifi_init(wifi_init_callback);
	if (success == false)
	{	
		main_debug_print( "%s: WIFI Init failed\n", __FUNCTION__ );
	}
		

	main_debug_print( "%s: *** Initializing SML handler ***\n", __FUNCTION__ );
	success = sml_server_init();
	if (success == false)
	{	
		main_debug_print( "%s: SML init failed\n", __FUNCTION__ );
	}

	main_debug_print( "%s: *** Light init ***\n", __FUNCTION__ );
	success = light_init();
	if (success == false)
	{	
		main_debug_print( "%s: Light init failed\n", __FUNCTION__ );
	}
	
	main_debug_print( "%s: User init finished\n", __FUNCTION__ );
	main_debug_print( "%s: Free heap memory: %d\n", __FUNCTION__, sdk_system_get_free_heap_size() );
}

