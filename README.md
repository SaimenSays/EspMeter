# EspMeter
This is an infrared to MQTT adapter for smartmeter based on libsml, running on ESP8266.

#### Hardware
A simple IR diode in series with a resistor connected to RX pin of ESP8266 module. I used a ESP01 because it is small and no more pins are used.
We only have an ltron OpenWay 3HZ in our house. So, the repo is only tested for this meter, but should work with others too.

#### Depends
esp-open-sdk and esp-open-rtos
https://github.com/SuperHouse/esp-open-rtos.git
or
https://github.com/ourairquality/oaq-esp-open-rtos.git

You need to install this on your own, because it is not linked to this projekt

#### Setup
You need to edit Makefile to add you WIFI credentials and MQTT broker.
All SML parameters received via infrared will be pubilshed as separate MQTT topic. Each object is a json string with value and unit:

	{"value":123456.7,"unit":"Wh"}
