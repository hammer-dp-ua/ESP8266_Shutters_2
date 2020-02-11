#ifndef GENERAL_UTILS
#define GENERAL_UTILS

#include "device_settings.h"
#include "global_definitions.h"
#include "malloc_logger.h"
#include "events.h"

#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "string.h"
#include "lwip/ip4_addr.h"
#include "lwip/sys.h"
#include "lwip/inet.h"
#include "sys/socket.h"

#define WI_FI_RECONNECTION_INTERVAL_MS (10 * 1000)

#define RTC_MEM_BASE 0x60001000

void *set_string_parameters(const char string[], const char *parameters[]);
void wifi_init_sta(void (*on_connected)(), void (*on_disconnected)(), void (*on_connection)());
void rtc_mem_read(unsigned int addr, void *dst, unsigned int length);
void rtc_mem_write(unsigned int dst, const void *src, unsigned int length);
int connect_to_http_server();
char *send_request(char *request, unsigned short response_buffer_length, const unsigned int *milliseconds_counter);
void shutdown_and_close_socket(int socket);
void disable_wifi_event_handler();
char *get_value_of_get_request_parameter(char *request, char *parameter, bool *is_numeric_param_value, unsigned int *milliseconds_counter);
#endif
