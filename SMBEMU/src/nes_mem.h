#ifndef NES_MEM_H
#define NES_MEM_H

#include <stdint.h>
#include <stdbool.h>
#include "nes.h"

uint8_t nes_cpu_read(nes_t *nes, uint16_t addr);
void nes_cpu_write(nes_t *nes, uint16_t addr, uint8_t value);

void nes_set_controller(nes_t *nes, uint8_t state);

#endif
