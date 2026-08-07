/* Lab 0 — startup: the two things the Cortex-M33 needs at reset, plus the one
 * thing the RP2350 boot ROM needs to accept our image. No crt0, no SDK.
 *
 * Adapted for the EP-0172 carrier (LED on GP16) from the from-scratch example at
 * https://github.com/OE-KVK-H2IoT/pico2 (0_bare_blinking_LED).
 */

/* =========================================================================
 * Vector table — must sit at the start of flash (the linker script puts it
 * there). On reset the core reads word 0 into MSP and jumps to word 1.
 * ========================================================================= */
.section .vector_table, "a"

    .word   0x20082000      /* [0] initial stack pointer: top of 520 KB SRAM
                                   (0x20000000 + 0x82000). The core loads this
                                   into MSP before any instruction runs. */
    .word   main            /* [1] reset vector: the address of our C entry.
                                   The linker sets bit 0 (Thumb) automatically,
                                   because `main` is a Thumb function. No crt0
                                   runs first — main IS the reset handler. */

/* =========================================================================
 * RP2350 boot metadata block. Unlike the RP2040, the RP2350 boot ROM will NOT
 * run an image that lacks a valid IMAGE_DEF block — a bare vector table alone
 * boots on an RP2040 but is REJECTED here. These magic words declare "this is
 * an executable ARM image for RP2350." Reference: RP2350 datasheet §5.9.
 * (This is the RP2350-specific gotcha the generic Cortex-M tutorials miss.)
 * ========================================================================= */
.section .boot_block, "a"

    .word   0xFFFFDED3      /* PICOBIN_BLOCK_MARKER_START */
    .word   0x10210142      /* item: IMAGE_TYPE = executable, secure, ARM, RP2350 */
    .word   0x000001FF      /* item: LAST (block terminator) */
    .word   0x00000000      /* next-block offset: 0 -> this is a single-block loop */
    .word   0xAB123579      /* PICOBIN_BLOCK_MARKER_END */
