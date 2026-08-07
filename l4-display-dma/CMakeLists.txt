# L4 — Display & DMA. Several demos that share the ST7796 SPI driver (st7796.c) and,
# for the touch ones, the GT911 driver (gt911.c). Standalone Pico SDK project:
#     mkdir build && cd build && cmake .. && make          # build all demos
#     cmake --build . --target l4_display                  # or just one
#     cmake -B build -DLCD_MHZ=75 -DSTART_MODE=1            # sweep a compile-time knob
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l4_display_dma C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

# Local helper (self-contained): a USB+UART serial app with .uf2 output. There are
# seven demos here, so this saves repeating the four stdio/output lines each time.
# Add the per-demo hardware libraries with target_link_libraries() after the call.
function(add_l4_app name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} pico_stdlib)
    pico_enable_stdio_usb(${name} 1)
    pico_enable_stdio_uart(${name} 1)
    pico_add_extra_outputs(${name})
endfunction()

add_l4_app(l4_display main.c st7796.c)                    # ST7796 panel self-test
target_link_libraries(l4_display hardware_spi hardware_dma)

add_l4_app(l4_touch touch_test.c st7796.c gt911.c)        # GT911 touch test (draws dots)
target_link_libraries(l4_touch hardware_spi hardware_dma hardware_i2c)

add_l4_app(l4_optimize optimize.c st7796.c)               # full vs dirty vs double-buffer demo
target_link_libraries(l4_optimize hardware_spi hardware_dma)
# Forward -D cache vars to compile defs, so these can be overridden from the build cmd.
foreach(opt START_MODE ENABLE_DBUF LOG TARGET_FPS LCD_MHZ SYS_MHZ)
    if(DEFINED ${opt})
        target_compile_definitions(l4_optimize PRIVATE ${opt}=${${opt}})
    endif()
endforeach()

add_l4_app(l4_gfx gfx_primitives.c st7796.c)              # "cost of a pixel" + roll-your-own primitives
target_link_libraries(l4_gfx hardware_spi hardware_dma)

add_l4_app(l4_fb gfx_buffer.c st7796.c)                   # render in a RAM framebuffer, blit once (sine wave)
target_link_libraries(l4_fb hardware_spi hardware_dma)
foreach(opt BAND_H)
    if(DEFINED ${opt})
        target_compile_definitions(l4_fb PRIVATE ${opt}=${${opt}})
    endif()
endforeach()

add_l4_app(l4_joybox joybox.c st7796.c)                   # L2+L4: joystick drives a box
target_link_libraries(l4_joybox hardware_spi hardware_dma hardware_adc)
foreach(opt DEADZONE FULL_DEFLECTION CAL_SAMPLES)         # sweepable from the build cmd
    if(DEFINED ${opt})
        target_compile_definitions(l4_joybox PRIVATE ${opt}=${${opt}})
    endif()
endforeach()

add_l4_app(l4_touchbox touchbox.c st7796.c gt911.c)       # touch-drag box + buttons recolour
target_link_libraries(l4_touchbox hardware_spi hardware_dma hardware_i2c)
