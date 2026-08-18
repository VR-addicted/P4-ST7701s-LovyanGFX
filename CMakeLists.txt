cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(P4-ST7701s-FB)

littlefs_create_partition_image(storage data FLASH_IN_PROJECT)

