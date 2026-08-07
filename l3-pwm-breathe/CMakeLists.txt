# L3 (PWM, variant) — gamma-corrected PWM "breathing" LED on GP16/GP17.
# One folder, one program — a standalone Pico project. Build:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l3_pwm_breathe C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(l3_pwm_breathe breathe.c)
# `m` = the C math library, for sinf()/powf() used to build the brightness table.
target_link_libraries(l3_pwm_breathe pico_stdlib hardware_pwm m)
pico_enable_stdio_usb(l3_pwm_breathe 1)
pico_enable_stdio_uart(l3_pwm_breathe 1)
pico_add_extra_outputs(l3_pwm_breathe)
