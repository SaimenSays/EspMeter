/*
 * mqtt.c
 *
 *  Created on: 06.11.2016
 *      Author: vieleicht_scho
 */
 
#include "mqtt.h"
#include "espressif/esp_common.h"
#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>
#include <semphr.h>
#include <stdarg.h>
#include <string.h>
#include "wifi.h"
//#include "watchdog.h"
#include "light.h"
#ifdef MQTT_DEBUG
	#include "debug.h"
#endif

	
//*****************************************************************************
// Data strutures
//*****************************************************************************

typedef struct
{
	uint8_t				Buf[100];
	uint8_t				ReadBuf[100];
	char					ClientId[20];
	int						Port;
	char					Host[20];
	xQueueHandle	PublishQueue;
	mqtt_client_t	Client;
} Mqtt_t;


Mqtt_t* Mqtt = NULL;
xTaskHandle mqtt_task_handle = NULL;



//*****************************************************************************
// Local function prototypes
//*****************************************************************************

static void light_message_received(mqtt_message_data_t *md);
static void watchdog_message_received(mqtt_message_data_t *md);
static void mqtt_task(void *pvParameters);
static char* mqtt_make_topic( const char* name );

#ifdef MQTT_DEBUG
	#define mqtt_debug_print(fmt, ...)			debug_print(fmt, ##__VA_ARGS__)
#else
	#define mqtt_debug_print(fmt, ...)
#endif


//*****************************************************************************
// Function code
//*****************************************************************************


bool mqtt_init( void ) 
{
	// Use MAC address for Station as unique ID, from example
  uint8_t				hwaddr[6];
	portBASE_TYPE	ret;
	
	Mqtt = pvPortMalloc(sizeof(Mqtt_t));
	if( Mqtt == NULL )
	{
		mqtt_debug_print( "%s: Error allocating RAM\n", __FUNCTION__ );
		return false;
	}
	memset( &Mqtt->Client, 0x00, sizeof(mqtt_client_t) );

	if ( !sdk_wifi_get_macaddr(STATION_IF, hwaddr) )
	{
		mqtt_debug_print( "%s: Error creating id\n", __FUNCTION__ );
		vPortFree( Mqtt );
		Mqtt = NULL;
		return false;
	}
	sprintf( Mqtt->ClientId, "%02X-%02X-%02X-%02X-%02X-%02X", MAC2STR(hwaddr) );
	mqtt_debug_print( "%s: Using id %s\n", __FUNCTION__, Mqtt->ClientId );

	Mqtt->PublishQueue = xQueueCreate(MQTT_PUBLISH_QUEUE_SIZE, sizeof(mqtt_msg*)); // queue transact by reference
	if( Mqtt->PublishQueue == NULL )
	{
		mqtt_debug_print( "%s: Error creating queue\n", __FUNCTION__ );		
		vPortFree( Mqtt );
		Mqtt = NULL;
		return false;
	}
	
	mqtt_debug_print( "%s: Creating task\n", __FUNCTION__ );
	ret = xTaskCreate( &mqtt_task, "mqtt", 500, NULL, 4, &mqtt_task_handle );
	if( (ret != pdPASS) || (mqtt_task_handle == NULL) )
	{
		vPortFree( Mqtt );
		Mqtt = NULL;
		mqtt_debug_print( "%s: Error creating task (%i) \n", __FUNCTION__, ret );
		return false;	
	}
	
	return true;
}



void mqtt_deinit( void )
{
	// Don't stop task to keep posiblity for soft reset via message
	// vTaskDelete( mqtt_task_handle );
	// mqtt_task_handle = NULL;
}



bool mqtt_is_connected( void )
{
	if( Mqtt == NULL ) return false;
	return (Mqtt->Client.isconnected == 1);
}



bool mqtt_pub( const char* topic, const char* format, ... ) 
{
	uint16_t	payload_len;
	va_list		arglist;
	
	if( (topic == NULL) || (strlen(topic) < 1) || (format == NULL) )
	{
		mqtt_debug_print( "%s: Invalid parameters\n", __FUNCTION__ );		
		return false;		
	}
	else if( (mqtt_task_handle == NULL) || (Mqtt == NULL) ) 
	{
		mqtt_debug_print( "%s: Should send message, but not initialized\n", __FUNCTION__ );		
		return false;
	}
	
	if( uxQueueSpacesAvailable(Mqtt->PublishQueue) == 0 )
	{
		mqtt_debug_print( "%s: No space in queue left\n", __FUNCTION__ );
		return false;		
	}
	
	mqtt_msg* msg = pvPortMalloc(sizeof(mqtt_msg));
	if( msg == NULL )
	{
		mqtt_debug_print( "%s: Not enough memory for mqtt msg\n", __FUNCTION__ );
		return false;
	}
	
	msg->topic = mqtt_make_topic( topic );
	if( msg->topic == NULL )
	{
		mqtt_debug_print( "%s: no enough memory for topic\n", __FUNCTION__ );
		vPortFree( msg );
		return false;
	}

	va_start( arglist, format );
	payload_len = vsnprintf( NULL, 0, format, arglist ); 	
	msg->payload = pvPortMalloc( payload_len+1 );
	if( msg->payload != NULL )
	{
		vsprintf( msg->payload, format, arglist ); 
		msg->payload_len = payload_len;
	}
	va_end( arglist );

	if( msg->payload == NULL )
	{
		mqtt_debug_print( "%s: no enough memory for payload\n", __FUNCTION__ );
		vPortFree( msg->topic );
		vPortFree( msg );
		return false;
	}
	
	mqtt_debug_print( "%s: Message to queue '%s'='%s'\n", __FUNCTION__, msg->topic, msg->payload );
	if( xQueueSend(Mqtt->PublishQueue, (void *)&msg, 0) == pdFALSE )
	{
		mqtt_debug_print( "%s: Queue overflow\n", __FUNCTION__ );
		vPortFree( msg->payload );
		vPortFree( msg->topic );
		vPortFree( msg );
		return false;
	}
	
	return true;
}



bool mqtt_reconnect( void )
{
	return (xQueueSend(Mqtt->PublishQueue, NULL, 0) == pdTRUE);
}



static char* mqtt_make_topic( const char* name )
{
	uint16_t	len;
	char*			str;
	
	if( (Mqtt == NULL) || (name == NULL) )
	{
		mqtt_debug_print( "%s: Invalid parameters\n", __FUNCTION__ );		
		return NULL;
	}

	len = snprintf( NULL, 0, "%s/%s", MQTT_TOPIC_MAIN, name );
	str = pvPortMalloc( len +1 );
	if( str == NULL )
	{
		mqtt_debug_print( "%s: Failed to allocate memory\n", __FUNCTION__ );		
		return NULL;
	}

	sprintf( str, "%s/%s", MQTT_TOPIC_MAIN, name );
	return str;
}



static void mqtt_task( void *pvParameters )
{
	int													ret = 0;
	struct mqtt_network 				network;
	mqtt_packet_connect_data_t	data = mqtt_packet_connect_data_initializer;
	char*												lwt_topic;
	char*												light_topic;
	char*												watchdog_topic;
	bool												reconnect;

	lwt_topic = mqtt_make_topic( "Status" ); // last will
	light_topic = mqtt_make_topic( "Remote/Light" );
	watchdog_topic = mqtt_make_topic( "Remote/Watchdog" );

	#ifdef MQTT_HOST
		strcpy(Mqtt->Host, MQTT_HOST);	
		Mqtt->Port = MQTT_PORT;
	#else
		struct ip_info info;
		// See below, host is generated dynamically
		Mqtt->Port = 1883;
	#endif
	
	mqtt_network_new( &network );
	
	mqtt_debug_print( "%s: Starting mqtt service\n", __FUNCTION__ );
	while(1) 
	{
		reconnect = false;
			
		vTaskDelay( 2000 / portTICK_RATE_MS );
		mqtt_debug_print( "%s: (Re)connecting to MQTT server ... ", __FUNCTION__ );
		   
		ret = sdk_wifi_station_get_connect_status();
		if( ret != STATION_GOT_IP )
		{
			mqtt_debug_print( "error wlan (%d)\n", ret );
			continue;
		}
		 
		#ifndef MQTT_HOST
			ret = sdk_wifi_get_ip_info(STATION_IF, &info);
			if( ret == false )
			{
				mqtt_debug_print( "error getting gateway IP\n" );
				continue;
			}
			sprintf(Mqtt->Host, IPSTR, IP2STR(&info.gw));
			mqtt_debug_print( "%s:%d ... ", Mqtt->Host, Mqtt->Port );
		#endif
		
		ret = mqtt_network_connect( &network, Mqtt->Host, Mqtt->Port );
		if( ret != 0 ) 
		{
			mqtt_debug_print( "error connecting (%d)\n", ret );
			continue;
		}
		mqtt_debug_print( "done\n\r" );
		mqtt_client_new( &Mqtt->Client, &network, 5000, Mqtt->Buf, sizeof(Mqtt->Buf),
		                 Mqtt->ReadBuf, sizeof(Mqtt->ReadBuf) );

		data.willFlag = 1;
		data.will.qos = 1;
		data.will.retained = 1;
		data.will.topicName.cstring = lwt_topic;
		data.will.message.cstring = (char*)"Offline";
		data.MQTTVersion        = 3;
		data.clientID.cstring   = Mqtt->ClientId;
		data.username.cstring   = 0;
		data.password.cstring   = 0;
		data.keepAliveInterval  = 100;
		data.cleansession       = 1;
		mqtt_debug_print( "%s: Send MQTT connect ... ", __FUNCTION__ );
		ret = mqtt_connect( &Mqtt->Client, &data );
		if(ret)
		{
			mqtt_debug_print("error: %d\n\r", ret);
			mqtt_network_disconnect(&network);
			taskYIELD();
			continue;
		}
		mqtt_debug_print( "done\n" );

		if( light_topic != NULL )
		{
			mqtt_subscribe( &Mqtt->Client, light_topic, MQTT_QOS1, light_message_received );
			if( ret == MQTT_FAILURE )
			{
				mqtt_debug_print( "%s: Failed to subscribe '%s'\n", __FUNCTION__, light_topic );
			}
		}

		if( watchdog_topic != NULL )
		{
			ret = mqtt_subscribe( &Mqtt->Client, watchdog_topic, MQTT_QOS1, watchdog_message_received );
			if ( ret == MQTT_FAILURE )
			{
				mqtt_debug_print( "%s: Failed to subscribe '%s'\n", __FUNCTION__, watchdog_topic );
			}
		}

		xQueueReset( Mqtt->PublishQueue );
		
		ret = mqtt_pub( "Status", "Online" ); 
		if( ret == false ) continue;
		ret = mqtt_pub( "Build", __DATE__ " " __TIME__ ); 
		if( ret == false ) continue;
		wifi_pub_stations();		

		while(1)
		{
			mqtt_msg* msg = NULL;
			while( xQueueReceive(Mqtt->PublishQueue, (void *)&msg, 0) == pdTRUE )
			{
				if( msg == NULL ) 
				{
					reconnect = true;
					break; 
				}
				mqtt_debug_print( "%s: got message '%s' to publish\n", __FUNCTION__, msg->topic );
				mqtt_message_t message;
				message.payload = msg->payload;
				message.payloadlen = msg->payload_len;
				message.dup = 0;
				message.qos = MQTT_QOS1;
				message.retained = 0;
				ret = mqtt_publish( &Mqtt->Client, msg->topic, &message );
				if (ret != MQTT_SUCCESS ){
					mqtt_debug_print( "%s: Error while publishing message (%d)\n", __FUNCTION__, ret );
				}
				vPortFree( msg->topic );
				vPortFree( msg->payload );
				vPortFree( msg );
				msg = NULL;
			}

			if( reconnect == true ) break;
			
			ret = mqtt_yield( &Mqtt->Client, 1000 );
			if( ret == MQTT_DISCONNECTED ) break;
		}
		mqtt_debug_print( "%s: Connection dropped, request restart\n\r", __FUNCTION__ );
		mqtt_network_disconnect(&network);
		taskYIELD();
	}
}



static void light_message_received( mqtt_message_data_t *md )
{
	mqtt_message_t *message = md->message;

	mqtt_debug_print( "%s: received light message '%.*s'\n", __FUNCTION__, message->payloadlen, message->payload );
	light_start( message->payload, message->payloadlen );
}

static void watchdog_message_received( mqtt_message_data_t *md )
{
	mqtt_message_t *message = md->message;

	mqtt_debug_print( "%s: received watchdog message '%.*s'\n", __FUNCTION__, message->payloadlen, message->payload );
//	watchdog_set_msg( message->payload, message->payloadlen );
}

