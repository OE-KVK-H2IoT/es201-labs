# A0 — First Contact: the PicoBot C/Pico SDK bring-up program.
#
# Standalone Pico SDK project for the Architecture of Embedded Systems lab A0.
# The PicoBot helper library lives in the sibling submodule (src/picobot/c); the
# default path assumes the standard courses checkout. Override if needed:
#     picolab build a0_first_contact PICOBOT_DIR=/path/to/picobot/c
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(a0_first_contact C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICOBOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../picobot/c" CACHE PATH
    "Directory containing picobot.c / picobot.h")
# Registered so picolab accepts PICOBOT_DIR=... as a build knob (it forwards it as -D).
foreach(opt PICOBOT_DIR)
    if(DEFINED ${opt} AND NOT "${${opt}}" STREQUAL "")
        set(PICOBOT_DIR "${${opt}}" CACHE PATH
            "Directory containing picobot.c / picobot.h" FORCE)
    endif()
endforeach()

if(NOT EXISTS "${PICOBOT_DIR}/picobot.c")
    message(FATAL_ERROR
        "picobot.c not found in ${PICOBOT_DIR} — the picobot submodule is not checked out, "
        "or pass -DPICOBOT_DIR=<path to picobot/c>")
endif()

pico_sdk_init()

add_executable(a0_first_contact main.c "${PICOBOT_DIR}/picobot.c")
target_include_directories(a0_first_contact PRIVATE "${PICOBOT_DIR}")
target_link_libraries(a0_first_contact
    pico_stdlib hardware_pwm hardware_i2c hardware_clocks)
pico_enable_stdio_usb(a0_first_contact 1)
pico_enable_stdio_uart(a0_first_contact 1)
pico_add_extra_outputs(a0_first_contact)
