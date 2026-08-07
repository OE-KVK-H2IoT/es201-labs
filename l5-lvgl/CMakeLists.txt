# L5 — LVGL GUI. Reuses the L4 ST7796 + GT911 drivers and builds LVGL (the git
# submodule under ../lib/lvgl, v8.3) with our lv_conf.h. Standalone Pico SDK project:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l5_lvgl C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

# LVGL: point it at our lv_conf.h (LVGL adds that dir to its include path and does
# #include "lv_conf.h"), then build the submodule. It lives one level up (shared with
# the course), so give add_subdirectory an explicit binary dir.
set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/lvgl ${CMAKE_BINARY_DIR}/lvgl)

# The L4 display + touch drivers, reused from the neighbouring lab folder.
set(L4 ${CMAKE_CURRENT_LIST_DIR}/../l4-display-dma)

# Local helper: an LVGL app = the demo source + the L4 drivers, linked against LVGL.
function(add_l5_app name)
    add_executable(${name} ${ARGN} ${L4}/st7796.c ${L4}/gt911.c)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${L4})
    target_link_libraries(${name} pico_stdlib lvgl hardware_spi hardware_dma hardware_i2c)
    pico_enable_stdio_usb(${name} 1)
    pico_enable_stdio_uart(${name} 1)
    pico_add_extra_outputs(${name})
endfunction()

add_l5_app(l5_lvgl main.c)                 # title + live sine chart + slider + button

add_l5_app(l5_scope scope.c)               # 4-channel scope (sine + 2 ADC + noise) + autoscale
target_link_libraries(l5_scope hardware_adc)
