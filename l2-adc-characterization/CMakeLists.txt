# L2 — ADC characterization: sample the joystick, print raw counts and volts.
# Standalone Pico SDK project (see the getting-started tutorial §2). Build:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l2_adc_read C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(l2_adc_read main.c)
target_link_libraries(l2_adc_read pico_stdlib hardware_adc)
pico_enable_stdio_usb(l2_adc_read 1)
pico_enable_stdio_uart(l2_adc_read 1)
pico_add_extra_outputs(l2_adc_read)
