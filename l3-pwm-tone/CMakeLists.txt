# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# L3 — PWM signal generation: sweep a tone on the buzzer.
# Standalone Pico SDK project (see the getting-started tutorial §2). Build:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l3_pwm_tone C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(l3_pwm_tone main.c)
target_link_libraries(l3_pwm_tone pico_stdlib hardware_pwm hardware_clocks)
pico_enable_stdio_usb(l3_pwm_tone 1)
pico_enable_stdio_uart(l3_pwm_tone 1)
pico_add_extra_outputs(l3_pwm_tone)
