#ifndef ROM_H
#define ROM_H

#include <stdbool.h>
#include "nes.h"

bool rom_load_smb(nes_t *nes, const char **error);

#endif
