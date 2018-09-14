# lwm2m-client-for-sonoff-basic

C++ implementation of lwm2m client code for use with Sonoff Basic WiFi Wireless Smart Switch from iTead.

## Compiling/Flashing

Project built on PlatformIO core. Refer to PlatformIO documentation for detailed instructions on core installation and project configuration at: http://docs.platformio.org/en/latest/


To compile project run:

    platformio run

------------------------------------------------------------------------------------------------------------------------------

To upload project switch device into flash mode by powering on device with button pressed, then run:

    platformio run -t upload

------------------------------------------------------------------------------------------------------------------------------

To see device status information through serial connection, upload project as such:

    platformio run -e debug -t upload


## Configuration

To configure device hold button for at least three seconds, until green LED starts blinking. Connect to AP "SonoffAP" with password "12345678".

------------------------------------------------------------------------------------------------------------------------------

To view current configuration send GET request to "http://192.168.4.1:80/ap", ex.:

    curl http://192.168.4.1:80/ap

------------------------------------------------------------------------------------------------------------------------------

To keep current configuration send GET request to "http://192.168.4.1:80/keep", ex.:

    curl http://192.168.4.1:80/keep

------------------------------------------------------------------------------------------------------------------------------

To change current configuration send POST request to "http://192.168.4.1:80/ap", with JSON payload, ex.:

    curl http://192.168.4.1:80/ap -d'{"ssid":"ap_ssid","pass":"ap_password","client_name":"example_client","server_address":"coap://192.168.0.1:5555"}' -H 'Content-type:application/json'

Password recommended at least eight characters long.


## LED indication:

Green LED fast blinking: device in AP mode;

Green LED on for some time then off for short period: configured AP not found or device failed to connect. Device will try to reconnect automaticaly;

Geen LED one short pulse: device is operational;

Geen LED two short pulses: device AP mode failed;

(IF GPIO12 INTERFACED WITH RED LED):

Red LED on: relay switched on;

Red LED off: relay switched off;


## Controling device

Device configured to connect to lwm2m rest server. For instructions on how to access REST interface, refer to rest server example. To control relay send PUT request with TLV package to rest server. Object ID - 3312, resource ID - 5850.

Example:

To turn on:

    echo -ne '\xe1\x16\xda\x01' | curl http://localhost:8888/endpoints/clientname/3312/0/5850 -X PUT -H "Content-Type: application/vnd.oma.lwm2m+tlv" --data-binary @-

To turn off:

    echo -ne '\xe1\x16\xda\x00' | curl http://localhost:8888/endpoints/clientname/3312/0/5850 -X PUT -H "Content-Type: application/vnd.oma.lwm2m+tlv" --data-binary @-

Last byte of TLV package is a boolean representation of desired relay state - 00 for off, and 01 for on.




