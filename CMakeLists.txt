# Omni-P4 Smart Speaker Firmware
# Target: ESP32-P4-Function-EV-Board
# ESP-IDF: v5.3+

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(omni_p4)
