cmake_minimum_required(VERSION 3.16)

# Add the sx127x component directory
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components/sx127x)

# Include the project cmake file
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
project(esp32-lora-transmitter)