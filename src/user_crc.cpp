/*
 * user_crc.cpp
 *
 *  Created on: Jul 31, 2018
 *      Author: tautvydas
 */

//SDK
#include "user_interface.h"

//USER
#include "user_functions.h"

//ARDUINO
#include <EEPROM.h>

extern struct network_info buff;
u32 poly = 0x82f63b78;

void ICACHE_FLASH_ATTR set_config_eeprom(struct network_info buf)
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

void ICACHE_FLASH_ATTR get_config_eeprom(void)
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

uint32_t ICACHE_FLASH_ATTR gen_crc(const network_info *buff)
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

