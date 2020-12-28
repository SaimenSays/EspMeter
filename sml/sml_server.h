#ifndef SML_SERVER_H_
#define SML_SERVER_H_

#include "stdbool.h"



//*****************************************************************************
// Configuration
//*****************************************************************************

// Uncomment to enable debug output
#define SML_DEBUG

#define UART_TASK_PRIORITY     	2
#define UART_TASK_STACK					5000
#define UART_BUFFER_LEN					512



//*****************************************************************************
// Data structures
//*****************************************************************************



//*****************************************************************************
// Function prototypes
//*****************************************************************************

bool sml_server_init( void );



#endif // SML_SERVER_H_