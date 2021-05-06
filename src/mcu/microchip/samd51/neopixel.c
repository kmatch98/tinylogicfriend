#include <stdint.h>

#include "sam.h"
#include <stdio.h>
#include <string.h>
#include "tlf_board.h"

#ifdef BOARD_NEOPIXEL_PIN

#define PINOP(pin, OP) (PORT->Group[(pin) / 32].OP.reg = (1ul << ((pin) % 32)))

void delay(uint32_t ms) {
// SAMD21 starts up at 1mhz by default.
#ifdef SAMD21
    ms <<= 8;
#endif
// SAMD51 starts up at 48mhz by default.
#ifdef SAMD51
    ms <<= 12;
#endif
    for (uint32_t i = 1; i < ms; ++i) {
        asm("nop");
    }
}



static void neopixel_send_buffer_core(volatile uint32_t *clraddr, uint32_t pinMask,
                                      const uint8_t *ptr, int numBytes) {
    asm volatile("        push    {r4, r5, r6, lr};"
                 "        add     r3, r2, r3;"
                 "loopLoad:"
                 "        ldrb r5, [r2, #0];" // r5 := *ptr
                 "        add  r2, #1;"       // ptr++
                 "        movs    r4, #128;"  // r4-mask, 0x80
                 "loopBit:"
                 "        str r1, [r0, #4];"                    // set
                 #ifdef SAMD21
                 "        movs r6, #3; d2: sub r6, #1; bne d2;" // delay 3
                 #endif
                 #ifdef SAMD51
                 "        movs r6, #3; d2: subs r6, #1; bne d2;" // delay 3
                 #endif
                 "        tst r4, r5;"                          // mask&r5
                 "        bne skipclr;"
                 "        str r1, [r0, #0];" // clr
                 "skipclr:"
                 #ifdef SAMD21
                 "        movs r6, #6; d0: sub r6, #1; bne d0;" // delay 6
                 #endif
                 #ifdef SAMD51
                 "        movs r6, #6; d0: subs r6, #1; bne d0;" // delay 6
                 #endif
                 "        str r1, [r0, #0];"   // clr (possibly again, doesn't matter)
                 #ifdef SAMD21
                 "        asr     r4, r4, #1;" // mask >>= 1
                 #endif
                 #ifdef SAMD51
                 "        asrs     r4, r4, #1;" // mask >>= 1
                 #endif
                 "        beq     nextbyte;"
                 "        uxtb    r4, r4;"
                 #ifdef SAMD21
                 "        movs r6, #2; d1: sub r6, #1; bne d1;" // delay 2
                 #endif
                 #ifdef SAMD51
                 "        movs r6, #2; d1: subs r6, #1; bne d1;" // delay 2
                 #endif
                 "        b       loopBit;"
                 "nextbyte:"
                 "        cmp r2, r3;"
                 "        bcs stop;"
                 "        b loopLoad;"
                 "stop:"
                 "        pop {r4, r5, r6, pc};"
                 "");
}

// this assumes the pin has been configured correctly
static inline void neopixel_send_buffer(const uint8_t *ptr, int numBytes) {
    uint8_t portNum = BOARD_NEOPIXEL_PIN / 32;
    uint32_t pinMask = 1ul << (BOARD_NEOPIXEL_PIN % 32);

    PINOP(BOARD_NEOPIXEL_PIN, DIRSET);

    PINOP(BOARD_NEOPIXEL_PIN, OUTCLR);

    delay(1);

    volatile uint32_t *clraddr = &PORT->Group[portNum].OUTCLR.reg;

    // equivalent to cpu_irq_is_enabled()
    if (__get_PRIMASK() == 0) {
        __disable_irq();
        neopixel_send_buffer_core(clraddr, pinMask, ptr, numBytes);
        __enable_irq();
    } else {
        neopixel_send_buffer_core(clraddr, pinMask, ptr, numBytes);
    }
}

// Neopixel function
void RGBLED_set_color(uint32_t color) {

    uint8_t buf[3];

    // memset(buf, 0, sizeof(buf));
    buf[0] = color >> 8;
    buf[1] = color >> 16;
    buf[2] = color;

    neopixel_send_buffer(buf, 3);
}

#endif // BOARD_NEOPIXEL_PIN