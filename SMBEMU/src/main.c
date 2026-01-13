#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <string.h>
#include "nes.h"
#include "nes_cpu.h"
#include "nes_ppu.h"
#include "nes_mem.h"
#include "rom.h"

static uint8_t read_controller(void) {
    uint8_t state = 0;
    kb_Scan();
    if (kb_Data[1] & kb_2nd) {
        state |= 0x01;
    }
    if (kb_Data[2] & kb_Alpha) {
        state |= 0x02;
    }
    if (kb_Data[1] & kb_Enter) {
        state |= 0x08;
    }
    if (kb_Data[6] & kb_Mode) {
        state |= 0x04;
    }
    if (kb_Data[7] & kb_Left) {
        state |= 0x40;
    }
    if (kb_Data[7] & kb_Right) {
        state |= 0x80;
    }
    if (kb_Data[7] & kb_Up) {
        state |= 0x10;
    }
    if (kb_Data[7] & kb_Down) {
        state |= 0x20;
    }
    return state;
}

static void show_error(const char *msg) {
    gfx_Begin();
    gfx_FillScreen(0);
    gfx_SetTextFGColor(255);
    gfx_SetTextXY(10, 10);
    gfx_PrintString("SMBEMU Error:");
    gfx_SetTextXY(10, 30);
    gfx_PrintString(msg);
    gfx_SetTextXY(10, 50);
    gfx_PrintString("Press CLEAR");
    gfx_SwapDraw();
    while (!os_GetCSC()) {
    }
    gfx_End();
}

int main(void) {
    nes_t nes;
    memset(&nes, 0, sizeof(nes));

    const char *error = NULL;
    if (!rom_load_smb(&nes, &error)) {
        show_error(error ? error : "ROM load failed");
        return 0;
    }

    gfx_Begin();
    gfx_SetDrawBuffer();
    nes_ppu_reset(&nes);
    nes_cpu_reset(&nes);

    bool running = true;
    while (running) {
        uint8_t controller = read_controller();
        nes_set_controller(&nes, controller);
        if (kb_Data[6] & kb_Clear) {
            running = false;
        }
        int cycles = 0;
        while (cycles < 29780) {
            cycles += nes_cpu_step(&nes);
        }
        nes_ppu_render_frame(&nes);
        gfx_SwapDraw();
    }

    gfx_End();
    return 0;
}
