cmake_minimum_required(VERSION 3.0.2)

project(netcfg)

get_directory_property(PROJECT_SOURCE_DIR DIRECTORY ../ DEFINITION PROJECT_SOURCE_DIR)
get_directory_property(INSTALL_DIR DIRECTORY ${PROJECT_SOURCE_DIR} DEFINITION INSTALL_DIR)

message("netcfgplugin PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}")

set(SOURCES
netcfgplugin.cpp
netif/netif.cpp
netif/netifs.cpp
netcfgplugin.cpp
plugincfg.cpp
farconnect.cpp
common/errname.c
common/log.c
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(netcfg MODULE ${SOURCES})
target_link_libraries(netcfg utils far2l)

target_compile_definitions(netcfg PRIVATE -DUSEUCD=OFF -DWINPORT_DIRECT -DUNICODE -DFAR_DONT_USE_INTERNALS)

target_include_directories(netcfg PRIVATE .)
target_include_directories(netcfg PRIVATE ${PROJECT_SOURCE_DIR}/utils/include)
target_include_directories(netcfg PRIVATE ${PROJECT_SOURCE_DIR}/far2l/far2sdk)
target_include_directories(netcfg PRIVATE ${PROJECT_SOURCE_DIR}/WinPort)

set_target_properties(netcfg
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${INSTALL_DIR}/Plugins/netcfg/plug"
        PREFIX ""
        SUFFIX ".far-plug-wide")

add_custom_command(TARGET netcfg POST_BUILD
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/configs
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/configs "${INSTALL_DIR}/Plugins/netcfg"
)
