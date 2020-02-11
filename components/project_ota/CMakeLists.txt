idf_component_register(SRCS "ota.c"
        INCLUDE_DIRS "include" "../project_common/include" "../project_utils/include"
        REQUIRES project_events app_update malloc_logger tcpip_adapter)