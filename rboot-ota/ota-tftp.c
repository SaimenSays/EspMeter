/* TFTP Server OTA support
 *
 * For details of use see ota-tftp.h
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#include <FreeRTOS.h>
#include <string.h>
#include <strings.h>

#include "lwip/err.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/mem.h"

#include <netbuf_helpers.h>
#include <espressif/spi_flash.h>
#include <espressif/esp_system.h>

#include "ota-tftp.h"
#include "rboot-api.h"
#ifdef OTA_TFTP_DEBUG
	#include "debug.h"
#endif


//*****************************************************************************
// Definitions
//*****************************************************************************
#define TFTP_OP_RRQ 1
#define TFTP_OP_WRQ 2
#define TFTP_OP_DATA 3
#define TFTP_OP_ACK 4
#define TFTP_OP_ERROR 5
#define TFTP_OP_OACK 6

#define TFTP_ERR_FILENOTFOUND 1
#define TFTP_ERR_FULL 3
#define TFTP_ERR_ILLEGAL 4
#define TFTP_ERR_BADID 5

#define MAX_IMAGE_SIZE 0x100000 /*1MB images max at the moment */


//*****************************************************************************
// Global data
//*****************************************************************************

xTaskHandle tftp_task_handle = NULL;



//*****************************************************************************
// Local function prototypes
//*****************************************************************************

static void ota_tftp_task(void* pvParameters);
static void ota_tftp_event_callback(struct netconn *nc, enum netconn_evt evt, u16_t len);
static char *tftp_get_field(int field, struct netbuf *netbuf);
static err_t tftp_receive_data(struct netconn *nc, size_t write_offs, size_t limit_offs, size_t *received_len, ip_addr_t *peer_addr, int peer_port, tftp_receive_cb receive_cb);
static err_t tftp_send_ack(struct netconn *nc, int block);
static void tftp_send_error(struct netconn *nc, int err_code, const char *err_msg);

#ifdef OTA_TFTP_DEBUG
	#define ota_debug_print(fmt, ...)			debug_print(fmt, ##__VA_ARGS__)
#else	
	#define ota_debug_print(fmt, ...)
#endif


//*****************************************************************************
// Function code
//*****************************************************************************


void ota_print_info( void )
{
	rboot_config conf = rboot_get_config();
	
	ota_debug_print( "%s: Image addresses in flash:\n", __FUNCTION__ );
	for( int i = 0; i <conf.count; i++ ) 
	{
		ota_debug_print( "  %c%d: offset 0x%08x\n", (i == conf.current_rom ? '*':' '), i, conf.roms[i] );
	}	
}


bool ota_tftp_init_server( void )
{
	struct netconn* nc;
	rboot_config conf;
  
	conf = rboot_get_config();
	if(conf.count == 1)
	{
		ota_debug_print( "%s: Only one firmware slot available. OTA is not possible\n", __FUNCTION__ );
		return false;
	}

	nc = netconn_new_with_callback(NETCONN_UDP, ota_tftp_event_callback);
	if(nc == NULL) 
	{
		ota_debug_print( "%s: Failed to allocate socket\n", __FUNCTION__ );
		return false;
	}

	netconn_bind(nc, IP_ADDR_ANY, OTA_TFTP_PORT);
	return true;
}


static void ota_tftp_event_callback(struct netconn *nc, enum netconn_evt evt, u16_t len)
{
	// No action needed if task is already created
	if(tftp_task_handle != NULL) return;
	
	ota_debug_print( "%s: Creating TFTP server task ... ", __FUNCTION__ );
	xTaskCreate(ota_tftp_task, "tftp_task", OTA_TFTP_TASK_STACK, nc, 2, &tftp_task_handle);
	#ifdef OTA_TFTP_DEBUG
		if(tftp_task_handle == NULL) 
		{
			debug_print( "Failed\n" );
		}
		else 
		{
			debug_print( "OK\n" );
		}
	#endif
}


static void ota_tftp_task( void *pvParameters )
{
		struct netconn *nc = pvParameters;	

		ota_debug_print( "%s: Task running and waiting for data\n", __FUNCTION__ );
	
    /* We expect a WRQ packet with filename OTA_TFTP_FIRMWARE_FILE and "octet" mode,
    */
    while(1)
    {
        /* wait as long as needed for a WRQ packet */
        netconn_set_recvtimeout(nc, 0);
        struct netbuf *netbuf;
        err_t err = netconn_recv(nc, &netbuf);
        if(err != ERR_OK) 
				{
						ota_debug_print( "%s: Failed to receive TFTP initial packet (%d)\n", __FUNCTION__, err );
            continue;
        }
        uint16_t len = netbuf_len(netbuf);
        if(len < 6) 
				{
						ota_debug_print( "%s: Packet too short for a valid WRQ\n", __FUNCTION__ );
            netbuf_delete(netbuf);
            continue;
        }

        uint16_t opcode = netbuf_read_u16_n(netbuf, 0);
        if(opcode != TFTP_OP_WRQ) 
				{
						ota_debug_print( "%s: Invalid opcode 0x%04x didn't match WRQ\n", __FUNCTION__, opcode );
            netbuf_delete(netbuf);
 						vTaskDelete(NULL);
        }

        /* check filename */
        char *filename = tftp_get_field(0, netbuf);
        if(!filename || strcmp(filename, OTA_TFTP_FIRMWARE_FILE)) 
				{
						ota_debug_print( "%s: File must be %s. Deleting task\n", __FUNCTION__, OTA_TFTP_FIRMWARE_FILE );
            tftp_send_error(nc, TFTP_ERR_FILENOTFOUND, "File must be " OTA_TFTP_FIRMWARE_FILE);
            free(filename);
            netbuf_delete(netbuf);
 						vTaskDelete(NULL);
        }
        free(filename);

        /* check mode */
        char *mode = tftp_get_field(1, netbuf);
        if(!mode || strcmp(OTA_TFTP_OCTET_MODE, mode)) 
				{
						ota_debug_print( "%s: Mode must be binary. Deleting task\n", __FUNCTION__ );
            tftp_send_error(nc, TFTP_ERR_ILLEGAL, "Mode must be octet/binary");
            free(mode);
            netbuf_delete(netbuf);
						vTaskDelete(NULL);
        }
        free(mode);

        /* establish a connection back to the sender from this netbuf */
        netconn_connect(nc, netbuf_fromaddr(netbuf), netbuf_fromport(netbuf));
        netbuf_delete(netbuf);

        /* Find next free slot - this requires flash unmapping so best done when no packets in flight */
        rboot_config conf;
        conf = rboot_get_config();
        int slot = (conf.current_rom + 1) % conf.count;

        /* ACK the WRQ */
        int ack_err = tftp_send_ack(nc, 0);
        if(ack_err != 0) 
				{
						ota_debug_print( "%s: Initial ACK failed. Deleting task\n", __FUNCTION__ );
            netconn_disconnect(nc);
						vTaskDelete(NULL);
        }

        /* Finished WRQ phase, start TFTP data transfer */
        size_t received_len;
        netconn_set_recvtimeout(nc, 10000);
        int recv_err = tftp_receive_data(nc, conf.roms[slot], conf.roms[slot]+MAX_IMAGE_SIZE, &received_len, NULL, 0, NULL);

        netconn_disconnect(nc);
				ota_debug_print( "%s: Receive data result %d bytes %d\n", __FUNCTION__, recv_err, received_len );
        if(recv_err == ERR_OK) 
				{
						ota_debug_print( "%s: Receiving finished. Changing slot to %d\n", __FUNCTION__, slot );
            vPortEnterCritical();
            if(!rboot_set_current_rom(slot)) 
						{
								vPortExitCritical();
								ota_debug_print( "%s: Failed to set new rboot slot. Deleting task\n", __FUNCTION__ );
								netconn_disconnect(nc);
								vTaskDelete(NULL);
            }

            sdk_system_restart();
        }
    }
}

/* Return numbered field in a TFTP RRQ/WRQ packet

   Uses dest_buf (length dest_len) for temporary storage, so dest_len must be
   at least as long as the longest valid/expected field.

   Result is either NULL if an error occurs, or a newly malloced string that the
   caller needs to free().
 */
static char *tftp_get_field(int field, struct netbuf *netbuf)
{
    int offs = 2;
    int field_offs = 2;
    int field_len = 0;
    /* Starting past the opcode, skip all previous fields then find start/len of ours */
    while(field >= 0 && offs < netbuf_len(netbuf)) {
        char c = netbuf_read_u8(netbuf, offs++);
        if(field == 0) {
            field_len++;
        }
        if(c == 0) {
            field--;
            if(field == 0)
                field_offs = offs;
        }
    }

    if(field != -1)
        return NULL;

    char * result = malloc(field_len);
    netbuf_copy_partial(netbuf, result, field_len, field_offs);
    return result;
}

#define TFTP_TIMEOUT_RETRANSMITS 10

static err_t tftp_receive_data(struct netconn *nc, size_t write_offs, size_t limit_offs, size_t *received_len, ip_addr_t *peer_addr, int peer_port, tftp_receive_cb receive_cb)
{
    *received_len = 0;
    const int DATA_PACKET_SZ = 512 + 4; /*( packet size plus header */
    uint32_t start_offs = write_offs;
    int block = 1;

    struct netbuf *netbuf = 0;
    int retries = TFTP_TIMEOUT_RETRANSMITS;

    while(1)
    {
        if(peer_addr) {
            netconn_disconnect(nc);
        }

        err_t err = netconn_recv(nc, &netbuf);

        if(peer_addr) {
            if(netbuf) {
                /* For TFTP server, the UDP connection is already established. But for client,
                   we don't know what port the server is using until we see the first data
                   packet - so we connect here.
                */
                netconn_connect(nc, netbuf_fromaddr(netbuf), netbuf_fromport(netbuf));
                peer_addr = 0;
            } else {
                /* Otherwise, temporarily re-connect so we can send errors */
                netconn_connect(nc, peer_addr, peer_port);
            }
        }

        if(err == ERR_TIMEOUT) {
            if(retries-- > 0 && block > 1) {
                /* Retransmit the last ACK, wait for repeat data block.

                 This doesn't work for the first block, have to time out and start again. */
                tftp_send_ack(nc, block-1);
                continue;
            }
            tftp_send_error(nc, TFTP_ERR_ILLEGAL, "Timeout");
            return ERR_TIMEOUT;
        }
        else if(err != ERR_OK) {
            tftp_send_error(nc, TFTP_ERR_ILLEGAL, "Failed to receive packet");
            return err;
        }

        uint16_t opcode = netbuf_read_u16_n(netbuf, 0);
        if(opcode != TFTP_OP_DATA) {
            tftp_send_error(nc, TFTP_ERR_ILLEGAL, "Unknown opcode");
            netbuf_delete(netbuf);
            return ERR_VAL;
        }

        uint16_t client_block = netbuf_read_u16_n(netbuf, 2);
        if(client_block != block) {
            netbuf_delete(netbuf);
            if(client_block == block-1) {
                /* duplicate block, means our ack got lost */
                tftp_send_ack(nc, block-1);
                continue;
            }
            else {
                tftp_send_error(nc, TFTP_ERR_ILLEGAL, "Block# out of order");
                return ERR_VAL;
            }
        }

        /* Reset retry count if we got valid data */
        retries = TFTP_TIMEOUT_RETRANSMITS;

        if(write_offs % SECTOR_SIZE == 0) {
            sdk_spi_flash_erase_sector(write_offs / SECTOR_SIZE);
        }

        /* One UDP packet can be more than one netbuf segment, so iterate all the
           segments in the netbuf and write them to flash
        */
        int offset = 0;
        int len = netbuf_len(netbuf);

        if(write_offs + len >= limit_offs) {
            tftp_send_error(nc, TFTP_ERR_FULL, "Image too large");
            return ERR_VAL;
        }

        bool first_chunk = true;
        do
        {
            uint16_t chunk_len;
            uint32_t *chunk;
            netbuf_data(netbuf, (void **)&chunk, &chunk_len);
            if(first_chunk) {
                chunk++; /* skip the 4 byte TFTP header */
                chunk_len -= 4; /* assuming this netbuf chunk is at least 4 bytes! */
                first_chunk = false;
            }
            if(chunk_len && ((uint32_t)chunk % 4)) {
                /* sdk_spi_flash_write requires a word aligned
                   buffer, so if the UDP payload is unaligned
                   (common) then we copy the first word to the stack
                   and write that to flash, then move the rest of the
                   buffer internally to sit on an aligned offset.

                   Assuming chunk_len is always a multiple of 4 bytes.
                */
                uint32_t first_word;
                memcpy(&first_word, chunk, 4);
                sdk_spi_flash_write(write_offs+offset, &first_word, 4);
                memmove(LWIP_MEM_ALIGN(chunk),&chunk[1],chunk_len-4);
                chunk = LWIP_MEM_ALIGN(chunk);
                offset += 4;
                chunk_len -= 4;
            }
            sdk_spi_flash_write(write_offs+offset, chunk, chunk_len);
            offset += chunk_len;
        } while(netbuf_next(netbuf) >= 0);

        netbuf_delete(netbuf);

        *received_len += len - 4;

        if(len < DATA_PACKET_SZ) 
				{
            /* This was the last block, but verify the image before we ACK
               it so the client gets an indication if things were successful.
            */
            const char *err = "Unknown validation error";
            uint32_t image_length;
            if(!rboot_verify_image(start_offs, &image_length, &err)
               || image_length != *received_len) {
                tftp_send_error(nc, TFTP_ERR_ILLEGAL, err);
                return ERR_VAL;
            }
        }

        err_t ack_err = tftp_send_ack(nc, block);
        if(ack_err != ERR_OK) 
				{
						ota_debug_print( "%s: Failed to send ACK\n", __FUNCTION__ );
            return ack_err;
        }

        // Make sure ack was successful before calling callback.
        if(receive_cb) {
            receive_cb(*received_len);
        }

        if(len < DATA_PACKET_SZ) {
            return ERR_OK;
        }

        block++;
        write_offs += 512;
    }
}

static err_t tftp_send_ack(struct netconn *nc, int block)
{
    /* Send ACK */
    struct netbuf *resp = netbuf_new();
    uint16_t *ack_buf = (uint16_t *)netbuf_alloc(resp, 4);
    ack_buf[0] = htons(TFTP_OP_ACK);
    ack_buf[1] = htons(block);
    err_t ack_err = netconn_send(nc, resp);
    netbuf_delete(resp);
    return ack_err;
}

static void tftp_send_error(struct netconn *nc, int err_code, const char *err_msg)
{
		ota_debug_print( "%s: Error: %s\n", __FUNCTION__, err_msg );
    struct netbuf *err = netbuf_new();
    uint16_t *err_buf = (uint16_t *)netbuf_alloc(err, 4+strlen(err_msg)+1);
    err_buf[0] = htons(TFTP_OP_ERROR);
    err_buf[1] = htons(err_code);
    strcpy((char *)&err_buf[2], err_msg);
    netconn_send(nc, err);
    netbuf_delete(err);
}
