cmake_minimum_required(VERSION 3.13)

# Pull in the Pico SDK (set PICO_SDK_PATH, or pass -DPICO_SDK_FETCH_FROM_GIT=ON)
include(pico_sdk_import.cmake)

project(rp2040_hid C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(rp2040_hid
    src/main.c
    src/usb_descriptors.c
)

target_include_directories(rp2040_hid PRIVATE src)

target_link_libraries(rp2040_hid PRIVATE
    pico_stdlib
    pico_unique_id
    hardware_adc
    hardware_spi
    tinyusb_device
)

pico_add_extra_outputs(rp2040_hid)
