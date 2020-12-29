# EspMeter
This is an infrared to MQTT adapter for smartmeter based on libsml, running on ESP8266.  
It is a quick and dirty build, but already transmitting data since some month now without any issue or hangup.

#### Hardware
A simple IR diode in series with a resistor connected to RX pin of ESP8266 module. (The resistor needs to be adjusted, to get squarewave signal on RX pin)  
We only have an itron OpenWay 3HZ in our house. So, the repo is only tested for this meter, but should work with others too.
I used a ESP01 because it is small and no more pins are used. If you need some inspiration how to build a device, have a look in pictures folder.

#### Depends
esp-open-sdk and esp-open-rtos. You need to download and install these on your own, because it is not linked to this projekt.
https://github.com/SuperHouse/esp-open-rtos.git  
or  
https://github.com/ourairquality/oaq-esp-open-rtos.git

#### Setup
You need to edit Makefile to adapt your WIFI credentials and MQTT broker.  
Compiling and upload to ESP is also on your side.
Hint: RX input of ESP is used for infrared. Remove programmer TX pin for normal operation, otherwise it won't receive anything from infrared diode.

#### Output
SML messages received via infrared will be paresed by libsml and pubilshed as a separate MQTT topic. Each MQTT value is a json string with value and unit:

	{"value":123456.7,"unit":"Wh"}


#### References
https://wiki.volkszaehler.org/hardware/controllers/ir-schreib-lesekopf-rs232-ausgang  
https://de.wikipedia.org/wiki/Smart_Message_Language  
https://www.promotic.eu/en/pmdoc/Subsystems/Comm/PmDrivers/IEC62056_OBIS.htm  
