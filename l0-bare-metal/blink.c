// Lab 0 — bare-metal blink, NO SDK, NO libraries: just writes to memory-mapped
// registers, straight from the RP2350 datasheet. This whole file is the program.
//
// The carrier's user LED 1 is on GP16 (the Pico 2 W has no plain GP25 LED — that
// pin drives nothing visible on this board). Every address below is a fact you
// verify in the datasheet, not a number to trust: see the register map in
//   RP2350 datasheet §2.2 (address map), §9.11 (GPIO), §3.1.11 (SIO).
#define SIO_BASE        0xD0000000u   // Single-cycle IO — core-local GPIO (AHB)
#define IO_BANK0_BASE   0x40028000u   // GPIO function select (APB)
#define PADS_BANK0_BASE 0x40038000u   // Pad control: isolation, pulls, drive (APB)

#define LED_PIN         16            // carrier user LED 1

// A memory-mapped register is just an address you read/write. `volatile` tells the
// compiler the value can change outside the program (it's hardware), so it must
// really do every access and never optimise one away.
#define REG(addr)       (*(volatile unsigned int *)(addr))

int main(void) {
    // 1) Pad: clear GPIO16's pad control. On RP2350 pads power up ISOLATED
    //    (bit 8 = ISO = 1); writing 0 clears isolation and leaves a plain output.
    //    Offset = 0x04 + 4*pin = 0x04 + 4*16 = 0x44.
    REG(PADS_BANK0_BASE + 0x44) = 0u;

    // 2) Function select: route GPIO16 to the SIO function (value 5).
    //    Offset = 0x04 + 8*pin = 0x04 + 8*16 = 0x84 (GPIO16_CTRL).
    REG(IO_BANK0_BASE + 0x84) = 5u;

    // 3) Direction: enable GPIO16 as an output via SIO GPIO_OE_SET (offset 0x38).
    //    Atomic "set" register: writing a 1-bit sets that bit, others untouched.
    REG(SIO_BASE + 0x38) = 1u << LED_PIN;

    // 4) Blink forever: GPIO_OUT_XOR (offset 0x28) toggles the pin each write.
    while (1) {
        REG(SIO_BASE + 0x28) = 1u << LED_PIN;

        // Crude busy-wait — no timer, no SDK sleep_ms, just burn cycles so the
        // toggle is slow enough to see. `volatile` keeps the loop from being
        // optimised to nothing. (You will measure and fix this in the tasks.)
        for (volatile unsigned int i = 0; i < 800000u; i++) { /* spin */ }
    }
}
