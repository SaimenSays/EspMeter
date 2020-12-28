#ifndef MQTT_H_
#define MQTT_H_

#include "FreeRTOS.h"
#include "task.h"
#include "stdint.h"


//*****************************************************************************
// Configuration
//*****************************************************************************

// Uncomment to enable debug output
//#define MQTT_DEBUG

#define MQTT_TOPIC_MAIN 							"OpenWay"

#define MQTT_PUBLISH_QUEUE_SIZE				10



//*****************************************************************************
// Data structures
//*****************************************************************************

typedef struct {
	char* topic;
	char* payload;
	uint16_t payload_len;
} mqtt_msg;

extern xTaskHandle mqtt_task_handle;

//*****************************************************************************
// Function prototypes
//*****************************************************************************

bool mqtt_init( void );
void mqtt_deinit( void );
bool mqtt_pub( const char* topic, const char* payload, ... );
bool mqtt_reconnect( void );
bool mqtt_is_connected( void );


#endif /* MQTT_H_ */
