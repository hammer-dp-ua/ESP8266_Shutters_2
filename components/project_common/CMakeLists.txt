set(COMPONENT_ADD_INCLUDEDIRS include)
register_component()

configure_file(include/global_definitions.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/global_definitions.h)