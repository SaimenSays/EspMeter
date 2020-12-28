#include "FreeRTOS.h"
#include "task.h"
#include <semphr.h>
#include <stdio.h>
#include <unistd.h>
#include <espressif/sdk_private.h>
#include <math.h>

#include <sml/sml_file.h>
#include <sml/sml_value.h>
#include <libsml/examples/unit.h>

#include "sml_server.h"
#include "mqtt.h"
#include "buffer.h"
#ifdef SML_DEBUG
	#include "debug.h"
#endif


//*****************************************************************************
// Definitions coppied from sml_transport.c
//*****************************************************************************

#define MC_SML_BUFFER_LEN 8096

unsigned char esc_seq[] = {0x1b, 0x1b, 0x1b, 0x1b};
unsigned char start_seq[] = {0x1b, 0x1b, 0x1b, 0x1b, 0x01, 0x01, 0x01, 0x01};
unsigned char end_seq[] = {0x1b, 0x1b, 0x1b, 0x1b, 0x1a};


//*****************************************************************************
// Local variables and definitions
//*****************************************************************************

xTaskHandle uart_task_handle = NULL;
static xSemaphoreHandle uart_sem = NULL;
	
#define UART0 						0
#define UART0_RX_SIZE  		128 // ESP8266 UART HW FIFO size

unsigned char rx_buffer[MC_SML_BUFFER_LEN];
#ifdef SML_DEBUG
	char hex_buffer[DEBUG_STRING_LEN];
#else	
#endif



//*****************************************************************************
// Local function prototypes
//*****************************************************************************

static void uart_task( void *pvParameters );
static void uart_rx_init( void );
#ifdef SML_DEBUG
	#define sml_debug_print(fmt, ...)			debug_print(fmt, ##__VA_ARGS__)
#else	
	#define sml_debug_print(fmt, ...)
#endif



//*****************************************************************************
// Function code
//*****************************************************************************

bool sml_server_init( void )
{
	xTaskCreate( uart_task, 
	             "uart_task", 
							 UART_TASK_STACK, 
							 NULL, 
							 UART_TASK_PRIORITY, 
							 &uart_task_handle );
	
	uart_rx_init();
	return true;
}



// Adopted from example sml_server.c
void sml_transport_receiver(unsigned char *buffer, size_t buffer_len)
{
	sml_file* file;
	int i, n;
	char* value_str;
	const char *unit_str = NULL;
	char obis_str[20];

	#ifdef SML_DEBUG
		sml_debug_print("%s: File:\n", __FUNCTION__);
		i = 0;
		for (n=0; n<buffer_len; n++)
		{
			sprintf(&hex_buffer[i], "%02X", buffer[n]);
			i += 2;
			if (i >= (DEBUG_STRING_LEN-2))
			{
				sml_debug_print(hex_buffer);
				i = 0; hex_buffer[0] = '\0';
			}
		}
		sml_debug_print("%s\n", hex_buffer);
	#endif
	
	sml_debug_print("%s: Parsing %d bytes  ... \n", __FUNCTION__, buffer_len);
	// the buffer contains the whole message, with transport escape sequences.
	// these escape sequences are stripped here.
	file = sml_file_parse(buffer + 8, buffer_len - 16);
	// the sml file is parsed now
	sml_debug_print("%s: %d messages found\n", __FUNCTION__, file->messages_len);
	
	// this prints some information about the file
	#ifdef SML_DEBUG
	//	sml_file_print(file);
	#endif

	for (i = 0; i < file->messages_len; i++)
	{
		sml_message *message = file->messages[i];
		sml_debug_print("Message %d: tag=%d\n", i, *message->message_body->tag);

		if (*message->message_body->tag == SML_MESSAGE_OPEN_RESPONSE)
		{
			sml_open_response* open = (sml_open_response*) message->message_body->data;
		
			sml_debug_print("time %d, verison %d\n", open->ref_time->data.timestamp, open->sml_version);
			
		}
		else if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE)
		{
			sml_list *entry;
			sml_get_list_response *body = (sml_get_list_response*) message->message_body->data;
			
			for (entry = body->val_list; entry != NULL; entry = entry->next)
			{
				if (!entry->value)
				{ // do not crash on null value
					sml_debug_print( "%s: Error in data stream. entry->value should not be NULL. Skipping this.\n", __FUNCTION__);
					continue;
				}
				
				snprintf(obis_str, sizeof(obis_str), "%d-%d:%d.%d.%d*%d",
					entry->obj_name->str[0], entry->obj_name->str[1],
					entry->obj_name->str[2], entry->obj_name->str[3],
					entry->obj_name->str[4], entry->obj_name->str[5]);
				obis_str[sizeof(obis_str)-1] = '\0';		// Ensure string termination when snprintf fails

				if (entry->value->type == SML_TYPE_OCTET_STRING)
				{
					sml_value_to_strhex(entry->value, &value_str, true);

					sml_debug_print("%s <str> %s\n", obis_str, value_str);
					mqtt_pub(obis_str, "{\"value\":\"%s\"}", value_str);
					
					free(value_str);
				}
				else if (entry->value->type == SML_TYPE_BOOLEAN)
				{
					sml_debug_print("%s <bool> %s\n", obis_str, (entry->value->data.boolean?"true":"false"));
					mqtt_pub(obis_str, "{\"value\":%s}", (entry->value->data.boolean?"true":"false"));
				}
				else if (((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_INTEGER) ||
				         ((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_UNSIGNED))
				{
					double value = sml_value_to_double(entry->value);
					int scaler = (entry->scaler) ? *entry->scaler : 0;
					int prec = -scaler;
					if (prec < 0)	prec = 0;
					value = value * pow(10, scaler);
					
					if (entry->unit)  												// do not crash on null (unit is optional)
					{
						unit_str = dlms_get_unit((unsigned char) *entry->unit);
					}
	
					sml_debug_print("%s <value> %.*f %s\n", obis_str, prec, value, unit_str?unit_str:"");
					mqtt_pub(obis_str, "{\"value\":%.*f,\"unit\":\"%s\"}", prec, value, unit_str?unit_str:"");
				}
				else
				{
					sml_debug_print("%s: Unhandled type (%d)\n", __FUNCTION__, entry->value->type);
				}
			}
		}
	}

	// free the malloc'd memory
	sml_file_free(file);
}


size_t sml_read(unsigned char *buffer, size_t len)
{
	size_t n;
	unsigned char byte;
	#ifdef SML_DEBUG
		char debug_str[10];
		char debug_buf[sizeof(debug_str)*3];
	#endif
	
	for (n=0; n<len; n++)
	{
		if (!(UART(UART0).STATUS & (UART_STATUS_RXFIFO_COUNT_M << UART_STATUS_RXFIFO_COUNT_S))) 
		{
			_xt_isr_unmask(1 << INUM_UART);
			if (!xSemaphoreTake(uart_sem, portMAX_DELAY)) 
			{
				sml_debug_print("%s: Failed to get semaphore! Exiting uart task\n", __FUNCTION__);
				vTaskDelete(NULL);
			}
		}
		byte = UART(UART0).FIFO & (UART_FIFO_DATA_M << UART_FIFO_DATA_S);
		buffer[n] = byte;
		#ifdef SML_DEBUG
			if (n < (sizeof(debug_str)-1))
			{
				sprintf(&debug_str[n], "%c", ((byte>31)?byte:' '));				
				sprintf(&debug_buf[n*3], "%02x ", byte);
			}
		#endif
	}
	
//	sml_debug_print("%s: Received '%s' %s\n", __FUNCTION__, debug_str, debug_buf);
	return n;
}



// Adopted from sml_transport.c
size_t sml_transport_read(unsigned char *buf, size_t max_len) 
{
	unsigned int len = 0;

	memset(buf, 0, max_len);

	if (max_len < 8) {
		// prevent buffer overflow
		sml_debug_print("%s: Can't read, buffer too small!\n", __FUNCTION__);
		return 0;
	}

	while (len < 8) {
		if (sml_read(&buf[len], 1) == 0)
		{
			sml_debug_print("%s: Read failed\n", __FUNCTION__);
			return 0;
		}

		if ((buf[len] == 0x1b && len < 4) || (buf[len] == 0x01 && len >= 4)) {
			len++;
		} else {
			len = 0;
		}
	}

	sml_debug_print("%s: Found start sequence\n", __FUNCTION__);
	
	// found start sequence
	while ((len + 8) < max_len)
	{
		if (sml_read(&buf[len], 4) == 0)
		{
			sml_debug_print("%s: Read failed\n", __FUNCTION__);
			return 0;
		}

		if (memcmp(&buf[len], esc_seq, 4) == 0)
		{
			// found esc sequence
			len += 4;
			if (sml_read(&buf[len], 4) == 0)
			{
				sml_debug_print("%s: Failed to read\n", __FUNCTION__);
				return 0;
			}

			if (buf[len] == 0x1a)
			{
				sml_debug_print("%s: Found end sequence\n", __FUNCTION__);
				// found end sequence
				len += 4;
				return len;
			} else {
				// don't read other escaped sequences yet
				sml_debug_print("%s: Unrecognized sequence\n", __FUNCTION__);
				return 0;
			}
		}
		len += 4;
	}

	sml_debug_print("%s: Message to long for buffer (%d)\n", __FUNCTION__, max_len);
	return 0;
}



// Adopted from sml_transport.c, function sml_transport_listen()
static void uart_task( void *pvParameters )
{
	size_t bytes;

	while (true)
	{
		bytes = sml_transport_read(rx_buffer, MC_SML_BUFFER_LEN);
		if (bytes > 0)
		{	
			sml_transport_receiver(rx_buffer, bytes);
		}
	}
}



// Following code is copied from 'extras/stdin_uart_interrupt/stdin_uart_interrupt.c'.
// It needs to be copied here for faster implementation. Using functions from there is not possible because of inaccessible static variables 
// Extra component is not needed therfore anymore.

IRAM void uart0_rx_handler(void *arg)
{
	if (!UART(UART0).INT_STATUS & UART_INT_STATUS_RXFIFO_FULL) 
	{
			return;
	}
	if (UART(UART0).INT_STATUS & UART_INT_STATUS_RXFIFO_FULL) 
	{
		UART(UART0).INT_CLEAR = UART_INT_CLEAR_RXFIFO_FULL;
		if (UART(UART0).STATUS & (UART_STATUS_RXFIFO_COUNT_M << UART_STATUS_RXFIFO_COUNT_S)) 
		{
			long int xHigherPriorityTaskWoken;
			_xt_isr_mask(1 << INUM_UART);
			_xt_clear_ints(1<<INUM_UART);
			xSemaphoreGiveFromISR(uart_sem, &xHigherPriorityTaskWoken);
			if(xHigherPriorityTaskWoken) portYIELD();
		}
	} 
	else 
	{
		sml_debug_print("%s: Unexpected uart irq, INT_STATUS 0x%02x\n", __FUNCTION__, UART(UART0).INT_STATUS);
	}
}


static void uart_rx_init(void)
{
	int trig_lvl = 1;
	uart_sem = xSemaphoreCreateCounting(UART0_RX_SIZE, 0);

	_xt_isr_attach(INUM_UART, uart0_rx_handler, NULL);
	_xt_isr_unmask(1 << INUM_UART);

	// reset the rx fifo
	uint32_t conf = UART(UART0).CONF0;
	UART(UART0).CONF0 = conf | UART_CONF0_RXFIFO_RESET;
	UART(UART0).CONF0 = conf & ~UART_CONF0_RXFIFO_RESET;

	// set rx fifo trigger
	UART(UART0).CONF1 |= (trig_lvl & UART_CONF1_RXFIFO_FULL_THRESHOLD_M) << UART_CONF1_RXFIFO_FULL_THRESHOLD_S;

	// clear all interrupts
	UART(UART0).INT_CLEAR = 0x1ff;

	// enable rx_interrupt
	UART(UART0).INT_ENABLE = UART_INT_ENABLE_RXFIFO_FULL;
}
