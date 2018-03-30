#pragma once

#include "wakaama_object_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t relay_write_cb(unsigned res_id, const lwm2m_data_t * dataP, bool* memberP);
lwm2m_object_meta_information_t* relay_object_get_meta();
lwm2m_list_t* relay_object_create_instance();
void ICACHE_FLASH_ATTR debugln(const char * format, ... );

#ifdef __cplusplus
}
#endif
