/**
 * Pins 4 and 5 on some ESP8266-07 are exchanged on silk screen!!!
 *
 * Required connections:
 * GPIO15 to GND
 * GPIO0 to "+" OR to "GND" for flashing
 * EN to "+"
 */

#include "main.h"

static unsigned int milliseconds_counter_g;
static int signal_strength_g;
static unsigned short errors_counter_g = 0;
static unsigned short repetitive_request_errors_counter_g = 0;
static unsigned char pending_connection_errors_counter_g;
static unsigned int repetitive_ap_connecting_errors_counter_g = 0;
static unsigned int repetitive_tcp_server_errors_counter_g = 0;

static shutter_state shutters_states_g[2];
static int opened_sockets_g[2];

static esp_timer_handle_t milliseconds_time_serv_g;
static esp_timer_handle_t errors_checker_timer_g;
static esp_timer_handle_t blink_both_leds_timer_g;
static esp_timer_handle_t status_sender_timer_g;
static esp_timer_handle_t stop_shutters_activity_timer_g;
static esp_timer_handle_t blink_on_shutters_activity_g;

static SemaphoreHandle_t wirelessNetworkActionsSemaphore_g;

static TaskHandle_t tcp_server_task_g;

static void milliseconds_counter() {
   milliseconds_counter_g++;
}

static void start_100milliseconds_counter() {
   esp_timer_create_args_t timer_config = {
         .callback = &milliseconds_counter
   };

   ESP_ERROR_CHECK(esp_timer_create(&timer_config, &milliseconds_time_serv_g))
   ESP_ERROR_CHECK(esp_timer_start_periodic(milliseconds_time_serv_g, 1000 / MILLISECONDS_COUNTER_DIVIDER * 1000)) // 1000/10 = 100 ms
}

static void scan_access_point_task() {
   long rescan_when_connected_task_delay = 10 * 60 * 1000 / portTICK_RATE_MS; // 10 mins
   long rescan_when_not_connected_task_delay = 10 * 1000 / portTICK_RATE_MS; // 10 secs
   wifi_scan_config_t scan_config;
   unsigned short scanned_access_points_amount = 1;
   wifi_ap_record_t scanned_access_points[1];

   scan_config.ssid = (unsigned char *) ACCESS_POINT_NAME;
   scan_config.bssid = 0;
   scan_config.channel = 0;
   scan_config.show_hidden = false;

   for (;;) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nStart of Wi-Fi scanning... %u\n", milliseconds_counter_g);
      #endif

      xSemaphoreTake(wirelessNetworkActionsSemaphore_g, portMAX_DELAY);

      if (is_connected_to_wifi() && !is_being_updated() &&
            esp_wifi_scan_start(&scan_config, true) == ESP_OK &&
            esp_wifi_scan_get_ap_records(&scanned_access_points_amount, scanned_access_points) == ESP_OK) {
         signal_strength_g = scanned_access_points[0].rssi;

         #ifdef ALLOW_USE_PRINTF
         printf("Scanned %u access points", scanned_access_points_amount);
         for (unsigned short i = 0; i < scanned_access_points_amount; i++) {
            printf("\nScan index: %u, ssid: %s, rssi: %d", i, scanned_access_points[i].ssid, scanned_access_points[i].rssi);
         }
         printf("\n");
         #endif

         xSemaphoreGive(wirelessNetworkActionsSemaphore_g);
         vTaskDelay(rescan_when_connected_task_delay);
      } else {
         #ifdef ALLOW_USE_PRINTF
         printf("Wi-Fi scanning skipped. %u\n", milliseconds_counter_g);
         #endif

         xSemaphoreGive(wirelessNetworkActionsSemaphore_g);
         vTaskDelay(rescan_when_not_connected_task_delay);
      }
   }
}

static void blink_both_leds_cb() {
   if (gpio_get_level(AP_CONNECTION_STATUS_LED_PIN)) {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);
   } else {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
   }
}

static void init_both_leds_blinking_timer() {
   esp_timer_create_args_t timer_config = {
         .callback = &blink_both_leds_cb
   };

   ESP_ERROR_CHECK(esp_timer_create(&timer_config, &blink_both_leds_timer_g))
}

static void start_both_leds_blinking(unsigned int ms) {
   ESP_ERROR_CHECK(esp_timer_start_periodic(blink_both_leds_timer_g, ms * 1000))
}

static void stop_both_leds_blinking() {
   ESP_ERROR_CHECK(esp_timer_stop(blink_both_leds_timer_g))

   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
}

static void blink_on_send(gpio_num_t pin) {
   int initial_pin_state = gpio_get_level(pin);
   unsigned char i;

   for (i = 0; i < 4; i++) {
      bool set_pin = initial_pin_state == 1 ? i % 2 == 1 : i % 2 == 0;

      if (set_pin) {
         gpio_set_level(pin, 1);
      } else {
         gpio_set_level(pin, 0);
      }
      vTaskDelay(100 / portTICK_RATE_MS);
   }

   if (pin == AP_CONNECTION_STATUS_LED_PIN) {
      if (is_connected_to_wifi()) {
         gpio_set_level(pin, 1);
      } else {
         gpio_set_level(pin, 0);
      }
   }
}

static void on_response_error() {
   repetitive_request_errors_counter_g++;
   errors_counter_g++;
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
   save_request_error_occurred_event();
}

void send_status_info_task() {
   save_sending_status_info_event();
   xSemaphoreTake(wirelessNetworkActionsSemaphore_g, portMAX_DELAY);
   blink_on_send(SERVER_AVAILABILITY_STATUS_LED_PIN);
   for (unsigned char i = 0; i < 2; i++) {
      shutter_state current_state = shutters_states_g[i];

      if (current_state.state == SHUTTER_OPENING) {
         start_blinking_on_shutters_opening();
      } else if (current_state.state == SHUTTER_CLOSING) {
         start_blinking_on_shutters_closing();
      }
   }

   char signal_strength[5];
   snprintf(signal_strength, 5, "%d", signal_strength_g);
   char errors_counter[6];
   snprintf(errors_counter, 6, "%u", errors_counter_g);
   char pending_connection_errors_counter[4];
   snprintf(pending_connection_errors_counter, 4, "%u", pending_connection_errors_counter_g);
   char uptime[11];
   snprintf(uptime, 11, "%u", milliseconds_counter_g / MILLISECONDS_COUNTER_DIVIDER);
   char *build_timestamp = "";
   char free_heap_space[7];
   snprintf(free_heap_space, 7, "%u", esp_get_free_heap_size());
   char *reset_reason = "";
   char *system_restart_reason = "";

   #ifdef ROOM_SHUTTER
   char shutters_states[34];
   snprintf(shutters_states, 34, "{\"shutterNo\":%u, \"shutterState\":%u}",
         0, shutters_states_g[0].state);
   #endif
   #ifdef KITCHEN_SHUTTER
   char shutters_states[68];
   shutter_state shutter_1_state = shutters_states_g[0];
   shutter_state shutter_2_state = shutters_states_g[1];

   snprintf(shutters_states, 68, "{\"shutterNo\":%u, \"shutterState\":%u},{\"shutterNo\":%u, \"shutterState\":%u}",
         1, shutter_1_state.state, 2, shutter_2_state.state);
   #endif

   if (!is_first_status_info_sent()) {
      char build_timestamp_filled[30];
      snprintf(build_timestamp_filled, 30, "%s", __TIMESTAMP__);
      build_timestamp = build_timestamp_filled;

      esp_reset_reason_t rst_info = esp_reset_reason();

      switch (rst_info) {
         case ESP_RST_UNKNOWN:
            reset_reason = "Unknown";
            break;
         case ESP_RST_POWERON:
            reset_reason = "Power on";
            break;
         case ESP_RST_EXT:
            reset_reason = "Reset by external pin";
            break;
         case ESP_RST_SW:
            reset_reason = "Software";
            break;
         case ESP_RST_PANIC:
            reset_reason = "Exception/panic";
            break;
         case ESP_RST_INT_WDT:
            reset_reason = "Watchdog";
            break;
         case ESP_RST_TASK_WDT:
            reset_reason = "Task watchdog";
            break;
         case ESP_RST_WDT:
            reset_reason = "Other watchdog";
            break;
         case ESP_RST_DEEPSLEEP:
            reset_reason = "Deep sleep";
            break;
         case ESP_RST_BROWNOUT:
            reset_reason = "Brownout";
            break;
         case ESP_RST_SDIO:
            reset_reason = "SDIO";
            break;
         default:
            break;
      }

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type;

      rtc_mem_read(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);

      if (system_restart_reason_type == ACCESS_POINT_CONNECTION_ERROR) {
         int connection_error_code = 1;
         char system_restart_reason_inner[31];

         rtc_mem_read(CONNECTION_ERROR_CODE_RTC_ADDRESS, &connection_error_code, 4);

         snprintf(system_restart_reason_inner, 31, "AP connections error. Code: %d", connection_error_code);
         system_restart_reason = system_restart_reason_inner;
      } else if (system_restart_reason_type == REQUEST_CONNECTION_ERROR) {
         int connection_error_code = 1;
         char system_restart_reason_inner[25];

         rtc_mem_read(CONNECTION_ERROR_CODE_RTC_ADDRESS, &connection_error_code, 4);

         snprintf(system_restart_reason_inner, 25, "Requests error. Code: %d", connection_error_code);
         system_restart_reason = system_restart_reason_inner;
      } else if (system_restart_reason_type == SOFTWARE_UPGRADE) {
         system_restart_reason = "Software upgrade";
      }
   }

   const char *request_payload_template_parameters[] =
         {signal_strength, DEVICE_NAME, errors_counter, pending_connection_errors_counter, uptime, build_timestamp, free_heap_space,
          reset_reason, system_restart_reason, shutters_states, NULL};
   char *request_payload = set_string_parameters(STATUS_INFO_REQUEST_PAYLOAD_TEMPLATE, request_payload_template_parameters);

   #ifdef ALLOW_USE_PRINTF
   //printf("\nRequest payload: %s\n", request_payload);
   #endif

   unsigned short request_payload_length = strnlen(request_payload, 0xFFFF);
   char request_payload_length_string[6];
   snprintf(request_payload_length_string, 6, "%u", request_payload_length);
   const char *request_template_parameters[] = {request_payload_length_string, SERVER_IP_ADDRESS, request_payload, NULL};
   char *request = set_string_parameters(STATUS_INFO_POST_REQUEST, request_template_parameters);
   FREE(request_payload);

   #ifdef ALLOW_USE_PRINTF
   printf("\nCreated request: %s\nTime: %u\n", request, milliseconds_counter_g);
   #endif

   char *response = send_request(request, 255, &milliseconds_counter_g);

   FREE(request);

   if (response == NULL) {
      on_response_error();
   } else {
      if (strstr(response, RESPONSE_SERVER_SENT_OK)) {
         repetitive_request_errors_counter_g = 0;
         gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);

         if (!is_first_status_info_sent()) {
            save_first_status_info_sent_event();

            unsigned int overwrite_value = 0xFFFF;
            rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &overwrite_value, 4);
         }

         #ifdef ALLOW_USE_PRINTF
         printf("Response OK, time: %u\n", milliseconds_counter_g);
         #endif

         if (strstr(response, UPDATE_FIRMWARE)) {
            save_being_updated_event();
            start_both_leds_blinking(200);

            SYSTEM_RESTART_REASON_TYPE reason = SOFTWARE_UPGRADE;
            rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &reason, 4);

            // TCP server task will be deleted
            close_opened_sockets();

            update_firmware();
         }
      } else {
         on_response_error();
      }

      FREE(response);
   }

   clear_sending_status_info_event();
   xSemaphoreGive(wirelessNetworkActionsSemaphore_g);
   vTaskDelete(NULL);
}

static void send_status_info() {
   if (!is_connected_to_wifi() || is_status_info_being_sent() || is_being_updated()) {
      return;
   }

   xTaskCreate(send_status_info_task, SEND_STATUS_INFO_TASK_NAME, configMINIMAL_STACK_SIZE * 3, NULL, 1, NULL);
}

static void schedule_sending_status_info(unsigned int timeout_ms) {
   esp_timer_create_args_t timer_config = {
         .callback = &send_status_info
   };

   ESP_ERROR_CHECK(esp_timer_create(&timer_config, &status_sender_timer_g))
   ESP_ERROR_CHECK(esp_timer_start_periodic(status_sender_timer_g, timeout_ms * 1000))
}

static void start_blink_on_shutters_activity_timer(esp_timer_cb_t callback_param) {
   if (blink_on_shutters_activity_g != NULL) {
      ESP_ERROR_CHECK(esp_timer_stop(blink_on_shutters_activity_g))
      ESP_ERROR_CHECK(esp_timer_delete(blink_on_shutters_activity_g))
      blink_on_shutters_activity_g = NULL;
   }

   esp_timer_create_args_t timer_config = {
         .callback = callback_param
   };
   ESP_ERROR_CHECK(esp_timer_create(&timer_config, &blink_on_shutters_activity_g))
   ESP_ERROR_CHECK(esp_timer_start_periodic(blink_on_shutters_activity_g, 200 * 1000))
}

static void blink_on_shutters_opening_cb() {
   if (gpio_get_level(AP_CONNECTION_STATUS_LED_PIN)) {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   } else {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
   }
}

static void start_blinking_on_shutters_opening() {
   start_blink_on_shutters_activity_timer(&blink_on_shutters_opening_cb);
}

static void blink_on_shutters_closing_cb() {
   if (gpio_get_level(SERVER_AVAILABILITY_STATUS_LED_PIN)) {
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);
   } else {
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);
   }
}

static void start_blinking_on_shutters_closing() {
   start_blink_on_shutters_activity_timer(&blink_on_shutters_closing_cb);
}

static void stop_blinking_on_shutters_activity() {
   ESP_ERROR_CHECK(esp_timer_stop(blink_on_shutters_activity_g))
   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);

   if (is_connected_to_wifi()) {
      gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
      gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 1);
   }
}

static void stop_shutters_activity_task() {
   #ifdef ALLOW_USE_PRINTF
   printf("\n\nStopping shutters activity...\n");
   #endif

   #ifdef ROOM_SHUTTER
   shutter_state current_shutter_state = shutters_states_g[0];
   bool state_was_changed = false;

   switch (current_shutter_state.state) {
      case SHUTTER_OPENING:
         shutters_states_g[0].state = SHUTTER_OPENED;
         state_was_changed = true;
         break;
      case SHUTTER_CLOSING:
         shutters_states_g[0].state = SHUTTER_CLOSED;
         state_was_changed = true;
         break;
      default:
         break;
   }

   if (state_was_changed) {
      gpio_set_level(RELAY_DOWN_PIN, RELAY_PIN_DISABLED);
      gpio_set_level(RELAY_UP_PIN, RELAY_PIN_DISABLED);
   }

   rtc_mem_write(ROOM_SHUTTER_STATE_RTC_ADDRESS, &current_shutter_state.state, 4);
   #endif

   #ifdef KITCHEN_SHUTTER
   for (unsigned char i = 0; i < 2; i++) {
      shutter_state current_shutter_state = shutters_states_g[i];
      bool state_was_changed = false;

      switch (current_shutter_state.state) {
         case SHUTTER_OPENING:
            #ifdef ALLOW_USE_PRINTF
            printf("Shutter '%u' has been opened", current_shutter_state.shutter_no);
            #endif

            shutters_states_g[i].state = SHUTTER_OPENED;
            state_was_changed = true;
            break;
         case SHUTTER_CLOSING:
            #ifdef ALLOW_USE_PRINTF
            printf("Shutter '%u' has been closed", current_shutter_state.shutter_no);
            #endif

            shutters_states_g[i].state = SHUTTER_CLOSED;
            state_was_changed = true;
            break;
         default:
            break;
      }

      if (state_was_changed) {
         current_shutter_state = shutters_states_g[i];

         if (current_shutter_state.shutter_no == 1) {
            rtc_mem_write(KITCHEN_SHUTTER_1_STATE_RTC_ADDRESS, &current_shutter_state.state, 4);

            gpio_set_level(RELAY1_DOWN_PIN, RELAY_PIN_DISABLED);
            gpio_set_level(RELAY1_UP_PIN, RELAY_PIN_DISABLED);
         } else if (current_shutter_state.shutter_no == 2) {
            rtc_mem_write(KITCHEN_SHUTTER_2_STATE_RTC_ADDRESS, &current_shutter_state.state, 4);

            gpio_set_level(RELAY2_DOWN_PIN, RELAY_PIN_DISABLED);
            gpio_set_level(RELAY2_UP_PIN, RELAY_PIN_DISABLED);
         }
      }
   }
   #endif

   stop_blinking_on_shutters_activity();
   send_status_info();
   vTaskDelete(NULL);
}

// Callback to be invoked by timer
static void stop_shutters_activity_cb() {
   xTaskCreate(stop_shutters_activity_task, "stop_shutters", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY, NULL);
}

static void open_shutter(unsigned char opening_time_sec, unsigned char shutter_no, bool also_send_request) {
   unsigned int opening_time_ms = (unsigned int) opening_time_sec;
   opening_time_ms *= 1000;

   #ifdef ALLOW_USE_PRINTF
   printf("\nOpening shutter: %u, opening time: %ums, send request: %u\n",
         shutter_no, opening_time_ms, also_send_request);
   #endif

   ESP_ERROR_CHECK(esp_timer_stop(stop_shutters_activity_timer_g))
   ESP_ERROR_CHECK(esp_timer_start_once(stop_shutters_activity_timer_g, opening_time_ms * 1000))

   #ifdef ROOM_SHUTTER
   shutters_states_g[0].shutter_no = 0;
   shutters_states_g[0].state = SHUTTER_OPENING;
   rtc_mem_write(ROOM_SHUTTER_STATE_RTC_ADDRESS, &shutters_states_g[0].state, 4);

   if (gpio_get_level(RELAY_DOWN_PIN) == RELAY_PIN_ENABLED) {
      gpio_set_level(RELAY_DOWN_PIN, RELAY_PIN_DISABLED);
      vTaskDelay(500 / portTICK_RATE_MS);
   }
   gpio_set_level(RELAY_UP_PIN, RELAY_PIN_ENABLED);
   #endif

   #ifdef KITCHEN_SHUTTER
   if (shutter_no == 1) {
      shutters_states_g[0].shutter_no = 1;
      shutters_states_g[0].state = SHUTTER_OPENING;
      rtc_mem_write(KITCHEN_SHUTTER_1_STATE_RTC_ADDRESS, &shutters_states_g[0].state, 4);

      if (gpio_get_level(RELAY1_DOWN_PIN) == RELAY_PIN_ENABLED) {
         gpio_set_level(RELAY1_DOWN_PIN, RELAY_PIN_DISABLED);
         vTaskDelay(500 / portTICK_RATE_MS);
      }
      gpio_set_level(RELAY1_UP_PIN, RELAY_PIN_ENABLED);
   } else if (shutter_no == 2) {
      shutters_states_g[1].shutter_no = 2;
      shutters_states_g[1].state = SHUTTER_OPENING;
      rtc_mem_write(KITCHEN_SHUTTER_2_STATE_RTC_ADDRESS, &shutters_states_g[1].state, 4);

      if (gpio_get_level(RELAY2_DOWN_PIN) == RELAY_PIN_ENABLED) {
         gpio_set_level(RELAY2_DOWN_PIN, RELAY_PIN_DISABLED);
         vTaskDelay(500 / portTICK_RATE_MS);
      }
      gpio_set_level(RELAY2_UP_PIN, RELAY_PIN_ENABLED);
   }
   #endif

   if (also_send_request) {
      send_status_info();
   }
}

static void close_shutter(unsigned char closing_time_sec, unsigned char shutter_no, bool also_send_request) {
   unsigned int closing_time_ms = (unsigned int) closing_time_sec;
   closing_time_ms *= 1000;

   #ifdef ALLOW_USE_PRINTF
   printf("\nClosing shutter: %u, closing time: %ums, send request: %u\n",
         shutter_no, closing_time_ms, also_send_request);
   #endif

   ESP_ERROR_CHECK(esp_timer_stop(stop_shutters_activity_timer_g))
   ESP_ERROR_CHECK(esp_timer_start_once(stop_shutters_activity_timer_g, closing_time_ms * 1000))

   #ifdef ROOM_SHUTTER
   shutters_states_g[0].shutter_no = shutter_no;
   shutters_states_g[0].state = SHUTTER_CLOSING;
   rtc_mem_write(ROOM_SHUTTER_STATE_RTC_ADDRESS, &shutters_states_g[0].state, 4);

   if (gpio_get_level(RELAY_UP_PIN) == RELAY_PIN_ENABLED) {
      gpio_set_level(RELAY_UP_PIN, RELAY_PIN_DISABLED);
      vTaskDelay(500 / portTICK_RATE_MS);
   }
   gpio_set_level(RELAY_DOWN_PIN, RELAY_PIN_ENABLED);
   #endif

   #ifdef KITCHEN_SHUTTER
   if (shutter_no == 1) {
      shutters_states_g[0].shutter_no = shutter_no;
      shutters_states_g[0].state = SHUTTER_CLOSING;
      rtc_mem_write(KITCHEN_SHUTTER_1_STATE_RTC_ADDRESS, &shutters_states_g[0].state, 4);

      if (gpio_get_level(RELAY1_UP_PIN) == RELAY_PIN_ENABLED) {
         gpio_set_level(RELAY1_UP_PIN, RELAY_PIN_DISABLED);
         vTaskDelay(500 / portTICK_RATE_MS);
      }
      gpio_set_level(RELAY1_DOWN_PIN, RELAY_PIN_ENABLED);
   } else if (shutter_no == 2) {
      shutters_states_g[1].shutter_no = shutter_no;
      shutters_states_g[1].state = SHUTTER_CLOSING;
      rtc_mem_write(KITCHEN_SHUTTER_2_STATE_RTC_ADDRESS, &shutters_states_g[1].state, 4);

      if (gpio_get_level(RELAY2_UP_PIN) == RELAY_PIN_ENABLED) {
         gpio_set_level(RELAY2_UP_PIN, RELAY_PIN_DISABLED);
         vTaskDelay(500 / portTICK_RATE_MS);
      }
      gpio_set_level(RELAY2_DOWN_PIN, RELAY_PIN_ENABLED);
   }
   #endif

   if (also_send_request) {
      send_status_info();
   }
}

static void process_request_and_send_response(char *request, int socket) {
   #ifdef ALLOW_USE_PRINTF
   printf("All data received\n");
   #endif

   char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
   int sent = write(socket, response, strlen(response));

   if (sent < 0) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nError occurred during sending\n");
      #endif

      repetitive_tcp_server_errors_counter_g++;
   } else {
      #ifdef ALLOW_USE_PRINTF
      printf("Sent %d bytes\n", sent);
      #endif
   }

   bool is_duration_value_numeric = false;
   char *opening_activity_duration = get_value_of_get_request_parameter(request, "open", &is_duration_value_numeric, &milliseconds_counter_g);
   bool is_shutter_no_numeric = false;
   char *shutter_no = get_value_of_get_request_parameter(request, "shutter_no", &is_shutter_no_numeric, &milliseconds_counter_g);
   unsigned char shutter_no_numeric = 0;

   if (shutter_no != NULL) {
      if (is_shutter_no_numeric) {
         shutter_no_numeric = (unsigned char) atoi(shutter_no);
      }

      FREE(shutter_no);
   }

   if (opening_activity_duration != NULL) {
      if (is_duration_value_numeric) {
         int opening_time = atoi(opening_activity_duration);

         if (opening_time > 0) {
            open_shutter((unsigned char) opening_time, shutter_no_numeric, true);
         }
      }

      FREE(opening_activity_duration);
   }

   char *closing_activity_duration = get_value_of_get_request_parameter(request, "close", &is_duration_value_numeric, &milliseconds_counter_g);

   if (closing_activity_duration != NULL) {
      if (is_duration_value_numeric) {
         int closing_time = atoi(closing_activity_duration);

         if (closing_time > 0) {
            close_shutter((unsigned char) closing_time, shutter_no_numeric, true);
         }
      }

      FREE(closing_activity_duration);
   }
}

static void tcp_server_task() {
   while (true) {
      if (is_being_updated()) {
         // If some request is receipted during update
         #ifdef ALLOW_USE_PRINTF
         printf("tcp_server_task is to be removed\n");
         #endif

         vTaskDelete(NULL);
      }

      if (!is_connected_to_wifi()) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nNot connected to AP for TCP server, time: %u\n", milliseconds_counter_g);
         #endif

         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

      if (listen_socket == -1) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nError on socket opening, time: %u\n", milliseconds_counter_g);
         #endif

         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      opened_sockets_g[0] = listen_socket;

      #ifdef ALLOW_USE_PRINTF
      printf("\nSocket %d created, time: %u\n", listen_socket, milliseconds_counter_g);
      #endif

      struct sockaddr_in sock_addr;
      sock_addr.sin_addr.s_addr = INADDR_ANY;
      sock_addr.sin_family = AF_INET;
      sock_addr.sin_port = htons(LOCAL_SERVER_PORT);

      int ret = bind(listen_socket, (struct sockaddr*) &sock_addr, sizeof(sock_addr));

      if (ret != 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nSocked %d binding error, time: %u\n", listen_socket, milliseconds_counter_g);
         #endif

         shutdown_and_close_socket(listen_socket);
         opened_sockets_g[0] = -1;
         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      #ifdef ALLOW_USE_PRINTF
      printf("Socket %d binding OK. Listening..., time: %u\n", listen_socket, milliseconds_counter_g);
      #endif

      ret = listen(listen_socket, 5);

      if (ret != 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nSocket %d listening error, time: %u\n", listen_socket, milliseconds_counter_g);
         #endif

         shutdown_and_close_socket(listen_socket);
         opened_sockets_g[0] = -1;
         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }

      #ifdef ALLOW_USE_PRINTF
      printf("Socket %d listening OK, time: %u\n", listen_socket, milliseconds_counter_g);
      #endif

      struct sockaddr_in client_addr;
      unsigned int addr_len = sizeof(client_addr);
      // Blocks here until a request
      int accept_socket = accept(listen_socket, (struct sockaddr *) &client_addr, &addr_len);

      if (is_being_updated()) {
         continue;
      }

      if (accept_socket < 0) {
         #ifdef ALLOW_USE_PRINTF
         printf("\nUnable to accept connection: errno %d\n", errno);
         #endif

         shutdown_and_close_socket(listen_socket);
         repetitive_tcp_server_errors_counter_g++;
         vTaskDelay(2000 / portTICK_RATE_MS);
         continue;
      }
      opened_sockets_g[1] = accept_socket;

      #ifdef ALLOW_USE_PRINTF
      printf("\nSocket %d accepted\n", accept_socket);
      #endif

      // Buffer size should always be enough, because for GET requests only the beginning of requests is required
      unsigned short rx_buffer_size = 300;
      char rx_buffer[rx_buffer_size];

      while (true) {
         #ifdef ALLOW_USE_PRINTF
         printf("Receiving..., time: %u\n", milliseconds_counter_g);
         #endif

         int received_bytes = read(accept_socket, rx_buffer, rx_buffer_size - 1);

         if (received_bytes < 0) {
            #ifdef ALLOW_USE_PRINTF
            printf("\nReceive failed. Error no.: %d, time: %u\n", received_bytes, milliseconds_counter_g);
            #endif

            repetitive_tcp_server_errors_counter_g++;
            break;
         } else if (received_bytes == 0) {
            break;
         } else {
            rx_buffer[received_bytes] = 0; // Null-terminate whatever we received and treat like a string

            #ifdef ALLOW_USE_PRINTF
            char addr_str[20];
            inet_ntoa_r(((struct sockaddr_in *) &client_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
            printf("Received %d bytes from %s\n", received_bytes, addr_str);
            printf("Content: %s\n", rx_buffer);
            #endif

            process_request_and_send_response(rx_buffer, accept_socket);
         }
      }

      #ifdef ALLOW_USE_PRINTF
      printf("Shutting down sockets %d and %d, restarting...\n", accept_socket, listen_socket);
      #endif

      close_opened_sockets();
      repetitive_tcp_server_errors_counter_g = 0;
   }
}

static void pins_config() {
   gpio_config_t output_pins;
   output_pins.mode = GPIO_MODE_OUTPUT;

   #ifdef ROOM_SHUTTER
   output_pins.pin_bit_mask = (1<<AP_CONNECTION_STATUS_LED_PIN) | (1<<SERVER_AVAILABILITY_STATUS_LED_PIN)
         | (1<<RELAY_DOWN_PIN) | (1<<RELAY_UP_PIN);
   #endif
   #ifdef KITCHEN_SHUTTER
   output_pins.pin_bit_mask = (1<<AP_CONNECTION_STATUS_LED_PIN) | (1<<SERVER_AVAILABILITY_STATUS_LED_PIN)
         | (1<<RELAY1_DOWN_PIN) | (1<<RELAY1_UP_PIN) | (1<<RELAY2_DOWN_PIN) | (1<<RELAY2_UP_PIN);
   #endif

   output_pins.pull_up_en = GPIO_PULLUP_DISABLE;
   output_pins.pull_down_en = GPIO_PULLDOWN_DISABLE;

   gpio_config(&output_pins);

   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);

   #ifdef ROOM_SHUTTER
   gpio_set_level(RELAY_DOWN_PIN, RELAY_PIN_DISABLED);
   gpio_set_level(RELAY_UP_PIN, RELAY_PIN_DISABLED);
   #endif
   #ifdef KITCHEN_SHUTTER
   gpio_set_level(RELAY1_DOWN_PIN, RELAY_PIN_DISABLED);
   gpio_set_level(RELAY1_UP_PIN, RELAY_PIN_DISABLED);
   gpio_set_level(RELAY2_DOWN_PIN, RELAY_PIN_DISABLED);
   gpio_set_level(RELAY2_UP_PIN, RELAY_PIN_DISABLED);
   #endif
}

static void close_opened_sockets() {
   shutdown_and_close_socket(opened_sockets_g[0]);
   opened_sockets_g[0] = -1;
   shutdown_and_close_socket(opened_sockets_g[1]);
   opened_sockets_g[1] = -1;
}

void on_wifi_connected_task() {
   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 1);
   repetitive_ap_connecting_errors_counter_g = 0;

   xTaskCreate(tcp_server_task, "tcp_server_task", configMINIMAL_STACK_SIZE * 3, NULL, 1, &tcp_server_task_g);
   send_status_info();

   vTaskDelete(NULL);
}

void on_wifi_connected() {
   xTaskCreate(on_wifi_connected_task, "on_wifi_connected_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

void on_wifi_disconnected_task() {
   repetitive_ap_connecting_errors_counter_g++;
   gpio_set_level(AP_CONNECTION_STATUS_LED_PIN, 0);
   gpio_set_level(SERVER_AVAILABILITY_STATUS_LED_PIN, 0);

   vTaskDelete(tcp_server_task_g);
   close_opened_sockets();
   vTaskDelete(NULL);
}

void on_wifi_disconnected() {
   xTaskCreate(on_wifi_disconnected_task, "on_wifi_disconnected_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

static void blink_on_wifi_connection_task() {
   blink_on_send(AP_CONNECTION_STATUS_LED_PIN);
   vTaskDelete(NULL);
}

void blink_on_wifi_connection() {
   xTaskCreate(blink_on_wifi_connection_task, "blink_on_wifi_connection_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

static void uart_config() {
   uart_config_t uart_config;
   uart_config.baud_rate = 115200;
   uart_config.data_bits = UART_DATA_8_BITS;
   uart_config.parity    = UART_PARITY_DISABLE;
   uart_config.stop_bits = UART_STOP_BITS_1;
   uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
   uart_param_config(UART_NUM_0, &uart_config);
}

/**
 * Created as a workaround to handle unknown issues.
 */
static void check_errors_amount() {
   bool restart = false;

   if (repetitive_request_errors_counter_g >= MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nRequest errors amount: %u\n", repetitive_request_errors_counter_g);
      #endif

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type = REQUEST_CONNECTION_ERROR;

      rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);
      restart = true;
   } else if (repetitive_ap_connecting_errors_counter_g >= MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nAP connection errors amount: %u\n", repetitive_ap_connecting_errors_counter_g);
      #endif

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type = ACCESS_POINT_CONNECTION_ERROR;

      rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);
      restart = true;
   } else if (repetitive_tcp_server_errors_counter_g >= MAX_REPETITIVE_ALLOWED_ERRORS_AMOUNT + 10) {
      #ifdef ALLOW_USE_PRINTF
      printf("\nTCP server errors amount: %u\n", repetitive_ap_connecting_errors_counter_g);
      #endif

      SYSTEM_RESTART_REASON_TYPE system_restart_reason_type = TCP_SERVER_ERROR;

      rtc_mem_write(SYSTEM_RESTART_REASON_TYPE_RTC_ADDRESS, &system_restart_reason_type, 4);
      restart = true;
   }

   if (restart) {
      esp_restart();
   }
}

static void init_shutters_states() {
   esp_timer_create_args_t shutters_activity_timer_config = {
         .callback = &stop_shutters_activity_cb
   };
   ESP_ERROR_CHECK(esp_timer_create(&shutters_activity_timer_config, &stop_shutters_activity_timer_g))

   unsigned char closing_time_sec = 30;
   esp_reset_reason_t rst_info = esp_reset_reason();

   #ifdef ROOM_SHUTTER
   unsigned int saved_shutter_state;
   rtc_mem_read(ROOM_SHUTTER_STATE_RTC_ADDRESS, &saved_shutter_state, 4);

   if (saved_shutter_state == 0 || saved_shutter_state > SHUTTER_CLOSED || rst_info == ESP_RST_POWERON) {
      close_shutter(closing_time_sec, 0, false);
   } else {
      shutters_states_g[0].shutter_no = 0;
      shutters_states_g[0].state = saved_shutter_state;
   }
   #endif

   #ifdef KITCHEN_SHUTTER
   unsigned int saved_shutter_1_state;
   rtc_mem_read(KITCHEN_SHUTTER_1_STATE_RTC_ADDRESS, &saved_shutter_1_state, 4);
   unsigned int saved_shutter_2_state;
   rtc_mem_read(KITCHEN_SHUTTER_2_STATE_RTC_ADDRESS, &saved_shutter_2_state, 4);

   if (saved_shutter_1_state == 0 || saved_shutter_1_state > SHUTTER_CLOSED || rst_info == ESP_RST_POWERON) {
      close_shutter(closing_time_sec, 1, false);
   } else {
      shutters_states_g[0].shutter_no = 1;
      shutters_states_g[0].state = saved_shutter_1_state;
   }
   if (saved_shutter_2_state == 0 || saved_shutter_2_state > SHUTTER_CLOSED || rst_info == ESP_RST_POWERON) {
      close_shutter(closing_time_sec, 2, false);
   } else {
      shutters_states_g[1].shutter_no = 2;
      shutters_states_g[1].state = saved_shutter_2_state;
   }
   #endif
}

static void schedule_errors_checker(unsigned int timeout_ms) {
   esp_timer_create_args_t timer_config = {
         .callback = &check_errors_amount
   };

   ESP_ERROR_CHECK(esp_timer_create(&timer_config, &errors_checker_timer_g))
   ESP_ERROR_CHECK(esp_timer_start_periodic(errors_checker_timer_g, timeout_ms * 1000))
}

void app_main(void) {
   pins_config();
   uart_config();

   init_events();

   init_both_leds_blinking_timer();
   start_both_leds_blinking(100);
   vTaskDelay(5000 / portTICK_RATE_MS);
   stop_both_leds_blinking();

   #ifdef ALLOW_USE_PRINTF
   const esp_partition_t *running = esp_ota_get_running_partition();
   printf("\nRunning partition type: label: %s, %d, subtype: %d, offset: 0x%X, size: 0x%X, build timestamp: %s\n",
         running->label, running->type, running->subtype, running->address, running->size, __TIMESTAMP__);
   #endif

   tcpip_adapter_init();
   tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA); // Stop DHCP client
   tcpip_adapter_ip_info_t ip_info;
   ip_info.ip.addr = inet_addr(OWN_IP_ADDRESS);
   ip_info.gw.addr = inet_addr(OWN_GETAWAY_ADDRESS);
   ip_info.netmask.addr = inet_addr(OWN_NETMASK);
   tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

   init_shutters_states();

   wirelessNetworkActionsSemaphore_g = xSemaphoreCreateBinary();
   xSemaphoreGive(wirelessNetworkActionsSemaphore_g);

   wifi_init_sta(on_wifi_connected, on_wifi_disconnected, blink_on_wifi_connection);

   xTaskCreate(scan_access_point_task, "scan_access_point_task", configMINIMAL_STACK_SIZE * 3, NULL, 1, NULL);

   schedule_errors_checker(ERRORS_CHECKER_INTERVAL_MS);
   schedule_sending_status_info(STATUS_REQUESTS_SEND_INTERVAL_MS);

   start_100milliseconds_counter();
}