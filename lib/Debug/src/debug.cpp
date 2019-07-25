/*
Code by Jacob.Schultz 2019 Teensy forum
https://forum.pjrc.com/threads/54869-Reboot-out-of-control-in-hardfault-handler
 */

#include "debug.h"

// TODO defined in core_cm4.h but CMSIS seems to be broken.
#define SCnSCB_ACTLR (*(volatile uint32_t *)0xE000E008)
#define SCnSCB_ACTLR_DISDEFWBUF 2

extern "C" void HardFault_Handler();

static Stream *dout;

namespace debug
{

void init(Stream &stream)
{
    dout = &stream;
    _VectorsRam[3] = HardFault_Handler;
}

void dumpHex(uint8_t *data, int len)
{
    int i, dpos = 0;
    char buf[17];

    buf[16] = '\0';
    for (i = 0; i < len; i++)
    {
        buf[dpos] = (isprint(*data)) ? *data : '.';
        if (dpos == 0)
            dout->printf("%08x ", data);
        dout->printf("%02x ", *data++);
        if (dpos == 15)
        {
            dout->printf("  %s\n\r", buf);
            dpos = 0;
        }
        else
            dpos++;
    }
    if (dpos)
    {
        for (i = 0; i < (16 - dpos); i++)
            dout->print("   ");
        buf[dpos] = '\0';
        dout->printf("  %s\n\r", buf);
    }
}

void disableWriteBuffer(bool disabled)
{
    if (disabled)
    {
        SCnSCB_ACTLR |= SCnSCB_ACTLR_DISDEFWBUF;
    }
    else
    {
        SCnSCB_ACTLR &= ~SCnSCB_ACTLR_DISDEFWBUF;
    }
}

} // namespace debug

extern "C" void debugHardfault(uint32_t *sp)
{
    
    uint32_t cfsr = SCB_CFSR;
    uint32_t afsr = SCB_AFAR; // TODO Name error should be AFSR
    uint32_t dfsr = SCB_DFSR;
    uint32_t hfsr = SCB_HFSR;
    uint32_t mmfar = SCB_MMFAR;
    uint32_t bfar = SCB_BFAR;
    /*
    uint32_t r0 = sp[0];
    uint32_t r1 = sp[1];
    uint32_t r2 = sp[2];
    uint32_t r3 = sp[3];
    uint32_t r12 = sp[4];
    uint32_t lr = sp[5];
    uint32_t pc = sp[6];
    uint32_t psr = sp[7];
    */
    dout->println("\n\rHardFault:");
    dout->printf("SCB->CFSR  0x%08lb\n\r", cfsr);
    dout->printf("SCB->AFSR  0x%08lb\n\r", afsr);
    dout->printf("SCB->DFSR  0x%08lb\n\r", dfsr);
    dout->printf("SCB->HFSR  0x%08lb\n\r", hfsr);
    dout->printf("SCB->MMFAR 0x%08lb\n\r", mmfar);
    dout->printf("SCB->BFAR  0x%08lb\n\r", bfar);
    /*
    dout->printf("SP   0x%08lx\n\r", (uint32_t)sp);
    dout->printf("R0   0x%08lx\n\r", r0);
    dout->printf("R1   0x%08lx\n\r", r1);
    dout->printf("R2   0x%08lx\n\r", r2);
    dout->printf("R3   0x%08lx\n\r", r3);
    dout->printf("R12  0x%08lx\n\r", r12);
    dout->printf("LR   0x%08lx\n\r", lr);
    dout->printf("PC   0x%08lx\n\r", pc);
    dout->printf("PSR  0x%08lx\n\r", psr);
    */
    //dout->println("Stack:");
    //debug::dumpHex(reinterpret_cast<uint8_t *>(&sp[8]), 64);
    for (;;)
    {
    }
}

void HardFault_Handler()
{
    /*
    __asm volatile(
        "tst lr, #4                                 \n"
        "ite eq                                     \n"
        "mrseq r0, msp                              \n"
        "mrsne r0, psp                              \n"
        "b %0                                       \n" ::"i"(debugHardfault)
        : "r0", "memory");

    */
    uint32_t cfsr = SCB_CFSR;
    uint32_t afsr = SCB_AFAR; // TODO Name error should be AFSR
    uint32_t dfsr = SCB_DFSR;
    uint32_t hfsr = SCB_HFSR;
    uint32_t mmfar = SCB_MMFAR;
    uint32_t bfar = SCB_BFAR;
    
    dout->println("\n\rHardFault:");
    dout->printf("SCB->CFSR  0x%08lx\n\r", cfsr);
    dout->printf("SCB->HFSR  0x%08lx\n\r", hfsr);
    dout->printf("SCB->MMFAR 0x%08lx\n\r", mmfar);
    dout->printf("SCB->BFAR  0x%08lx\n\r", bfar);
    dout->printf("SCB->AFSR  0x%08lx\n\r", afsr);
    dout->printf("SCB->DFSR  0x%08lx\n\r", dfsr);


    dout->printf("reboot\n\r");

    //_reboot_Teensyduino_();
    
}