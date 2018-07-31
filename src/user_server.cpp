/*
 * user_server.c
 *
 *  Created on: Jul 31, 2018
 *      Author: tautvydas
 */
//SDK
#include "user_interface.h"

//ARDUINO
#include <../../libraries/ESP8266WebServer/src/ESP8266WebServer.h>
#include <ArduinoJson.h>

//USER
#include "user_functions.h"

static volatile bool server_done = false;
ESP8266WebServer server(80);
extern struct network_info buff;

void ICACHE_FLASH_ATTR httpserver(void)
{
	server_done = false;

	server.on("/ap", HTTP_POST, handle_post);
	server.on("/ap", HTTP_GET, handle_get);
	server.on("/keep", [](){

		server.send(200, "text/plain", "Keeping current configuration\r\n");

		server_done = true;
	});
	server.onNotFound(handle_not_found);
	server.begin();

	while(!server_done)
	{
		server.handleClient();
		delay(100);
		yield();
	}

	server.stop();
}

void ICACHE_FLASH_ATTR handle_post(void)
{
	debugln("POST request\r\n");

	WiFiClient client = server.client();

	StaticJsonBuffer<200> jsonBuffer;

	JsonObject &root = jsonBuffer.parseObject(server.arg("plain"));

	client.flush();

	if(!root.success())
	{
		debugln("Couldn't parse buffer\r\n");
		server.send(500, "text/plain", "Couldn't parse buffer\r\n");
		return;
	}

	if(!(root.containsKey("ssid") && root.containsKey("pass") && root.containsKey("client_name") && root.containsKey("server_address")))
	{
		debugln("One or more JSON attributes missing\r\n");
		server.send(400, "text/plain", "One or more JSON attributes missing\r\n");
		return;
	}

	else if(root["ssid"] == "" || root["pass"] == "" || root["client_name"] == "" || root["server_address"] == "")
	{
		debugln("One or more JSON attributes value empty\r\n");
		server.send(400, "text/plain", "One or more JSON attributes value empty\r\n");
		return;
	}

	struct network_info buf;

	strncpy(buf.sta_ssid, root["ssid"], sizeof(buf.sta_ssid));
	strncpy(buf.sta_pass, root["pass"], sizeof(buf.sta_pass));
	strncpy(buf.wak_client_name, root["client_name"], sizeof(buf.wak_client_name));
	strncpy(buf.wak_server, root["server_address"], sizeof(buf.wak_server));
	buf.crc = gen_crc(&buf);

	set_config_eeprom(buf);

	server.send(200, "text/plain", "Configuration set, device switching to station mode\r\n");

	server_done = true;
}

void ICACHE_RAM_ATTR handle_get(void)
{
	debugln("GET request\r\n");

	WiFiClient client = server.client();

	client.flush();

	String message = "";

	get_config_eeprom();

	if(gen_crc(&buff) != buff.crc)
	{
		debugln("CRC doesn't match\r\n");
		debugln("CRC: 0x%08x\r\n", buff.crc);

		message += "Device memory corrupt, reconfigure with POST request\r\n\r\n";

		message += "To change current config send http POST message with JSON body. Example:\r\n";
		message += "\r\n";
		message += "	{\r\n";
		message += "	\"ssid\": \"wifiap\",\r\n";
		message += "	\"pass\": \"wifipass\",\r\n";
		message += "	\"server_address\": \"coap://192.168.0.1:11\",\r\n";
		message += "	\"client_name\": \"device1\"\r\n";
		message += "	}\r\n";

		server.send(500, "text/plain", message);

		return;
	}

	char buffer[50];

	message = "";

	message += "Current config:\r\n";
	sprintf(buffer, "	connect to AP: SSID - %s\r\n", buff.sta_ssid);
	message += buffer;
	sprintf(buffer, "	lwm2m server address - %s\r\n", buff.wak_server);
	message += buffer;
	sprintf(buffer, "	lwm2m client name - %s\r\n", buff.wak_client_name);
	message += buffer;
	message += "\r\n";
	message += "To change current config send http POST message with JSON body. Example:\r\n";
	message += "\r\n";
	message += "	{\r\n";
	message += "	\"ssid\": \"wifiap\",\r\n";
	message += "	\"pass\": \"wifipass\",\r\n";
	message += "	\"server_address\": \"coap://192.168.0.1:11\",\r\n";
	message += "	\"client_name\": \"device1\"\r\n";
	message += "	}\r\n";


	server.send(200, "text/plain", message);
}

void ICACHE_RAM_ATTR handle_not_found(void)
{
	server.send(404, "text/plain", "Path not found\r\n");
}




