#include <stdarg.h>
#include "debug.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "espressif/esp_common.h"
#include "lwip/tcp.h"
#include "string.h"



//*****************************************************************************
// Global data structures
//*****************************************************************************
typedef struct
{
	#ifdef DEBUG_TCP
		struct tcp_pcb *tcp_pcb;
		struct tcp_pcb *tcp_pcb_out;
	#endif
	char* bufferPtr;
	SemaphoreHandle_t SemaphoreHandle;
} debug_t;


debug_t* debug = NULL;


//*****************************************************************************
// Local function prototypes
//*****************************************************************************
#ifdef DEBUG_TCP
	static err_t debug_accept( void *arg, struct tcp_pcb *pcb, err_t err );
	static void debug_close( void );
#endif

#ifdef DEBUG_DEBUG
	#define debug_printf(fmt, ...)			printf(fmt, ##__VA_ARGS__)
#else	
	#define debug_printf(fmt, ...)
#endif



//*****************************************************************************
// Function code
//*****************************************************************************

bool debug_init( void )
{
	if( debug != NULL ) 
	{
		debug_printf( "%s: Already initialized\n", __FUNCTION__ );
		return false;
	}
	debug_printf( "%s: Initialising debug ... ", __FUNCTION__ );
	
	debug = pvPortMalloc( sizeof(debug_t) );
	if( debug == NULL ) 
	{
		debug_printf( "%s: Unable to allocate memory\n", __FUNCTION__ );
		return false;
	}
	#ifdef DEBUG_TCP
		debug->tcp_pcb = NULL;
		debug->tcp_pcb_out = NULL;
	#endif
	debug->bufferPtr = NULL;
	debug->SemaphoreHandle = NULL;
	
	debug->SemaphoreHandle = xSemaphoreCreateMutex();
	if( debug->SemaphoreHandle == NULL ) 
	{
		debug_printf( "%s: Unable to create sempaphore\n", __FUNCTION__ );
		return false;
	}
	
	#if defined(DEBUG_TCP) || defined(DEBUG_PRINTF)
		debug->bufferPtr = pvPortMalloc( DEBUG_STRING_SIZE );
		if( debug->bufferPtr == NULL ) 
		{
			debug_printf( "Failed to allocate buffer\n" );
			return false;
		}
		
		debug_printf( "Done\n" );
		return true;

	#else
		debug_printf( "No debug interface defined\n" );
		return false;
	#endif
}



bool debug_wifi_init( void )
{
	err_t err;
	
	if( debug == NULL )
	{
		debug_printf( "%s: Debug not initialized\n", __FUNCTION__ );
		return false;
	}

	#ifdef DEBUG_TCP
		debug_printf( "%s: TCP binding ... ", __FUNCTION__ );
		LOCK_TCPIP_CORE();
		debug->tcp_pcb = tcp_new();
		if( debug->tcp_pcb == NULL )
		{
			UNLOCK_TCPIP_CORE();
			debug_printf( "Failed to create\n" );
			return false;
		}
		err = tcp_bind( debug->tcp_pcb, IP_ADDR_ANY, DEBUG_TCP_PORT );
		if( err != ERR_OK )
		{
			UNLOCK_TCPIP_CORE();
			debug_printf( "Failed to bind (%d)\n", err );
			return false;
		}
		debug->tcp_pcb = tcp_listen( debug->tcp_pcb );
		tcp_accept( debug->tcp_pcb, debug_accept );
		UNLOCK_TCPIP_CORE();
		debug_printf( "Port is %d\n", DEBUG_TCP_PORT );
	#endif
				
	return true;
}


void debug_print( const char *format, ... )
{
	va_list arglist;
	va_start( arglist, format );
	
	debug_print_va( format, arglist );
	
	va_end( arglist );
}



void debug_print_va( const char *format, va_list arglist )
{
	#if defined(DEBUG_TCP) || defined(DEBUG_PRINTF)
	  if( debug == NULL ) return;
		#if defined(DEBUG_TCP) && !defined(DEBUG_PRINTF)
			if( debug->tcp_pcb_out == NULL ) return;
		#endif	

		bool ret = xSemaphoreTake( debug->SemaphoreHandle, (DEBUG_PRINT_TIMEOUT / portTICK_RATE_MS) );
		if( ret == pdFALSE )
		{
			debug_printf( "%s: Unable to take semaphore\n", __FUNCTION__ );
			return;
		}
		
		vsnprintf( debug->bufferPtr, DEBUG_STRING_SIZE, format, arglist );
		debug->bufferPtr[DEBUG_STRING_LEN] = '\0';		// Ensure string termination if snprintf ends len
		
		#ifdef DEBUG_PRINTF
			printf( debug->bufferPtr );
		#endif	

		#ifdef DEBUG_TCP			
			err_t err;
			size_t len;
			
			if( debug->tcp_pcb_out != NULL )
			{
				len = strlen(debug->bufferPtr);
				debug_printf("%s: Sending %d chars\n", __FUNCTION__, len );
				LOCK_TCPIP_CORE();
				err = tcp_write( debug->tcp_pcb_out, debug->bufferPtr, len, TCP_WRITE_FLAG_COPY );
				UNLOCK_TCPIP_CORE();
				if( err != ERR_OK ) 
				{
					debug_printf( "Failed to write (%d)\n", (int)err );
					debug_close();
				}
				#ifdef DEBUG_TCP_INSTANT
				else
				{
					LOCK_TCPIP_CORE();
					ret = tcp_output( debug->tcp_pcb_out );
					UNLOCK_TCPIP_CORE();
					if( ret != ERR_OK ) 
					{
						debug_printf( "Failed to output (%d)\n", (int)ret );
						debug_close();
					}
				}
				#endif
			}
		#endif
		
		xSemaphoreGive( debug->SemaphoreHandle );
	#endif
}		



#ifdef DEBUG_TCP
	static err_t debug_accept( void *arg, struct tcp_pcb *pcb, err_t err )
	{
		int len;
		err_t ret;
		
		debug_printf( "%s: Accepting connection ... ", __FUNCTION__);
		LWIP_UNUSED_ARG( arg );
		LWIP_UNUSED_ARG( err );
		tcp_setprio( pcb, TCP_PRIO_MIN );

		len = sprintf( debug->bufferPtr, "ESP debug output over TCP\n\n" );

		ret = tcp_write( pcb, debug->bufferPtr, len, TCP_WRITE_FLAG_COPY );
		if( ret == ERR_OK ) ret = tcp_output( pcb );
		
		if( ret == ERR_OK )
		{
			debug->tcp_pcb_out = pcb;
			debug_printf( "Done\n" );
		}
		else 
		{
			debug_printf( "Failed\n" );
		}
	
		return ret;
	}


	
	static void debug_close( void )
	{		
		tcp_close( debug->tcp_pcb_out );
		debug->tcp_pcb_out = NULL;
		debug_printf( "%s: Connection closed\n", __FUNCTION__);
	}
#endif
