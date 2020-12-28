#include "FreeRTOS.h"
#include "task.h"
#include "espressif/esp_common.h"
#include "espressif/user_interface.h"
#include "lwip/netif.h"
#include "esplibs/libmain.h"
#include "esplibs/libnet80211.h"

#include "wifi.h"
#include "string.h"
#include "sdk_internal.h"
#include "mqtt.h"
#ifdef WIFI_DEBUG
	#include "debug.h"
#else
	// According to https://github.com/SuperHouse/esp-open-rtos/issues/487
	// redefining this function disables unwanted prints on uart.
	int ets_printf(const char *format, ...)
	{
		return 0;
	}
#endif



//*****************************************************************************
// Data structures
//*****************************************************************************
typedef struct sdk_softap_config softap_config_t;
typedef struct sdk_station_config station_config_t;
typedef struct ip_info ip_info_t;
typedef struct sdk_bss_info bss_info_t;

typedef struct wifi_list_t
{
	char payload[30];
	struct wifi_list_t* next;
} wifi_list_t;


// According to enum in 'include/espressif/esp_wifi.h'
static const char* const wifi_mode_str[] = 
{
	[NULL_MODE]				= "",
	[STATION_MODE]		= "STATION",
	[SOFTAP_MODE]			= "SOFTAP",
	[STATIONAP_MODE]	= "STATIONAP",
	[MAX_MODE]				= "MAX"
};

#define WIFI_MODE_STR_LEN		strlen(wifi_mode_str[STATIONAP_MODE])

// According to enum in 'include/espressif/esp_wifi.h'
static const char* const wifi_connect_status_str[] = 
{
    [STATION_IDLE] 						= "Idle",
    [STATION_CONNECTING]			= "Connecting",
    [STATION_WRONG_PASSWORD]	= "Wrong password",
    [STATION_NO_AP_FOUND]			= "No AP found",
    [STATION_CONNECT_FAIL]		= "Connect fail",
    [STATION_GOT_IP]					= "Got IP"
};

static const char* const wifi_auth_modes_str[] =
{
	[AUTH_OPEN]         = "Open",
	[AUTH_WEP]          = "WEP",
	[AUTH_WPA_PSK]      = "WPA/PSK",
	[AUTH_WPA2_PSK]     = "WPA2/PSK",
	[AUTH_WPA_WPA2_PSK] = "WPA/WPA2/PSK"
};

#define ELEMS(x)			(sizeof(x) / sizeof(x[0]))


wifi_init_callback_t wifi_init_callback = NULL;
xTaskHandle wifi_init_task_handle = NULL;
bool wifi_start_connection = false;
wifi_list_t* wifi_station_list = NULL;



//*****************************************************************************
// Local function prototype
//*****************************************************************************
static void wifi_init_task( void *pvParameters );
void wifi_print_station_status( uint8_t state );
void wifi_scan_done_callback( void *arg, sdk_scan_status_t status );
static inline bool wifi_failed( bool ret );
static bool wifi_config_station( void );

#ifdef WIFI_DEBUG
	#define wifi_debug_print(fmt, ...)			printf(fmt, ##__VA_ARGS__)
#else	
	#define wifi_debug_print(fmt, ...)
#endif



//*****************************************************************************
// Function code
//*****************************************************************************

bool wifi_init( wifi_init_callback_t init_callback )
{
	int16_t		ret;
	
	// Required to call wifi_set_opmode before station_set_config
	ret = sdk_wifi_set_opmode( STATION_MODE );
	if( wifi_failed(ret) ) 
	{
		wifi_debug_print( "%s: Failed to set opmode\n", __FUNCTION__ );		
		return false;
	}
	
	ret = wifi_config_station();
	if( ret == false ) return false;
	
/*	ret = sdk_wifi_station_start();
	if( wifi_failed(ret) ) 
	{
		wifi_debug_print( "%s: Failed to connect\n", __FUNCTION__ );		
	}
*/	
	// Following functions are done in app_main.c, which calls our user_init()
	#if 0
		wifi_debug_print( "%s: Setting mode: ", __FUNCTION__ );		
		sdk_wifi_mode_set( opmode );

		wifi_debug_print( "%s: Starting wifi station\n", __FUNCTION__ );		
		ret = sdk_wifi_station_start();
		if( ret == false ) 
		{
			wifi_debug_print( "%s: Failed to start station\n", __FUNCTION__ );		
			return false;
		}
		
		//wifi_debug_print( "%s: Setting defaults\n", __FUNCTION__ );		
		//netif_set_default(sdk_g_ic.v.station_netif_info->netif);
	#endif

	if( init_callback != NULL )
	{
		wifi_init_callback = init_callback;
		
		xTaskCreate( wifi_init_task, "wifi_init_task", WIFI_INIT_TASK_STACK, NULL, WIFI_INIT_TASK_PRIORITY, &wifi_init_task_handle );
		if( wifi_init_task_handle == NULL ) 
		{
			wifi_debug_print( "%s: Error creating wifi init task\n", __FUNCTION__);
			return false;
		}
	}
	
	return true;
}


static bool wifi_config_station( void )
{
	station_config_t	config;
	int16_t						ret;

	// bssid_set must be set to zero, otherwise it will check routers mac, and won't connect!
	// We clear complete struct to avoid undefined behaviour
	memset( &config, 0x0, sizeof(config) );	
 			
	strcpy((char*)config.ssid, WIFI_SSID);
	strcpy((char*)config.password, WIFI_PASS);
	
	wifi_debug_print( "%s: Setting config ... ", __FUNCTION__ );		
	ret = sdk_wifi_station_set_config( &config );
	if( wifi_failed(ret) ) return false;
	
	return true;
}



static void wifi_init_task( void *pvParameters )
{
	int16_t		timeout = WIFI_INIT_TIMEOUT;
	int16_t		retry = 10;
	uint8_t		state = STATION_IDLE;
	bool			initialized = false;
	bool			scan_done = false; 
	
	while (true)
	{ 
		if( scan_done == false )
		{
			wifi_start_connection = false;
			scan_done = sdk_wifi_station_scan( NULL, &wifi_scan_done_callback );
		}
		state = sdk_wifi_station_get_connect_status();

		if( (timeout <= 0) || (state == STATION_CONNECT_FAIL) )
		{
			wifi_debug_print( "%s: Timeout or connection fail\n", __FUNCTION__ );
			// Rescan available stations, then connect
			sdk_wifi_station_connect();
			scan_done = false;
			timeout = WIFI_INIT_TIMEOUT;
			retry --;
			if( retry <= 0 )
			{
				sdk_system_restart();
			}
		}
		else if( (state == STATION_GOT_IP) && (wifi_start_connection == true) )
		{
			wifi_print_station_status( state );
	
			if( initialized == false )
			{
				wifi_debug_print( "%s: Wifi is up -> Initing wifi tasks ...\n", __FUNCTION__);
				vTaskDelay( 1000 / portTICK_RATE_MS );
				wifi_init_callback();
				#ifdef WIFI_SLEEP
					bool ret;
					uint8_t opmode = sdk_wifi_get_opmode();
					if( opmode == STATION_MODE )
					{
						wifi_debug_print( "%s: Setting automatic sleep mode (%d)\n", __FUNCTION__, WIFI_SLEEP );
						ret = sdk_wifi_set_sleep_type( WIFI_SLEEP );
						if( ret == false ) wifi_debug_print( "%s: Failed to set sleep mode\n", __FUNCTION__ );
					}
				#endif

				initialized = true;
			}
			
			do 
			{
				vTaskDelay( ((int32_t)WIFI_INIT_DELAY * 1000) / portTICK_RATE_MS );
				state = sdk_wifi_station_get_connect_status();
			} while (state == STATION_GOT_IP);

			// Go to reconnect in next cycle
			timeout = true;
		}
		else
		{
			wifi_debug_print( "%s: Waiting %ds for wifi to come up ...\n", __FUNCTION__, timeout);
			timeout -= WIFI_INIT_DELAY;
		}
		
		vTaskDelay( ((int32_t)WIFI_INIT_DELAY * 1000) / portTICK_RATE_MS );
	}
}



void wifi_print_station_status( uint8_t state )
{
	#ifdef WIFI_DEBUG
		if( state >= sizeof(wifi_connect_status_str) ) wifi_debug_print( "%s: Invalid search state (%d)\n", __FUNCTION__, state );
		else wifi_debug_print( "%s: %s\n", __FUNCTION__, wifi_connect_status_str[state] );
	#endif
}



// Copied from examples/wifi_scan/main.c
void wifi_scan_done_callback( void *arg, sdk_scan_status_t status )
{
	bss_info_t* bss;
	wifi_list_t* list = NULL;

	if( status != SCAN_OK )
	{
		#ifdef WIFI_DEBUG
			wifi_debug_print( "%s: Wifi scan failed\n", __FUNCTION__ );
		#else
			printf( "Wifi scan failed\n" );
		#endif
		return;
	}

	// Clear if an old list exists
	list = wifi_station_list;
	while( list != NULL )
	{
		list = wifi_station_list->next;
		vPortFree( wifi_station_list );
		wifi_station_list = list;
	}
	wifi_station_list = NULL;
	
	bss = (struct sdk_bss_info*)arg;
	// first one is invalid
	bss = bss->next.stqe_next;

	if( bss == NULL )
	{
		wifi_debug_print( "%s: No stations found\n", __FUNCTION__ );		
	}
	else
	{
		wifi_debug_print( "%s: Found stations:\n", __FUNCTION__ );		
		wifi_station_list = pvPortMalloc(sizeof(wifi_list_t));
		list = wifi_station_list;
		while(true)
		{
			if( list == NULL )
			{
				wifi_debug_print( "%s: Error creating list\n", __FUNCTION__ );		
				break;
			}
			snprintf( list->payload, 30, "%.20s,%d,%d", bss->ssid, bss->channel, bss->rssi );
			wifi_debug_print( DEBUG_INDENT "%s (" MACSTR "), Ch: %d, RSSI: %02d, security: %s\n", bss->ssid, MAC2STR(bss->bssid), bss->channel, bss->rssi, wifi_auth_modes_str[bss->authmode]);

			bss = bss->next.stqe_next;
			if( bss == NULL )
			{
				list->next = NULL;
				break;
			}
			
			list->next = pvPortMalloc(sizeof(wifi_list_t));
			list = list->next;
		}
	}
		
	wifi_start_connection = true;
}



void wifi_pub_stations( void )
{
	wifi_list_t* list = wifi_station_list;
	char topic[20];
	uint8_t n = 0;
	bool ret;

	if ( list == NULL )
	{
		mqtt_pub( "Error", "Stations" );
		return;
	}
	
	while( list != NULL )
	{
		snprintf( topic, sizeof(topic), "WifiStation/%d", n );
		ret = mqtt_pub( topic, list->payload );
		if( ret == false ) return;
		
		list = list->next;
		n++;
	}
	snprintf( topic, sizeof(topic), "WifiStation/%d", n );
	mqtt_pub( topic, "-" );
}



static inline bool wifi_failed( bool ret )
{
	if( ret == false )
	{
		wifi_debug_print( "Failed\n" );		
		return true;
	}
	else
	{
		wifi_debug_print( "Done\n" );	
		return false;
	}
}