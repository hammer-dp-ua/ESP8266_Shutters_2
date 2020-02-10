#set(COMPONENT_ADD_INCLUDEDIRS   include
#        ../project_common/include
#        ../project_utils/include)
#set(COMPONENT_SRCS  "ota.c")
#register_component()

idf_component_register(SRCS "ota.c"
        INCLUDE_DIRS "include" "../project_common/include" "../project_utils/include"
        REQUIRES app_update malloc_logger tcpip_adapter)