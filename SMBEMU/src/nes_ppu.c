#include "nes_ppu.h"
#include <graphx.h>
#include <string.h>

static const uint32_t nes_palette_rgb[64] = {
    0x545454, 0x001E74, 0x081090, 0x300088, 0x440064, 0x5C0030, 0x540400, 0x3C1800,
    0x202A00, 0x083A00, 0x004000, 0x003C00, 0x00323C, 0x000000, 0x000000, 0x000000,
    0x989698, 0x084CC4, 0x3032EC, 0x5C1EE4, 0x8814B0, 0xA01464, 0x982220, 0x783C00,
    0x545A00, 0x287200, 0x087C00, 0x007628, 0x006678, 0x000000, 0x000000, 0x000000,
    0xECEEEC, 0x4C9AEC, 0x787CEC, 0xB062EC, 0xE454EC, 0xEC58B4, 0xEC6A64, 0xD48820,
    0xA0AA00, 0x74C400, 0x4CD020, 0x38CC6C, 0x38B4CC, 0x3C3C3C, 0x000000, 0x000000,
    0xECEEEC, 0xA8CCEC, 0xBCBCEC, 0xD4B2EC, 0xECAEEC, 0xECAED4, 0xECB4B0, 0xE4C490,
    0xCCD278, 0xB4DE78, 0xA8E290, 0x98E2B4, 0xA0D6E4, 0xA0A2A0, 0x000000, 0x000000
};

static void init_palette(void) {
    static bool initialized = false;
    static uint16_t palette_1555[64];
    if (initialized) {
        return;
    }
    for (int i = 0; i < 64; i++) {
        uint8_t r = (nes_palette_rgb[i] >> 16) & 0xFF;
        uint8_t g = (nes_palette_rgb[i] >> 8) & 0xFF;
        uint8_t b = nes_palette_rgb[i] & 0xFF;
        palette_1555[i] = gfx_RGBTo1555(r, g, b);
    }
    gfx_SetPalette(palette_1555, 64, 0);
    initialized = true;
}

void nes_ppu_reset(nes_t *nes) {
    nes_ppu_t *ppu = &nes->ppu;
    memset(ppu, 0, sizeof(*ppu));
    ppu->status = 0x00;
    ppu->vram_addr = 0;
    ppu->temp_addr = 0;
    ppu->addr_latch = false;
    ppu->data_buffer = 0;
    init_palette();
}

static uint8_t ppu_read_vram(nes_t *nes, uint16_t addr) {
    nes_ppu_t *ppu = &nes->ppu;
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return ppu->chr[addr];
    }
    if (addr < 0x3F00) {
        return ppu->nametable[addr & 0x7FF];
    }
    return ppu->palette[addr & 0x1F];
}

static void ppu_write_vram(nes_t *nes, uint16_t addr, uint8_t value) {
    nes_ppu_t *ppu = &nes->ppu;
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return;
    }
    if (addr < 0x3F00) {
        ppu->nametable[addr & 0x7FF] = value;
        return;
    }
    ppu->palette[addr & 0x1F] = value & 0x3F;
}

uint8_t nes_ppu_read_data(nes_t *nes) {
    nes_ppu_t *ppu = &nes->ppu;
    uint16_t addr = ppu->vram_addr;
    uint8_t value = ppu_read_vram(nes, addr);
    uint8_t result;
    if (addr < 0x3F00) {
        result = ppu->data_buffer;
        ppu->data_buffer = value;
    } else {
        result = value;
        ppu->data_buffer = ppu_read_vram(nes, addr - 0x1000);
    }
    ppu->vram_addr += (ppu->ctrl & 0x04) ? 32 : 1;
    return result;
}

void nes_ppu_write_data(nes_t *nes, uint8_t value) {
    nes_ppu_t *ppu = &nes->ppu;
    ppu_write_vram(nes, ppu->vram_addr, value);
    ppu->vram_addr += (ppu->ctrl & 0x04) ? 32 : 1;
}

static void render_background(nes_t *nes, uint8_t *buffer, int x_offset) {
    nes_ppu_t *ppu = &nes->ppu;
    uint16_t base = (ppu->ctrl & 0x10) ? 0x1000 : 0x0000;
    uint16_t scroll = ppu->scroll_x;

    for (int y = 0; y < NES_SCREEN_HEIGHT; y++) {
        int world_y = y + ppu->scroll_y;
        int tile_y = (world_y / 8) % 30;
        int fine_y = world_y % 8;
        for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
            int world_x = x + scroll;
            int name_x = (world_x / 8) % 64;
            int fine_x = world_x % 8;
            int table = (name_x >= 32) ? 1 : 0;
            int tile_x = name_x % 32;
            uint16_t nt_index = table * 0x400 + tile_y * 32 + tile_x;
            uint8_t tile = ppu->nametable[nt_index & 0x7FF];
            uint16_t pattern = base + tile * 16;
            uint8_t plane0 = ppu->chr[pattern + fine_y];
            uint8_t plane1 = ppu->chr[pattern + fine_y + 8];
            uint8_t bit = 7 - fine_x;
            uint8_t color = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1);
            uint16_t attr_index = table * 0x400 + 0x3C0 + (tile_y / 4) * 8 + (tile_x / 4);
            uint8_t attr = ppu->nametable[attr_index & 0x7FF];
            uint8_t shift = ((tile_y & 2) ? 4 : 0) + ((tile_x & 2) ? 2 : 0);
            uint8_t palette = (attr >> shift) & 3;
            uint8_t final_color = 0;
            if (color) {
                final_color = ppu->palette[palette * 4 + color] & 0x3F;
            } else {
                final_color = ppu->palette[0] & 0x3F;
            }
            buffer[y * 320 + x_offset + x] = final_color;
        }
    }
}

static void render_sprites(nes_t *nes, uint8_t *buffer, int x_offset) {
    nes_ppu_t *ppu = &nes->ppu;
    bool sprite_8x16 = (ppu->ctrl & 0x20) != 0;
    uint16_t base = (ppu->ctrl & 0x08) ? 0x1000 : 0x0000;
    for (int i = 63; i >= 0; i--) {
        int index = i * 4;
        uint8_t y = ppu->oam[index];
        uint8_t tile = ppu->oam[index + 1];
        uint8_t attr = ppu->oam[index + 2];
        uint8_t x = ppu->oam[index + 3];
        int sprite_height = sprite_8x16 ? 16 : 8;
        for (int row = 0; row < sprite_height; row++) {
            int draw_y = y + 1 + row;
            if (draw_y < 0 || draw_y >= NES_SCREEN_HEIGHT) {
                continue;
            }
            int tile_row = (attr & 0x80) ? (sprite_height - 1 - row) : row;
            uint16_t pattern_addr;
            if (sprite_8x16) {
                uint8_t table = tile & 1;
                uint8_t tile_index = tile & 0xFE;
                if (tile_row >= 8) {
                    tile_index++;
                    tile_row -= 8;
                }
                pattern_addr = (uint16_t)table * 0x1000 + tile_index * 16 + tile_row;
            } else {
                pattern_addr = base + tile * 16 + tile_row;
            }
            uint8_t plane0 = ppu->chr[pattern_addr];
            uint8_t plane1 = ppu->chr[pattern_addr + 8];
            for (int col = 0; col < 8; col++) {
                int draw_x = x + col;
                if (draw_x < 0 || draw_x >= NES_SCREEN_WIDTH) {
                    continue;
                }
                int bit = (attr & 0x40) ? col : (7 - col);
                uint8_t color = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1);
                if (color == 0) {
                    continue;
                }
                uint8_t palette = (attr & 0x03) + 4;
                uint8_t final_color = ppu->palette[palette * 4 + color] & 0x3F;
                buffer[draw_y * 320 + x_offset + draw_x] = final_color;
            }
        }
    }
}

void nes_ppu_render_frame(nes_t *nes) {
    nes_ppu_t *ppu = &nes->ppu;
    uint8_t *buffer = gfx_vbuffer;
    int x_offset = (320 - NES_SCREEN_WIDTH) / 2;
    gfx_FillScreen(0);
    render_background(nes, buffer, x_offset);
    render_sprites(nes, buffer, x_offset);
    ppu->status |= 0x80;
    if (ppu->ctrl & 0x80) {
        nes->cpu.nmi_pending = true;
    }
}
