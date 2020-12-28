#ifndef DEBUG_H_
#define DEBUG_H_

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>


//*****************************************************************************
// Configuration
//*****************************************************************************

#define DEBUG_TCP_PORT								20000
#define DEBUG_STRING_LEN							80
#define DEBUG_STRING_SIZE							(DEBUG_STRING_LEN +1)
#define DEBUG_INDENT									"  "
#define DEBUG_PRINT_TIMEOUT						50			//ms		
// Uncomment to enable printf outputs to debug itself
//#define DEBUG_DEBUG									

//#define DEBUG_TCP_INSTANT						

#define DEBUG_TCP				           	// Output to TCP
//#define DEBUG_PRINTF		             	// Output to std output

#ifdef DEBUG_TCP
	#define YAALA_USE_LWIP
#endif


//*****************************************************************************
// Function prototypes
//*****************************************************************************

bool debug_init( void );
bool debug_wifi_init( void );
void debug_print( const char *format, ... );
void debug_print_va( const char *format, va_list arglist );


#endif // DEBUG_H_