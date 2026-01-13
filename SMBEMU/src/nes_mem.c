#include "nes_mem.h"
#include "nes_ppu.h"

static uint8_t ppu_read_register(nes_t *nes, uint16_t addr) {
    nes_ppu_t *ppu = &nes->ppu;
    switch (addr & 0x7) {
    case 2: {
        uint8_t value = ppu->status;
        ppu->status &= ~0x80;
        ppu->addr_latch = false;
        return value;
    }
    case 4:
        return ppu->oam[ppu->oam_addr];
    case 7:
        return nes_ppu_read_data(nes);
    default:
        return 0;
    }
}

static void ppu_write_register(nes_t *nes, uint16_t addr, uint8_t value) {
    nes_ppu_t *ppu = &nes->ppu;
    switch (addr & 0x7) {
    case 0:
        ppu->ctrl = value;
        ppu->temp_addr = (ppu->temp_addr & 0xF3FF) | ((value & 0x03) << 10);
        break;
    case 1:
        ppu->mask = value;
        break;
    case 3:
        ppu->oam_addr = value;
        break;
    case 4:
        ppu->oam[ppu->oam_addr++] = value;
        break;
    case 5:
        if (!ppu->addr_latch) {
            ppu->scroll_x = value;
            ppu->fine_x = value & 0x7;
            ppu->temp_addr = (ppu->temp_addr & 0xFFE0) | (value >> 3);
            ppu->addr_latch = true;
        } else {
            ppu->scroll_y = value;
            ppu->temp_addr = (ppu->temp_addr & 0x8FFF) | ((value & 0x07) << 12);
            ppu->temp_addr = (ppu->temp_addr & 0xFC1F) | ((value & 0xF8) << 2);
            ppu->addr_latch = false;
        }
        break;
    case 6:
        if (!ppu->addr_latch) {
            ppu->temp_addr = (ppu->temp_addr & 0x00FF) | ((value & 0x3F) << 8);
            ppu->addr_latch = true;
        } else {
            ppu->temp_addr = (ppu->temp_addr & 0xFF00) | value;
            ppu->vram_addr = ppu->temp_addr;
            ppu->addr_latch = false;
        }
        break;
    case 7:
        nes_ppu_write_data(nes, value);
        break;
    default:
        break;
    }
}

uint8_t nes_cpu_read(nes_t *nes, uint16_t addr) {
    if (addr < 0x2000) {
        return nes->ram[addr & 0x7FF];
    }
    if (addr < 0x4000) {
        return ppu_read_register(nes, addr);
    }
    if (addr == 0x4016) {
        uint8_t value = (nes->controller_shift & 1) | 0x40;
        if (!nes->controller_strobe) {
            nes->controller_shift >>= 1;
        }
        return value;
    }
    if (addr >= 0x8000) {
        return nes->prg[addr - 0x8000];
    }
    return 0;
}

void nes_cpu_write(nes_t *nes, uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        nes->ram[addr & 0x7FF] = value;
        return;
    }
    if (addr < 0x4000) {
        ppu_write_register(nes, addr, value);
        return;
    }
    if (addr == 0x4014) {
        uint16_t base = (uint16_t)value << 8;
        for (uint16_t i = 0; i < 256; i++) {
            nes->ppu.oam[i] = nes_cpu_read(nes, base + i);
        }
        return;
    }
    if (addr == 0x4016) {
        nes->controller_strobe = (value & 1) != 0;
        if (nes->controller_strobe) {
            nes->controller_shift = nes->controller_state;
        }
        return;
    }
    (void)value;
}

void nes_set_controller(nes_t *nes, uint8_t state) {
    nes->controller_state = state;
    if (nes->controller_strobe) {
        nes->controller_shift = state;
    }
}
