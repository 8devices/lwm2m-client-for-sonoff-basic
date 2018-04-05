
//GNU
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

//SDK
#include "osapi.h"
#include "user_interface.h"
#include "hw_timer.h"
#include "queue.h"

//WAKAAMA
#include "relay_object.h"
#include "wakaama/internals.h"
#include "wakaama_network.h"
#include "wakaama_object_utils.h"
#include "wakaama_simple_client.h"

//ARDUINO
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Arduino.h>
#include <ArduinoJson.h>

bool timer_initiated = false;
bool wakaama_initiated = false;
bool st_status = false;
bool ap_status = false;
bool server_done = false;
bool sta_reconnect = false;
bool had_connected = false;
volatile bool intr = false;

ESP8266WebServer server(80);

struct timeval tv;

struct network_info
{
	uint32_t crc;
	char sta_ssid[32];
	char sta_pass[64];
	char wak_server[64];
	char wak_client_name[32];
}buff;

enum
{
	NO_BLINK,
	BLINK1,
	BLINK2,
	BLINK3,
	BLINK4
};


os_timer_t timer;
os_timer_t led_timer;

uint8_t led_mode = 0;
uint8_t sta_tick = 0;
volatile uint8_t tick = 0;
uint32_t poly = 0x82f63b78;

lwm2m_context_t *client_context = nullptr;
device_instance_t * device_data = nullptr;
lwm2m_object_t* relay_object = nullptr;
lwm2m_object_t* led_object = nullptr;

WiFiEventHandler connection_loss;
WiFiEventHandler reconnect_after_loss;

void ICACHE_FLASH_ATTR gpio_init(void);
void ICACHE_FLASH_ATTR wifi_init(void);
void ICACHE_FLASH_ATTR wifi_ap_init(void);
void ICACHE_FLASH_ATTR debugln(const char * format, ... );
void ICACHE_FLASH_ATTR timer_init(os_timer_t *ptimer,uint32_t milliseconds, bool repeat);
void ICACHE_FLASH_ATTR timer_callback_lwm2m(void *pArg);
void ICACHE_FLASH_ATTR led_timer_init(os_timer_t *ptimer);
void ICACHE_FLASH_ATTR led_timer_callback(void *pArg);
void ICACHE_FLASH_ATTR gpio0_intr_handler(void);
const char* ICACHE_FLASH_ATTR get_client_state(lwm2m_client_state_t state);
const char* ICACHE_FLASH_ATTR get_server_state(lwm2m_status_t state);
void ICACHE_FLASH_ATTR wakaama_init(void);
void ICACHE_RAM_ATTR set_config_eeprom(struct network_info buff);
void ICACHE_RAM_ATTR get_config_eeprom(void);
void ICACHE_FLASH_ATTR httpserver(void);
void ICACHE_RAM_ATTR handle_post(void);
void ICACHE_RAM_ATTR handle_get(void);
void ICACHE_RAM_ATTR handle_not_found(void);
uint32_t ICACHE_RAM_ATTR gen_crc(const network_info *buff);
void ICACHE_RAM_ATTR connection_loss_handler(const WiFiEventStationModeDisconnected &evt);

void setup(void)
{
	Serial.begin(115200);
	while(!Serial)
	{
		yield();
	}

	debugln("setup\r\n");

	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);

	connection_loss = WiFi.onStationModeDisconnected(&connection_loss_handler);

	EEPROM.begin(512);

	led_timer_init(&led_timer);

	gpio_init();

#ifdef WIFI_AP_MODE
	wifi_ap_init();
#else
	wifi_init();
#endif
}

void loop(void)
{
	if(ap_status)
		wifi_ap_init();

#ifndef WIFI_AP_MODE
	else if(st_status)
		wifi_init();
#endif

	yield();
}


void ICACHE_FLASH_ATTR wifi_init(void)
{


	st_status = false;

	get_config_eeprom();

	if(gen_crc(&buff) != buff.crc)
	{
		debugln("CRC doesn't match\r\n");
		debugln("CRC: 0x%08x\r\n", buff.crc);
		return;
	}

	if(WiFi.status() == WL_CONNECTED)
	{
		debugln("Found previous connection\r\n");
		WiFi.reconnect();

		sta_tick = 0;

		while(WiFi.status() != WL_CONNECTED)
		{
			if(sta_tick == 19)
			{
				debugln("No AP with requested SSID found\r\n");

				led_mode = 3;


			}

			if(ap_status)
				return;

			delay(1000);

			sta_tick++;

			yield();
		}

		WiFi.setAutoConnect(false);

		WiFi.setAutoReconnect(true);
	}


	else if(WiFi.getMode() == WIFI_AP)
	{
		debugln("Switching off AP mode - ");

		WiFi.softAPdisconnect(true);

		WiFi.disconnect(true);

		WiFi.mode(WIFI_STA);

		WiFi.setAutoConnect(false);

		WiFi.setAutoReconnect(true);

		while(WiFi.status() == WL_DISCONNECTED) yield();

		debugln("successful\r\n");

		debugln("Connecting...\r\n");

		WiFi.begin(buff.sta_ssid, buff.sta_pass);

		sta_tick = 0;

		while(WiFi.status() != WL_CONNECTED)
		{
			if(sta_tick == 19)
			{
				debugln("No AP with requested SSID found\r\n");

				led_mode = 3;


			}

			if(ap_status)
				return;

			delay(1000);

			sta_tick++;

			yield();
		}

	}

	else if(WiFi.status() != WL_CONNECTED)
	{
		debugln("Setting up station\r\n");

		WiFi.disconnect(true);

		WiFi.mode(WIFI_STA);

		WiFi.begin(buff.sta_ssid, buff.sta_pass);

		debugln("Connecting...\r\n");

		sta_tick = 0;

		while(WiFi.status() != WL_CONNECTED)
		{
			if(sta_tick == 19)
			{
				debugln("No AP with requested SSID found\r\n");

				led_mode = 3;


			}

			if(ap_status)
				return;

			delay(1000);

			sta_tick++;

			yield();
		}

		WiFi.setAutoConnect(false);

		WiFi.setAutoReconnect(true);
	}

	debugln("Connected\r\n");

	sta_reconnect = false;

	wakaama_init();
	timer_init(&timer,5000,true);

	led_mode = 2;
}

void ICACHE_FLASH_ATTR wifi_ap_init(void)
{
	detachInterrupt(digitalPinToInterrupt(0));

	ap_status = false;
	st_status = false;
	sta_reconnect = true;

	debugln("%s\r\n", __func__);

	debugln("Switching off station mode - ");

	WiFi.softAPdisconnect(true);

	WiFi.disconnect(true);

	WiFi.mode(WIFI_AP);

	WiFi.setAutoConnect(false);

	WiFi.setAutoReconnect(false);

	while(WiFi.status() == WL_CONNECTED) yield();

	debugln("successful\r\n");

	if(WiFi.softAP("SonoffAP", "12345678"))
	{
		debugln("AP on\r\n");
		led_mode = 1;
	}
	else
	{
		debugln("AP should fail\r\n");
		led_mode = 4;
		attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);
		return;
	}

	httpserver();

	ap_status = false;
	led_mode = 2;

#ifndef WIFI_AP_MODE

	st_status = true;

#endif
	attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);
}

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

void ICACHE_RAM_ATTR handle_post(void)
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

void ICACHE_RAM_ATTR connection_loss_handler(const WiFiEventStationModeDisconnected &evt)
{
	if(!sta_reconnect)
	{
		debugln("Lost connection with AP, attempting to reconnect\r\n");

//		led timer on, isimt if'us



		if(timer_initiated)
		{
			os_timer_disarm(&timer);
			timer_initiated = false;
		}

		if(wakaama_initiated)
		{
			lwm2m_client_close();
			lwm2m_network_close(client_context);
			wakaama_initiated = false;
		}

		WiFi.disconnect(true);

		st_status = true;
	}

	had_connected = true;
	sta_reconnect = true;
}

void ICACHE_FLASH_ATTR debugln(const char * format, ... )
{
#ifdef DEBUGLN
  char buffer[256];
  va_list args;
  va_start (args, format);
  vsnprintf (buffer, sizeof(buffer), format, args);
  Serial.print(buffer);
  va_end (args);
#endif
}

void ICACHE_FLASH_ATTR gpio_init(void)
{
	debugln("gpio init\r\n");

	pinMode(12, OUTPUT); //relay
	pinMode(13, OUTPUT); //led
	pinMode(0, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);

	digitalWrite(12, LOW);
	digitalWrite(13, HIGH);

}

void ICACHE_FLASH_ATTR gpio0_intr_handler(void)
{
	debugln("%s\r\n", __func__);

	digitalWrite(13, HIGH);

	detachInterrupt(digitalPinToInterrupt(0));

	tick = 0;
	intr = true;

	delayMicroseconds(1000 * 100);


}

void ICACHE_FLASH_ATTR timer_init(os_timer_t *ptimer,uint32_t milliseconds, bool repeat)
{
	debugln("timer init\r\n");
	os_timer_setfn(ptimer, timer_callback_lwm2m, NULL);
	os_timer_arm(ptimer, milliseconds, repeat);

	timer_initiated = true;
}

void ICACHE_FLASH_ATTR timer_callback_lwm2m(void *pArg)
{
	debugln("timer_callback_lwm2m\r\n");

#ifdef DEBUGLN
	debugln("Client state before step: %s\r\n", get_client_state(client_context->state));
	if(client_context->serverList != 0)
		debugln("Server state before step: %s\r\n", get_server_state(client_context->serverList->status));
#endif

	uint8_t step_result = lwm2m_step(client_context, &(tv.tv_sec));

	if(step_result != 0)
	{
		debugln("lwm2m_step() failed: 0x%X\r\n", step_result);
	}

#ifdef DEBUGLN
	debugln("Client state after step: %s\r\n", get_client_state(client_context->state));
	if(client_context->serverList != 0)
		debugln("Server state after step: %s\r\n", get_server_state(client_context->serverList->status));
#endif

	if(client_context->state == STATE_BOOTSTRAP_REQUIRED)
	{
		client_context->state = STATE_INITIAL;
	}
}

void ICACHE_FLASH_ATTR led_timer_init(os_timer_t *ptimer)
{
	debugln("led timer init\r\n");
	os_timer_setfn(ptimer, led_timer_callback, NULL);
	os_timer_arm(&led_timer, 100, true);
	led_mode = 2;
}

void ICACHE_FLASH_ATTR led_timer_callback(void *pArg)
{
	if(intr)
	{
		if(tick < 20)
		{
			if(digitalRead(0))
			{

				intr = false;
				attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);
			}

			tick++;
		}

		else if(tick >=20)
		{
			if(timer_initiated)
			{
				os_timer_disarm(&timer);
				timer_initiated = false;
			}

			if(wakaama_initiated)
			{
				lwm2m_client_close();
				lwm2m_network_close(client_context);
				wakaama_initiated = false;
			}

			digitalWrite(13, HIGH);

			attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);

			tick = 0;

			intr = false;
			ap_status = true;
		}
	}

	switch(led_mode)
	{
	case BLINK1: //AP mode
		if(tick % 4 == 0)
			digitalWrite(13, LOW);
		else if(tick % 2 == 0)
			digitalWrite(13, HIGH);
		break;

	case BLINK2: //device is alive
		if((tick % 30 == 0) || ((tick - 1) % 30 == 0))
			digitalWrite(13, LOW);
		else
			digitalWrite(13, HIGH);
		break;

	case BLINK3: //can't find AP, failed to connect
		if((tick % 30 == 0) || ((tick - 1) % 30 == 0))
			digitalWrite(13, HIGH);
		else
			digitalWrite(13, LOW);
		break;
	case BLINK4: //AP mode failed
		if((tick % 30 == 0) || ((tick - 1) % 30 == 0) || ((tick % 30 - 4) == 0) || ((tick - 5) % 30 == 0))
			digitalWrite(13, HIGH);
		else
			digitalWrite(13, LOW);
		break;
	}

	tick++;
}

#ifdef DEBUGLN
const char* ICACHE_FLASH_ATTR get_client_state(lwm2m_client_state_t state)
{
	switch(state)
	{
		case STATE_INITIAL: return "STATE_INITIAL";
		case STATE_BOOTSTRAP_REQUIRED: return "STATE_BOOTSTRAP_REQUIRED";
		case STATE_BOOTSTRAPPING: return "STATE_BOOTSTRAPPING";
		case STATE_REGISTER_REQUIRED: return "STATE_REGISTER_REQUIRED";
		case STATE_REGISTERING: return "STATE_REGISTERING";
		case STATE_READY: return "STATE_READY";
		default: return "STATE_ERROR";
	}
}

const char* ICACHE_FLASH_ATTR get_server_state(lwm2m_status_t state)
{
	switch(state)
	{
		case STATE_DEREGISTERED: return "STATE_DEREGISTERED";
		case STATE_REG_PENDING: return "STATE_REG_PENDING";
		case STATE_REGISTERED: return "STATE_REGISTERED";
		case STATE_REG_FAILED: return "STATE_REG_FAILED";
		case STATE_REG_UPDATE_PENDING: return "STATE_REG_UPDATE_PENDING";
		case STATE_REG_UPDATE_NEEDED: return "STATE_REG_UPDATE_NEEDED";
		case STATE_REG_FULL_UPDATE_NEEDED: return "STATE_REG_FULL_UPDATE_NEEDED";
		case STATE_DEREG_PENDING: return "STATE_DEREG_PENDING";
		case STATE_BS_HOLD_OFF: return "STATE_BS_HOLD_OFF";
		case STATE_BS_INITIATED: return "STATE_BS_INITIATED";
		case STATE_BS_PENDING: return "STATE_BS_PENDING";
		case STATE_BS_FINISHING: return "STATE_BS_FINISHING";
		case STATE_BS_FINISHED: return "STATE_BS_FINISHED";
		case STATE_BS_FAILING: return "STATE_BS_FAILING";
		case STATE_BS_FAILED: return "STATE_BS_FAILED";
		default: return "STATE_ERROR";
	}
}
#endif

void ICACHE_FLASH_ATTR wakaama_init(void)
{
	get_config_eeprom();

	device_data = lwm2m_device_data_get();
	device_data->manufacturer = "test manufacturer";
	device_data->model_name = "test model";
	device_data->device_type = "sensor";
	device_data->firmware_ver = "1.0";
	device_data->serial_number = "140234-645235-12353";

//    sukuria security, server ir device objektus, ideda juos i kliento
//    objektu sarasa

	debugln("lwm2m_client_init\r\n");
	client_context = lwm2m_client_init(buff.wak_client_name);

	if (client_context == 0)
	{
		debugln("Failed to initialize wakaama\r\n");
	}

	// Create objects
	relay_object = lwm2m_object_create(3312, false, relay_object_get_meta());
	lwm2m_object_instances_add(relay_object, relay_object_create_instance());
	lwm2m_add_object(client_context, relay_object);

//    sukuria network struktura, papildo kliento objekto userData lauka

	debugln("lwm2m_network_init\r\n");
	uint8_t status = lwm2m_network_init(client_context, NULL);
	if (status == 0)
	{
		debugln("Failed to open socket\r\n");
	}

//    paima objektu sarasa ir sukuria objektu instances, paskirsto atminti
//    bet nepapildo serveriu saraso

	debugln("lwm2m_add_server\r\n");
	status = (uint8_t)lwm2m_add_server(123, buff.wak_server, 100, false, NULL, NULL, 0);

	if (!status)
	{
		debugln("Failed to add server\r\n");
	}

	tv.tv_sec = 60;

	tv.tv_usec = 0;

	wakaama_initiated = true;
}

void ICACHE_RAM_ATTR set_config_eeprom(struct network_info buf)
{
	char crc[4];

	for(uint8_t j = 0; j < 4; j++)
	{
		crc[j] = *((uint8_t*)(&buf.crc) + j);
	}

	for(uint16_t i = 0; i < sizeof(buf); i++)
	{
		EEPROM.write(i, 0);
	}

	for(uint16_t i = offsetof(network_info, crc); i < offsetof(network_info, crc) + sizeof(buf.crc); i++)
	{
		EEPROM.write(i, crc[i - offsetof(network_info, crc)]);
	}

	for(uint16_t i = offsetof(network_info, sta_ssid); i < offsetof(network_info, sta_ssid) + sizeof(buff.sta_ssid); i++)
	{
		EEPROM.write(i, buf.sta_ssid[i - offsetof(network_info, sta_ssid)]);
	}

	for(uint16_t i = offsetof(network_info, sta_pass); i < offsetof(network_info, sta_pass) + sizeof(buff.sta_pass); i++)
	{
		EEPROM.write(i, buf.sta_pass[i - offsetof(network_info, sta_pass)]);
	}

	for(uint16_t i = offsetof(network_info, wak_server); i < offsetof(network_info, wak_server) + sizeof(buff.wak_server); i++)
	{
		EEPROM.write(i, buf.wak_server[i - offsetof(network_info, wak_server)]);
	}

	for(uint16_t i = offsetof(network_info, wak_client_name); i < offsetof(network_info, wak_client_name) + sizeof(buff.wak_client_name); i++)
	{
		EEPROM.write(i, buf.wak_client_name[i - offsetof(network_info, wak_client_name)]);
	}

	EEPROM.commit();
}

void ICACHE_RAM_ATTR get_config_eeprom(void)
{
	size_t max = EEPROM.length();
	uint8_t crc[4];

	for(uint16_t i = offsetof(network_info, sta_ssid); i < offsetof(network_info, sta_ssid) + sizeof(buff.sta_ssid); i++)
	{
		buff.sta_ssid[i - offsetof(network_info, sta_ssid)] = (char)EEPROM.read(i);
		if(i >= max)
		{
			debugln("EEPROM index over EEPROM.length() 1\r\n");
			return;
		}
	}

	for(uint16_t i = offsetof(network_info, sta_pass); i < offsetof(network_info, sta_pass) + sizeof(buff.sta_pass); i++)
	{
		buff.sta_pass[i - offsetof(network_info, sta_pass)] = (char)EEPROM.read(i);
		if(i >= max)
		{
			debugln("EEPROM index over EEPROM.length() 2\r\n");
			return;
		}
	}

	for(uint16_t i = offsetof(network_info, wak_server); i < offsetof(network_info, wak_server) + sizeof(buff.wak_server); i++)
	{
		buff.wak_server[i - offsetof(network_info, wak_server)] = (char)EEPROM.read(i);
		if(i >= max)
		{
			debugln("EEPROM index over EEPROM.length() 3\r\n");
			return;
		}
	}

	for(uint16_t i = offsetof(network_info, wak_client_name); i < offsetof(network_info, wak_client_name) + sizeof(buff.wak_client_name); i++)
	{
		buff.wak_client_name[i - offsetof(network_info, wak_client_name)] = (char)EEPROM.read(i);
		if(i >= max)
		{
			debugln("EEPROM index over EEPROM.length() 4\r\n");
			return;
		}
	}

	for(uint16_t i = offsetof(network_info, crc); i < offsetof(network_info, crc) + sizeof(buff.crc); i++)
	{
		crc[i - offsetof(network_info, crc)] = EEPROM.read(i);
	}

	buff.crc = (*(uint32_t*)(crc));
}

uint32_t ICACHE_RAM_ATTR gen_crc(const network_info *buff)
{
	uint8_t len = (sizeof(buff->sta_pass) + sizeof(buff->sta_ssid) + sizeof(buff->wak_client_name) + sizeof(buff->wak_server)) / 4;

	uint32_t *ptr = (uint32_t *)((uint8_t *)buff + offsetof(network_info, sta_ssid));

	uint32_t crc = 0xffffffff;

	for(uint8_t i = 0; i < len; i++)
	{
		crc = crc ^ *(ptr + i);
		for(uint8_t j = 0; j < 8; j++)
		{
			if(crc & 1)
			{
				crc = (crc >> 1) ^ poly;
			}
			else
			{
				crc = crc >> 1;
			}
		}
	}
	return ~crc;
}

