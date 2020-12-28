PROGRAM=main

ESPPORT = /dev/ttyUSB0
ESPTOOL = esptool.py
ESPTOOL2 = esptool2/esptool2

# Path to RTOS sources
#RTOS = esp-open-rtos
RTOS = oaq-esp-open-rtos


WIFI_SSID = ChangeToYours
WIFI_PASS = ChangeToYours

MQTT_HOST = broker.hivemq.com
MQTT_PORT	= 1883


#################################################################
# Compiler definitions for passing configfile to gcc

EXTRA_CFLAGS += -DWIFI_SSID=\"$(WIFI_SSID)\"
EXTRA_CFLAGS += -DWIFI_PASS=\"$(WIFI_PASS)\"
# When follwing lines are commented, gateway ip is used as server
EXTRA_CFLAGS += -DMQTT_HOST=\"$(MQTT_HOST)\"
EXTRA_CFLAGS += -DMQTT_PORT=$(MQTT_PORT)


#################################################################
# Additional configuration

# Standard without '-Ox' is optimization level '-O2'. 
# Only thing to do is using '-O3'. But this increases stack usage and decreases free RAM. Also Code getsbigger. So we leave it default. 
#EXTRA_CFLAGS += -O3

# Flash size in megabits
# Valid values are same as for esptool.py - 2,4,8,16,32
# Note: This defines the 'sdk_flashchip.chip_size' read by main application.
#       We only need this size for programming space, not for filesystem. 
#       'sdk_flashchip.chip_size' is overwritten by FLASH_SIZE_MBIT, which is used for filesystem
# We are using two slots with 1Megabyte each -> 8Mbit*2
EXTRA_CFLAGS += -DFLASH_SIZE=16

# Real flash size in megabits
# Note: This overwrites'sdk_flashchip.chip_size', which is used for filesystem
EXTRA_CFLAGS += -DFLASH_SIZE_COMPLETE=64

EXTRA_CFLAGS += -DOTA=1

# Provide flag for SDK and RTOS 
# This predefines TICK_PER_SEC for 'esp-open-rtos\FreeRTOS\Source\portable\esp8266\xtensa_timer.h'
# And also is used for configTICK_RATE_HZ in local 'FreeRTOSConfig.h'
# Define number of RTOS ticks per second (Hz)
EXTRA_CFLAGS +=-DXT_TICK_PER_SEC=100


# LIBS definition from parmeters.mk
# Add 'm' to have math lib available for 'math.h'
LIBS = hal gcc c m

# Some fixes for rtos
EXTRA_CFLAGS += -DIP_SOF_BROADCAST=1
EXTRA_CFLAGS += -DIP_SOF_BROADCAST_RECV=1
# for httpd
#EXTRA_CFLAGS += -DTCP_SND_BUF=(3*(TCP_MSS))
# less stack usage
EXTRA_CFLAGS += -DTCPIP_THREAD_STACKSIZE=512
# mem error debugging
#EXTRA_CFLAGS += -DMEMP_DEBUG=LWIP_DBG_ON


# SPI speed in MHz
SPI_SPEED = 80

# Use 1Megabyte block for firmware
RBOOT_BIG_FLASH = 1


#################################################################
# Components including
EXTRA_COMPONENTS += $(RTOS)/extras/paho_mqtt_c
#EXTRA_COMPONENTS += $(RTOS)/extras/softuart
EXTRA_COMPONENTS += sml
EXTRA_COMPONENTS += rboot-ota




#################################################################
# Now using default buildfile with all additions from above

include $(RTOS)/common.mk
