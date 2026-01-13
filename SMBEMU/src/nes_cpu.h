#ifndef NES_CPU_H
#define NES_CPU_H

#include "nes.h"
#include <stdint.h>

void nes_cpu_reset(nes_t *nes);
int nes_cpu_step(nes_t *nes);
void nes_cpu_nmi(nes_t *nes);

#endif
