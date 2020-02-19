#ifndef SHUTTERS_EVENTS_H
#define SHUTTERS_EVENTS_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "event_groups.h"

#define FIRST_STATUS_INFO_SENT_FLAG    (1 << 0)
#define UPDATE_FIRMWARE_FLAG           (1 << 1)
#define REQUEST_ERROR_OCCURRED_FLAG    (1 << 2)
#define STATUS_INFO_IS_BEING_SENT_FLAG (1 << 3)
#define WIFI_CONNECTED_FLAG            (1 << 4)
#define DELETE_TCP_SERVER_FLAG         (1 << 5)

void init_events();
void save_being_updated_event();
bool is_being_updated();
void save_request_error_occurred_event();
void clear_request_error_occurred_event();
bool is_request_error_occurred();
void save_sending_status_info_event();
void clear_sending_status_info_event();
bool is_status_info_being_sent();
bool is_first_status_info_sent();
void save_first_status_info_sent_event();
void save_connected_to_wifi_event();
void clear_connected_to_wifi_event();
bool is_connected_to_wifi();
void save_delete_tcp_server_event();
void clear_tcp_server_deletion_event();
bool is_tcp_server_to_be_deleted();

#endif //SHUTTERS_EVENTS_H
