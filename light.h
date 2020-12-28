#ifndef LIGHT_H_
#define LIGHT_H_

#include "FreeRTOS.h"
#include "task.h"

//*****************************************************************************
// Configuration
//*****************************************************************************

// Uncomment to enable debug output
#define LIGHT_DEBUG

#define LIGHT_PIN									2		// PIN where Cathode is connected							
#define LIGHT_IOMUX								IOMUX_GPIO2_FUNC_GPIO

#define LIGHT_TASK_PRIORITY     	5
#define LIGHT_TASK_STACK					100


bool light_init( void );
void light_start( const char* str, size_t len );



#endif // LIGHT_H_