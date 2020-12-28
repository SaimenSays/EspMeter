#ifndef _OTA_TFTP_H
#define _OTA_TFTP_H

#include "lwip/err.h"

typedef void (*tftp_receive_cb)(size_t bytes_received);

/* TFTP Server OTA Support
 *
 * To use, call ota_tftp_init_server() which will start the TFTP server task
 * and keep it running until a valid image is sent.
 *
 * The server expects to see a valid image sent with filename "filename.bin"
 * and will switch "slots" and reboot if a valid image is received.
 *
 * Note that the server will allow you to flash an "OTA" update that doesn't
 * support OTA itself, and possibly brick the esp requiring serial upload.
 *
 * Example client comment:
 * tftp -m octet ESP_IP -c put firmware/myprogram.bin firmware.bin
 *
 * TFTP protocol implemented as per RFC1350:
 * https://tools.ietf.org/html/rfc1350
 *
 * IMPORTANT: TFTP is not a secure protocol.
 * Only allow TFTP OTA updates on trusted networks.
 *
 *
 * For more details, see https://github.com/SuperHouse/esp-open-rtos/wiki/OTA-Update-Configuration
 */

 
//*****************************************************************************
// Configuration
//*****************************************************************************

#define OTA_TFTP_PORT 						69
#define OTA_TFTP_MAX_IMAGE_SIZE 	0x80000 // We use 512kb, 1MB images are max at the moment
#define OTA_TFTP_FIRMWARE_FILE 		"main.bin"
#define OTA_TFTP_OCTET_MODE 			"octet" /* non-case-sensitive */

// Uncomment to enable debug output
//#define OTA_TFTP_DEBUG

#define OTA_TFTP_TASK_STACK		500


//*****************************************************************************
// Global data
//*****************************************************************************

extern xTaskHandle tftp_task_handle;


//*****************************************************************************
// Function prototypes
//*****************************************************************************


void ota_print_info( void );

// Create an connection handler for TFTP
// Task is created on demand when update from a TFTP client is initiated.
bool ota_tftp_init_server( void );



#endif
