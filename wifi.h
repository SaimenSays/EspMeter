#ifndef WIFI_H_
#define WIFI_H_

#include "stdbool.h"



//*****************************************************************************
// Configuration
//*****************************************************************************

// Uncomment to enable debug output
#define WIFI_DEBUG

#define WIFI_SLEEP												WIFI_SLEEP_LIGHT

#define WIFI_INIT_TASK_PRIORITY						1   // For init and fallback
#define WIFI_INIT_TASK_STACK							400

#define WIFI_INIT_DELAY										2  // s
#define WIFI_INIT_TIMEOUT									30 // s

//*****************************************************************************
// Data structures
//*****************************************************************************
typedef void(*wifi_init_callback_t)(void);



//*****************************************************************************
// Function prototypes
//*****************************************************************************
bool wifi_init( wifi_init_callback_t init_callback );
void wifi_pub_stations( void );



#endif // WIFI_H_