# Lab 1 — First Contact: the PicoBot C/Pico SDK bring-up program (SDK only).
#
# Standalone Pico SDK project for the Architecture of Embedded Systems Lab 1.
# Two source files: the application (main.c) and our own driver (picobot.c).
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(lab1 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(lab1 main.c picobot.c)
target_include_directories(lab1 PRIVATE .)
target_link_libraries(lab1 pico_stdlib hardware_clocks)
pico_enable_stdio_usb(lab1 1)
pico_enable_stdio_uart(lab1 1)
pico_add_extra_outputs(lab1)
