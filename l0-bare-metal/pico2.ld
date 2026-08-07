/* Lab 0 — linker script: the map that tells the linker WHERE every byte goes.
 * The SDK ships a ~200-line version of this; ours is the irreducible minimum. */

MEMORY
{
    /* The only region we use: execute-in-place flash on the RP2350/Pico 2.
     * rx = readable + executable. 4 MB is the Pico 2's flash size. */
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 4M
}

SECTIONS
{
    /* One output section, placed at flash base (0x10000000). ORDER MATTERS: */
    .text : {
        KEEP(*(.vector_table*))   /* [0x10000000] vector table FIRST — the core
                                          reads MSP + reset vector from flash base */
        KEEP(*(.boot_block*))     /* then the RP2350 boot metadata block */
        *(.text*)                 /* then all our code */
        *(.rodata*)               /* and read-only constants */
    } > FLASH

    /* KEEP() stops the linker from garbage-collecting sections nothing "calls" —
     * the vector table and boot block are read by hardware, not by our code, so
     * without KEEP they could be dropped and the image would never boot. */
}
