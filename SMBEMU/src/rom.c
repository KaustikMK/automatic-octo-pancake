#include "rom.h"
#include <fileioc.h>
#include <string.h>

typedef struct {
    uint8_t magic[4];
    uint8_t prg_banks;
    uint8_t chr_banks;
    uint8_t flags6;
    uint8_t flags7;
    uint8_t flags8;
    uint8_t flags9;
    uint8_t flags10;
    uint8_t padding[5];
} ines_header_t;

static bool validate_header(const ines_header_t *header, const char **error) {
    if (memcmp(header->magic, "NES\x1A", 4) != 0) {
        *error = "Bad iNES magic";
        return false;
    }
    if (header->prg_banks != 2 || header->chr_banks != 1) {
        *error = "Unsupported ROM size";
        return false;
    }
    uint8_t mapper = (header->flags7 & 0xF0) | (header->flags6 >> 4);
    if (mapper != 0) {
        *error = "Unsupported mapper";
        return false;
    }
    return true;
}

bool rom_load_smb(nes_t *nes, const char **error) {
    *error = NULL;
    ti_var_t handle = ti_Open("SMBROM", "r");
    if (!handle) {
        *error = "SMBROM AppVar not found";
        return false;
    }
    ines_header_t header;
    if (ti_Read(&header, sizeof(header), 1, handle) != 1) {
        ti_Close(handle);
        *error = "Failed to read header";
        return false;
    }
    if (!validate_header(&header, error)) {
        ti_Close(handle);
        return false;
    }
    uint16_t skip = (header.flags6 & 0x04) ? 512 : 0;
    if (skip) {
        ti_Seek(skip, SEEK_CUR, handle);
    }
    if (ti_Read(nes->prg, NES_PRG_SIZE, 1, handle) != 1) {
        ti_Close(handle);
        *error = "Failed to read PRG";
        return false;
    }
    if (ti_Read(nes->ppu.chr, NES_CHR_SIZE, 1, handle) != 1) {
        ti_Close(handle);
        *error = "Failed to read CHR";
        return false;
    }
    ti_Close(handle);
    return true;
}
