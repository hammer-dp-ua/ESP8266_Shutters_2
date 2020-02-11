idf_component_register(SRCS "utils.c"
        INCLUDE_DIRS "include"
        REQUIRES project_events malloc_logger esp8266 tcpip_adapter)