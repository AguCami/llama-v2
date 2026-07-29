# Proyecto ESP-IDF: mascota virtual 3D para Waveshare ESP32-S3-Touch-LCD-1.83
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(llama_virtual_3d)
