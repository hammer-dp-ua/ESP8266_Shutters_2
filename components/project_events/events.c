#include "events.h"

static EventGroupHandle_t flags_g = NULL;

void init_events() {
   if (flags_g == NULL) {
      flags_g = xEventGroupCreate();
   }
}

void save_being_updated_event() {
   xEventGroupSetBits(flags_g, UPDATE_FIRMWARE_FLAG);
}

bool is_being_updated() {
   return (xEventGroupGetBits(flags_g) & UPDATE_FIRMWARE_FLAG);
}

void save_request_error_occurred_event() {
   xEventGroupSetBits(flags_g, REQUEST_ERROR_OCCURRED_FLAG);
}

void clear_request_error_occurred_event() {
   xEventGroupClearBits(flags_g, REQUEST_ERROR_OCCURRED_FLAG);
}

bool is_request_error_occurred() {
   return xEventGroupGetBits(flags_g) & REQUEST_ERROR_OCCURRED_FLAG;
}

void save_sending_status_info_event() {
   xEventGroupSetBits(flags_g, STATUS_INFO_IS_BEING_SENT_FLAG);
}

void clear_sending_status_info_event() {
   xEventGroupClearBits(flags_g, STATUS_INFO_IS_BEING_SENT_FLAG);
}

bool is_status_info_being_sent() {
   return xEventGroupGetBits(flags_g) & STATUS_INFO_IS_BEING_SENT_FLAG;
}

void save_first_status_info_sent_event() {
   xEventGroupSetBits(flags_g, FIRST_STATUS_INFO_SENT_FLAG);
}

bool is_first_status_info_sent() {
   return xEventGroupGetBits(flags_g) & FIRST_STATUS_INFO_SENT_FLAG;
}

void save_connected_to_wifi_event() {
   xEventGroupSetBits(flags_g, WIFI_CONNECTED_FLAG);
}

void clear_connected_to_wifi_event() {
   xEventGroupClearBits(flags_g, WIFI_CONNECTED_FLAG);
}

bool is_connected_to_wifi() {
   return xEventGroupGetBits(flags_g) & WIFI_CONNECTED_FLAG;
}

void save_delete_tcp_server_event() {
   xEventGroupSetBits(flags_g, DELETE_TCP_SERVER_FLAG);
}

void clear_tcp_server_deletion_event() {
   xEventGroupClearBits(flags_g, DELETE_TCP_SERVER_FLAG);
}

bool is_tcp_server_to_be_deleted() {
   return xEventGroupGetBits(flags_g) & DELETE_TCP_SERVER_FLAG;
}