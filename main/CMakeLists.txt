set(COMPONENT_ADD_INCLUDEDIRS   include
                                ../components/common/include
                                ../components/malloc_logger/include
                                ../components/utils/include
                                ../components/ota/include)
set(COMPONENT_SRCS  "user_main.c")
register_component()