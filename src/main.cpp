
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
#include "queue.h"

//WAKAAMA
#include "internal_objects.h"
#include "internal.h"
#include "lwm2m/c_connect.h"
#include "lwm2m/debug.h"
#include "lwm2mObjects/3312.h"

//ARDUINO
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Arduino.h>

//USER
#include "user_functions.h"

static volatile bool timer_initiated = false;
static volatile bool wakaama_initiated = false;
static volatile bool st_status = false;
static volatile bool ap_status = false;
static volatile bool sta_reconnect = true;
static volatile bool intr = false;
static volatile bool connection_loss_f = false;
static volatile bool led_timer_f = false;
static volatile bool had_connected = false;
static volatile bool step_flag = false;

struct timeval tv;

struct network_info buff;

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

#ifdef WIFIONLY
static time_t curr_time = 0;
#endif

uint8_t led_mode = 0;
uint8_t sta_tick = 0;
volatile uint8_t tick = 0;

using namespace KnownObjects;

lwm2m_client_context_t client_context;
id3312::object relay;
id3312::instance relayinst;
//device_instance_t * device_data = nullptr;
//lwm2m_object_t* relay_object = nullptr;
//lwm2m_object_t* led_object = nullptr;

WiFiEventHandler connection_loss;

uint16_t test_tick = 0;

void ICACHE_FLASH_ATTR gpio_init(void);
void ICACHE_FLASH_ATTR wifi_init(void);
void ICACHE_FLASH_ATTR wifi_ap_init(void);
void ICACHE_FLASH_ATTR timer_init(os_timer_t *ptimer,uint32_t milliseconds, bool repeat);
void ICACHE_RAM_ATTR timer_callback_lwm2m(void *pArg);
void ICACHE_FLASH_ATTR led_timer_init(os_timer_t *ptimer);
void ICACHE_RAM_ATTR led_timer_callback(void *pArg);
void ICACHE_RAM_ATTR gpio0_intr_handler(void);
void ICACHE_FLASH_ATTR wakaama_init(void);
void ICACHE_RAM_ATTR connection_loss_handler(const WiFiEventStationModeDisconnected &evt);
void ICACHE_FLASH_ATTR connection_deinit(void);
void ICACHE_FLASH_ATTR timer_deinit(void);
void ICACHE_FLASH_ATTR wakaama_step(void);
#ifdef DEBUGLN
//const char* ICACHE_FLASH_ATTR get_client_state(lwm2m_client_state_t state);
//const char* ICACHE_FLASH_ATTR get_server_state(lwm2m_status_t state);
const char* ICACHE_FLASH_ATTR get_wifi_fail(wl_status_t status);
#endif

void setup(void)
{
	Serial.begin(115200);
	while(!Serial)
	{
		yield();
	}
#ifdef DEBUGLN
	Serial.setDebugOutput(true);
#endif
	system_set_os_print(1);
	debugln("setup\r\n");

	WiFi.persistent(false);
	WiFi.setAutoConnect(false);
	WiFi.setAutoReconnect(false);
	WiFi.mode(WIFI_STA);

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
	{
		wifi_ap_init();
	}
#ifndef WIFI_AP_MODE
	else if(st_status)
	{
		wifi_init();
	}
#endif
	if(connection_loss_f)
	{
		connection_loss_f = false;
		connection_deinit();
	}
	if(led_timer_f)
	{
		led_timer_f = false;
		timer_deinit();
	}
	if(step_flag)
	{
		step_flag = false;
		wakaama_step();
	}
	yield();
}

void ICACHE_FLASH_ATTR wifi_init(void)
{
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

		while(int status = WiFi.status() != WL_CONNECTED)
		{
			if(sta_tick == 10)
			{
#ifdef DEBUGLN
				debugln("%s failed with status %s\r\n", __func__, get_wifi_fail((wl_status_t) status));
#endif

				led_mode = 3;

				st_status = true;
				return;
			}

			if(ap_status)
				return;

			delay(1000);

			sta_tick++;

			yield();
		}
	}


	else if(WiFi.getMode() == WIFI_AP)
	{
		debugln("Switching off AP mode - ");

		WiFi.softAPdisconnect(true);

		WiFi.disconnect(true);

		WiFi.mode(WIFI_STA);

		while(WiFi.status() == WL_DISCONNECTED) yield();

		debugln("successful\r\n");

		debugln("Connecting...\r\n");

		WiFi.begin(buff.sta_ssid, buff.sta_pass);

		sta_tick = 0;

		while(int status = WiFi.status() != WL_CONNECTED)
		{
			if(sta_tick == 10)
			{
#ifdef DEBUGLN
				debugln("%s failed with status %s\r\n", __func__, get_wifi_fail((wl_status_t) status));
#endif

				led_mode = 3;

				st_status = true;
				return;
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

		while(int status = WiFi.status() != WL_CONNECTED)
		{
			if(sta_tick == 10)
			{
#ifdef DEBUGLN
				debugln("%s failed with status %s\r\n", __func__, get_wifi_fail((wl_status_t) status));
#endif

				led_mode = 3;

				st_status = true;
				return;
			}

			if(ap_status)
				return;

			delay(1000);

			sta_tick++;

			yield();
		}
	}

	debugln("Connected\r\n");
	sta_reconnect = false;
#ifndef WIFIONLY
	wakaama_init();
#endif
	timer_init(&timer, 3000,true);
	led_mode = 2;

	if(!had_connected)
	{
		had_connected = true;
		connection_loss = WiFi.onStationModeDisconnected(&connection_loss_handler);
	}
	st_status = false;
}

void ICACHE_FLASH_ATTR wifi_ap_init(void)
{
	detachInterrupt(digitalPinToInterrupt(0));

	sta_reconnect = true;

	debugln("%s\r\n", __func__);

	debugln("Switching off station mode - ");

	WiFi.softAPdisconnect(true);

	WiFi.disconnect(true);

	WiFi.mode(WIFI_AP);

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
	led_mode = 2;

#ifndef WIFI_AP_MODE
	st_status = true;
#endif
	ap_status = false;
	attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);
}

void ICACHE_FLASH_ATTR connection_deinit(void)
{
	debugln("%s\r\n", __func__);
	if(!sta_reconnect)
	{
		debugln("Lost connection with AP, attempting to reconnect\r\n");

		if(timer_initiated)
		{
			os_timer_disarm(&timer);
			timer_initiated = false;
		}

		if(wakaama_initiated)
		{
			lwm2m_client_close(&client_context);
			wakaama_initiated = false;
		}

		WiFi.disconnect(true);
		st_status = true;
		sta_reconnect = true;
	}

}

void ICACHE_RAM_ATTR connection_loss_handler(const WiFiEventStationModeDisconnected &evt)
{
	if(!ap_status && !st_status && evt.reason!=8)
	{
		debugln("wifi disconnect reason = %d\r\n", evt.reason);
		connection_loss_f = true;
	}
}

#ifdef DEBUGLN
extern "C" void ICACHE_FLASH_ATTR debugln(const char * format, ... )
{
  char buffer[256];
  va_list args;
  va_start (args, format);
  vsnprintf (buffer, sizeof(buffer), format, args);
  Serial.print(buffer);
  va_end (args);
}
#else
void ICACHE_RAM_ATTR debugln(const char * format, ... )
{
}
#endif

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

void ICACHE_RAM_ATTR gpio0_intr_handler(void)
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

void ICACHE_FLASH_ATTR wakaama_step(void)
{
#ifndef WIFIONLY
	debugln("Time: %u\r\n", lwm2m_gettime());
	print_state(CTX(client_context));
	int result = lwm2m_process(CTX(client_context), &tv);
	if(result != 0)
		debugln("lwm2m_process failed with status 0x%02x\r\n", result);
	lwm2m_watch_and_reconnect(CTX(client_context), &tv, 20);
#else
	debugln("Time: %u\r\n", lwm2m_gettime());
#endif
}

void ICACHE_RAM_ATTR timer_callback_lwm2m(void *pArg)
{
	step_flag = true;
}

void ICACHE_FLASH_ATTR timer_deinit(void)
{
	debugln("%s\r\n", __func__);
	if(timer_initiated)
	{
		os_timer_disarm(&timer);
		timer_initiated = false;
	}

	if(wakaama_initiated)
	{
		lwm2m_client_close(&client_context);
		wakaama_initiated = false;
	}
	digitalWrite(13, HIGH);
	attachInterrupt(digitalPinToInterrupt(0), gpio0_intr_handler, FALLING);
	tick = 0;
	intr = false;
	ap_status = true;
}

void ICACHE_FLASH_ATTR led_timer_init(os_timer_t *ptimer)
{
	debugln("led timer init\r\n");
	os_timer_setfn(ptimer, led_timer_callback, NULL);
	os_timer_arm(&led_timer, 100, true);
	led_mode = 2;
}

void ICACHE_RAM_ATTR led_timer_callback(void *pArg)
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
			led_timer_f = true;
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
const char* ICACHE_FLASH_ATTR get_wifi_fail(wl_status_t status)
{
	switch(status)
	{
		case WL_NO_SHIELD: return "WL_NO_SHIELD";
		case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
		case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
		case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
		case WL_CONNECTED: return "WL_CONNECTED";
		case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
		case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
		case WL_DISCONNECTED: return "WL_DISCONNECTED";
		default: return "WL_STATE_UNDEFINED";
	}
}
#endif

void ICACHE_FLASH_ATTR wakaama_init(void)
{
	get_config_eeprom();

	debugln("lwm2m_client_init\r\n");
	if(!lwm2m_client_init(&client_context, buff.wak_client_name))
		debugln("Failed to initialize wakaama\r\n");

	debugln("lwm2m_add_server\r\n");
	if(!lwm2m_add_server(CTX(client_context), 123, buff.wak_server, 30, false))
		debugln("Failed to add server\r\n");

	client_context.deviceInstance.manufacturer = "test manufacturer";
	client_context.deviceInstance.model_name = "test model";
	client_context.deviceInstance.device_type = "sensor";
	client_context.deviceInstance.firmware_ver = "1.0";
	client_context.deviceInstance.serial_number = "140234-645235-12353";
#ifdef LWM2M_DEVICE_INFO_WITH_TIME
	client_context.deviceInstance.time_offset = 3;
	client_context.deviceInstance.timezone = "+03:00";
#endif

	relay.verifyWrite = [](id3312::instance* i, uint16_t res_id)
	{
		if(i->id == 0 && res_id == id3312::RESID::OnOff)
		{
			debugln("/3312/0/5850 write = %d\r\n", i->OnOff);
			digitalWrite(12, i->OnOff);
		}
		return true;
	};
	relayinst.id = 0;
	relay.addInstance(CTX(client_context), &relayinst);
	relay.registerObject(CTX(client_context), false);

	tv.tv_sec = 60;
	tv.tv_usec = 0;
	wakaama_initiated = true;
}

#ifdef LWM2M_WITH_LOGS
void lwm2m_printf(const char * format, ...)
{
    va_list ap, ap1;
    va_start(ap, format);
    va_start(ap1, format);
    char c;
    void *ptr;

    if(format == NULL)
    {
		debugln("LWM2M LOG aborted because of errors\r\n");
    	return;
    }
    for(uint8_t i = 0; i < (strlen(format)); i++)
    {
    	c = *(format + i);
		if(c=='%')
		{
	    	c = *(format + i + 1);
			switch(c)
			{
			case 'i':
			case 'd':
			case 'c':
				va_arg(ap, int);
				break;
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':
				va_arg(ap, double);
				break;
			case 'p':
				ptr = va_arg(ap, void*);
				if(ptr == NULL)
				{
					debugln("LWM2M LOG aborted because of errors\r\n");
					return;
				}
				break;
			case 's':
				ptr = va_arg(ap, void*);
				if(ptr == NULL)
				{
					debugln("LWM2M LOG aborted because of errors\r\n");
					return;
				}
				break;
			case 'o':
			case 'u':
			case 'x':
			case 'X':
				va_arg(ap, unsigned int);
				break;
			}
		}
    }
    char buffer[256];
    vsnprintf (buffer, sizeof(buffer), format, ap1);
    Serial.print(buffer);
    va_end(ap);
    va_end(ap1);
}
#endif


