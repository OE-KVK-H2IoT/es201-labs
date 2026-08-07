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
# L0 — Blink: your first Pico SDK program (the getting-started tutorial's `l0-blink/`).
# One folder, one program — a standalone Pico project. Build:
#     mkdir build && cd build && cmake .. && make
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(l0_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(l0_blink blink.c)
target_link_libraries(l0_blink pico_stdlib)
# -DNOFLASH=ON links the image to run entirely from SRAM (debugger/RAM style):
# no boot2, no flash/XIP setup — the path a simulator can run without SSI.
if(NOFLASH)
  pico_set_binary_type(l0_blink no_flash)
endif()
pico_enable_stdio_usb(l0_blink 1)
pico_enable_stdio_uart(l0_blink 1)
pico_add_extra_outputs(l0_blink)
