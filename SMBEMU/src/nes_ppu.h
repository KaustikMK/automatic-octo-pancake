#ifndef NES_PPU_H
#define NES_PPU_H

#include <stdint.h>
#include "nes.h"

void nes_ppu_reset(nes_t *nes);
void nes_ppu_render_frame(nes_t *nes);
uint8_t nes_ppu_read_data(nes_t *nes);
void nes_ppu_write_data(nes_t *nes, uint8_t value);

#endif
