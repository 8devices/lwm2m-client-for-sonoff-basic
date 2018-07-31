#ifndef _WAKAAMA_CONF
#define _WAKAAMA_CONF

#define LWM2M_LITTLE_ENDIAN
// Put the library in client mode (you basically always need this)
#define LWM2M_CLIENT_MODE

#define LWM2M_WITH_LOGS
//#define LWM2M_NETWORK_LOGGING

// Enable json support
//#define LWM2M_SUPPORT_JSON
//
//// Enables the wifi object where you may provide information about the wifi strength, connected ssid etc.
//#define LWM2M_DEV_INFO_WIFI_METRICS
//
//// Allows to perform a reboot of the device. Implement lwm2m_reboot() for this to work.
//#define LWM2M_DEVICE_WITH_REBOOT
//
//// Allows to perform a factory reset. Implement lwm2m_factory_reset() for this to work.
//// In this method you should erase all user memory, connection setups and so on.
//#define LWM2M_DEVICE_WITH_FACTORY_RESET
//
//// Implement lwm2m_get_bat_level() and lwm2m_get_bat_status().
//#define LWM2M_DEVICE_INFO_WITH_BATTERY
//
//// Implement lwm2m_get_free_mem() and lwm2m_get_total_mem().
//#define LWM2M_DEVICE_INFO_WITH_MEMINFO
//
//// Implement lwm2m_get_last_error() and lwm2m_reset_last_error().
//#define LWM2M_DEVICE_INFO_WITH_ERRCODE
//
//// Implement lwm2m_gettime() and update the fields  **timezone** and **time_offset**
//// of the device information object.
//#define LWM2M_DEVICE_INFO_WITH_TIME
//
//// Adds a **hardware_ver** and a **software_ver** c-string field.
//#define LWM2M_DEVICE_INFO_WITH_ADDITIONAL_VERSIONS
//
//// Enables the firmware update mechanism.
//#define LWM2M_FIRMWARE_UPGRADES
//
// Enable DTLS support (preshared and public key)
//#define LWM2M_WITH_DTLS

// Enable additional support for X.509 certificates and PEM data
// #define LWM2M_WITH_DTLS_X509

// Overwrite maximum packet size. Defaults to 1024 bytes
// #define MAX_PACKET_SIZE 1024

#endif
