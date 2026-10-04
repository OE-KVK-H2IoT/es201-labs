# A0 — First Contact: the PicoBot C/Pico SDK bring-up program (SDK only).
#
# Standalone Pico SDK project for the Architecture of Embedded Systems lab A0.
# No helper library: the blink, buzzer, motors and failsafe are written in main.c
# with nothing but the SDK.
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(a0_first_contact C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(a0_first_contact main.c)
target_link_libraries(a0_first_contact
    pico_stdlib pico_cyw43_arch_none hardware_pwm hardware_clocks)
pico_enable_stdio_usb(a0_first_contact 1)
pico_enable_stdio_uart(a0_first_contact 1)
pico_add_extra_outputs(a0_first_contact)
