#include "FreeRTOS.h"
#include "task.h"
#include "esp/gpio.h"
#include "esp/wdev_regs.h"
#include <string.h>
#include "light.h"
#ifdef LIGHT_DEBUG
	#include "debug.h"
#endif



//*****************************************************************************
// Config
//*****************************************************************************
#define LIGHT_PULSE_SHORT						120		// 80ms some inputs lost. 100ms seem to work. Use 120ms to be sure
#define LIGHT_PULSE_CONFIRM					5500
#define LIGHT_PULSE_PIN_WAIT				3500
#define LIGHT_PULSE_START						5000
#define LIGHT_DISPLAY_TEST_TIMEOUT  120000



//*****************************************************************************
// Local variables and definitions
//*****************************************************************************

xTaskHandle light_task_handle = NULL;
uint8_t Pin[4];
uint32_t display_test_timeout = 0;


typedef enum
{
	LIGHT_TYPE_INFO,
	LIGHT_TYPE_PIN,
	LIGHT_TYPE_COUNT
} light_type_t;

const char* const light_type_str[LIGHT_TYPE_COUNT] =
{
	[LIGHT_TYPE_INFO] = "info",
	[LIGHT_TYPE_PIN] = "pin"
};
	


//*****************************************************************************
// Local function prototypes
//*****************************************************************************

static void light_pulse(uint32_t on, uint32_t off, uint8_t count);
void light_task( void *pvParameters );
#ifdef LIGHT_DEBUG
	#define light_debug_print(fmt, ...)			debug_print(fmt, ##__VA_ARGS__)
#else	
	#define light_debug_print(fmt, ...)
#endif



//*****************************************************************************
// Function code
//*****************************************************************************

bool light_init(void)
{
	gpio_write(LIGHT_PIN, true);	// inverted logic
	gpio_enable(LIGHT_PIN, GPIO_OUTPUT);
	gpio_set_iomux_function(LIGHT_PIN, LIGHT_IOMUX);
	
	return true;
}


void light_start( const char* str, size_t len )
{
	light_type_t type;
	uint8_t n;
	
	// Avoid to mess up different requests
	if (light_task_handle != NULL)
	{
		light_debug_print("%s: Old task active\n", __FUNCTION__);
		return;
	}
	
	for (type=0; type<LIGHT_TYPE_COUNT; type++)
	{
		n = strlen(light_type_str[type]);
		if (n <= len) 
		{
			if (strncmp(str, light_type_str[type], n) == 0) break;
		}
	}
	if (type >= LIGHT_TYPE_COUNT)
	{
		light_debug_print("%s: Unkown command '%.*s'\n", __FUNCTION__, len, str);
		return;
	}	
	
	if (type == LIGHT_TYPE_PIN)
	{
		if (len != 8)
		{
			light_debug_print("%s: Invalid pin command '%.*s'\n", __FUNCTION__, len, str);
			return;
		}
		
		for (n=0; n<4; n++)
		{
			if ((str[n+4] < '0') || (str[n+4] > '9'))
			{
				light_debug_print("%s: Invalid pin command '%.*s'\n", __FUNCTION__, len, str);
				return;
			}
			Pin[n] = str[n+4] - '0';
		}
	}
	
	light_debug_print("%s: Starting %s\n", __FUNCTION__, light_type_str[type]);
	xTaskCreate( light_task, 
	             "light_task", 
							 LIGHT_TASK_STACK, 
							 (void*)type, 
							 LIGHT_TASK_PRIORITY, 
							 &light_task_handle );
	
	if (light_task_handle == NULL)
	{
		light_debug_print("%s: Failed to create task\n", __FUNCTION__);
	}	
}



static void light_pulse(uint32_t on, uint32_t off, uint8_t count)
{
	uint8_t n;
	
	for (n=0; n<count; n++)
	{
		gpio_write(LIGHT_PIN, false);	// inverted logic
		vTaskDelay(on / portTICK_PERIOD_MS);
		gpio_write(LIGHT_PIN, true);	// inverted logic
		vTaskDelay(off / portTICK_PERIOD_MS);
	}
}


void light_task( void *pvParameters )
{
	light_type_t type = (light_type_t)pvParameters;
	uint32_t time;
	uint8_t n;

	// Display test is done on first pulse, when longer time no input
	time = WDEV.SYS_TIME;
	if ((display_test_timeout == 0) || (time > display_test_timeout))
	{
		light_pulse(LIGHT_PULSE_SHORT, LIGHT_PULSE_START, 1);
	}
	// Reset timeout
	display_test_timeout = time + LIGHT_DISPLAY_TEST_TIMEOUT;
	
	switch (type)
	{
		case LIGHT_TYPE_INFO:
			light_pulse(LIGHT_PULSE_SHORT, LIGHT_PULSE_SHORT, 9);
			light_pulse(LIGHT_PULSE_CONFIRM, 10000, 1);
			break;
			
		case LIGHT_TYPE_PIN:
			for (n=0; n<4; n++)
			{
				light_pulse(LIGHT_PULSE_SHORT, LIGHT_PULSE_SHORT, Pin[n] );
				vTaskDelay(LIGHT_PULSE_PIN_WAIT / portTICK_PERIOD_MS);
			}
			break;
		
		default:
			light_debug_print("%s: Invalid type\n", __FUNCTION__);
			return;
	}

	light_debug_print("%s: Finished output\n", __FUNCTION__);
	vTaskDelay(10);
	light_task_handle = NULL;
	vTaskDelete( NULL );		// remove this task
}
