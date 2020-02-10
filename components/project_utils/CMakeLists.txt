#set(COMPONENT_ADD_INCLUDEDIRS   include
#        ../project_common/include
#        ../malloc_logger/include)
#set(COMPONENT_SRCS  "utils.c")
#register_component()

idf_component_register(SRCS "utils.c"
        INCLUDE_DIRS "include" "../project_common/include" "../malloc_logger/include"
        REQUIRES malloc_logger esp8266 tcpip_adapter)