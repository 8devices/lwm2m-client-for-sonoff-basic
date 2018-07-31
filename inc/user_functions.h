/*
 * user_functions.h
 *
 *  Created on: Jul 31, 2018
 *      Author: tautvydas
 */

#ifndef INC_USER_FUNCTIONS_H_
#define INC_USER_FUNCTIONS_H_

struct network_info
{
	uint32_t crc;
	char sta_ssid[32];
	char sta_pass[64];
	char wak_server[64];
	char wak_client_name[32];
};

#ifdef __cplusplus
extern "C" {
#endif

void ICACHE_FLASH_ATTR debugln(const char * format, ... );
void ICACHE_FLASH_ATTR set_config_eeprom(struct network_info buff);
void ICACHE_FLASH_ATTR get_config_eeprom(void);
uint32_t ICACHE_FLASH_ATTR gen_crc(const network_info *buff);
void ICACHE_FLASH_ATTR httpserver(void);
void ICACHE_FLASH_ATTR handle_post(void);
void ICACHE_RAM_ATTR handle_get(void);
void ICACHE_RAM_ATTR handle_not_found(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_USER_FUNCTIONS_H_ */
