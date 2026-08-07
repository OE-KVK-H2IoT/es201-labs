# L5 — FreeRTOS Tasks. Preemptive multitasking on the RP2350's two Cortex-M33 cores.
# Standalone Pico SDK project (same skeleton the tutorial teaches).
#
# The FreeRTOS kernel is NOT part of the Pico SDK — it is source you build in. Get it:
#     git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git ../lib/FreeRTOS-Kernel
#   ...or point at a checkout you already have:
#     export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
#
# Build:  mkdir build && cd build && cmake .. && make      (or ../run.sh l5_rtos_hello)
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

# --- Locate the FreeRTOS kernel (env override, else the lib/ submodule) -------
if(DEFINED ENV{FREERTOS_KERNEL_PATH})
    set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})
else()
    set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/../lib/FreeRTOS-Kernel)
endif()
# RP2350 ARM (non-TrustZone) SMP port. The import file's location has moved between
# kernel releases, so probe the known layouts instead of hard-coding one.
set(_fr_candidates
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
set(FR_IMPORT "")
foreach(_c IN LISTS _fr_candidates)
    if(EXISTS ${_c})
        set(FR_IMPORT ${_c})
        break()
    endif()
endforeach()

project(l5_rtos C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

# Graceful skip so `run.sh build-all` never hard-fails when the kernel isn't present.
if(NOT FR_IMPORT OR NOT EXISTS ${FR_IMPORT})
    message(WARNING
        "FreeRTOS-Kernel not found at ${FREERTOS_KERNEL_PATH} — L5 RTOS targets skipped.\n"
        "  Get it:  git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git ../lib/FreeRTOS-Kernel\n"
        "  or set:  export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel")
    return()
endif()

include(${FR_IMPORT})

set(L4 ${CMAKE_CURRENT_LIST_DIR}/../l4-display-dma)

# Local helper: an RTOS app = its sources + the shared kernel hooks, with
# FreeRTOSConfig.h on the include path, linked against the kernel + heap_4 in one
# target. Named add_l5_app so ../run.sh discovers the targets (its regex matches
# add_lN_app(<target> …)); stdio over USB, UART off.
function(add_l5_app name)
    add_executable(${name} ${ARGN} rtos_hooks.c)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(${name} pico_stdlib FreeRTOS-Kernel-Heap4)
    pico_enable_stdio_usb(${name} 1)
    pico_enable_stdio_uart(${name} 0)
    pico_add_extra_outputs(${name})
endfunction()

# (1) the fall-back reference: two tasks, two priorities, two kinds of delay
add_l5_app(l5_rtos_hello main_hello.c)

# (2) the mini-scope split: sampling -> queue -> display, ISR trigger.
#     Reuses the L4 ST7796 driver + a thin sweep-scope renderer.
add_l5_app(l5_rtos_scope main_scope.c scope_display.c ${L4}/st7796.c)
target_include_directories(l5_rtos_scope PRIVATE ${L4})
target_link_libraries(l5_rtos_scope hardware_adc hardware_spi hardware_dma)
