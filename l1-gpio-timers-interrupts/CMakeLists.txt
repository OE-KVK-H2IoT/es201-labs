# L1 — GPIO, timers, interrupts: an ISR-driven blink that measures its own period.
# Standalone Pico SDK project (see the getting-started tutorial §2). Build:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l1_timer_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(l1_timer_blink main.c)
target_link_libraries(l1_timer_blink pico_stdlib)
pico_enable_stdio_usb(l1_timer_blink 1)
pico_enable_stdio_uart(l1_timer_blink 1)
pico_add_extra_outputs(l1_timer_blink)
