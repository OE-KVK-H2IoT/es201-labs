# L0 — Buttons: read a button and drive an LED, with serial narration.
# One folder, one program — a standalone Pico project. Build:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l0_buttons C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(l0_buttons buttons.c)
target_link_libraries(l0_buttons pico_stdlib)
pico_enable_stdio_usb(l0_buttons 1)
pico_enable_stdio_uart(l0_buttons 1)
pico_add_extra_outputs(l0_buttons)
