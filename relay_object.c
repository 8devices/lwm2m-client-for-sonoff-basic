/*
 MIT Licence
 David Graeff <david.graeff@web.de>
*/

/*
 * Implements an object for screen control
 *
 *                 Multiple
 * Object |  ID  | Instances | Mandatory |
 *  Test  | 3312 |    No     |    No     |
 *
 *  Ressources:
 *                  Supported     Multiple
 *  Name   |   ID | Operations | Instances | Mandatory |  Type   | Range | Units |      Description      |
 *  onoff  |  5850|    R/W     |    No     |    Yes    |   Bool  |       |       |                       |
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <Arduino.h>
#include "relay_object.h"

#include "osapi.h"
#include "user_interface.h"


typedef struct _relay_object_instance_
{
    struct relay_object_instance_t * next;   // matches lwm2m_list_t::next
    uint16_t instanceId;                       // matches lwm2m_list_t::id

    bool	onoff;
    uint8_t dimmer;
    uint16_t ontime;
    float cap;
    float pf;

} relay_object_instance_t;


bool relay_object_write_verify_cb(lwm2m_list_t* instance, uint16_t changed_res_id);


OBJECT_META(relay_object_instance_t, relay_object_meta, relay_object_write_verify_cb,
    {5850, O_RES_RW|O_RES_BOOL,  offsetof(relay_object_instance_t, onoff)},
	{5851, O_RES_W|O_RES_UINT8,  offsetof(relay_object_instance_t, dimmer)},
	{5852, O_RES_RW|O_RES_UINT16,  offsetof(relay_object_instance_t, ontime)},
	{5805, O_RES_R|O_RES_DOUBLE,  offsetof(relay_object_instance_t, cap)},
	{5820, O_RES_R|O_RES_DOUBLE,  offsetof(relay_object_instance_t, pf)},
	{0, O_RES_E                                     ,  0}
);



lwm2m_object_meta_information_t *relay_object_get_meta() {
    return (lwm2m_object_meta_information_t*)relay_object_meta;
}

lwm2m_list_t* relay_object_create_instance()
{
    relay_object_instance_t * targetP = (relay_object_instance_t *)malloc(sizeof(relay_object_instance_t));

    if (NULL == targetP) return NULL;

    memset(targetP, 0, sizeof(relay_object_instance_t));

    targetP->instanceId = 0;
    targetP->onoff = false;

    GPIO_OUTPUT_SET(GPIO_ID_PIN(12),0);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(13),1);

    return (lwm2m_list_t*)targetP;
}

bool relay_object_write_verify_cb(lwm2m_list_t* instance, uint16_t changed_res_id)
{
}

uint8_t relay_write_cb(unsigned res_id, const lwm2m_data_t * dataP, bool* memberP)
{
	if(res_id == 5850)
	{
		if(*memberP == 1)
		{
			GPIO_OUTPUT_SET(GPIO_ID_PIN(12),1);
			GPIO_OUTPUT_SET(GPIO_ID_PIN(13),0);
			return COAP_204_CHANGED;
		}
		else if(*memberP == 0)
		{
			GPIO_OUTPUT_SET(GPIO_ID_PIN(12),0);
			GPIO_OUTPUT_SET(GPIO_ID_PIN(13),1);
			return COAP_204_CHANGED;
		}

		return COAP_402_BAD_OPTION;
	}
}

