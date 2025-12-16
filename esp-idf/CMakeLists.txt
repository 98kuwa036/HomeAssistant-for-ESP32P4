# ESP32-P4 Smart Speaker with ReSpeaker USB Mic Array
# Using ESP-IDF + ESP-ADF for USB Audio Class support

cmake_minimum_required(VERSION 3.16)

# Set ESP-ADF path (adjust if needed)
set(EXTRA_COMPONENT_DIRS
    $ENV{ADF_PATH}/components
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32p4_smart_speaker)
