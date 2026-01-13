#ifndef NES_H
#define NES_H

#include <stdint.h>
#include <stdbool.h>

#define NES_RAM_SIZE 0x0800
#define NES_NAMETABLE_SIZE 0x0800
#define NES_PALETTE_SIZE 0x20
#define NES_OAM_SIZE 256
#define NES_CHR_SIZE 0x2000
#define NES_PRG_SIZE 0x8000

#define NES_SCREEN_WIDTH 256
#define NES_SCREEN_HEIGHT 240

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

typedef struct {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    uint8_t p;
    uint16_t pc;
    bool nmi_pending;
} nes_cpu_t;

typedef struct {
    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    uint8_t oam_addr;
    uint8_t oam[NES_OAM_SIZE];
    uint8_t nametable[NES_NAMETABLE_SIZE];
    uint8_t palette[NES_PALETTE_SIZE];
    uint8_t chr[NES_CHR_SIZE];
    uint16_t vram_addr;
    uint16_t temp_addr;
    uint8_t fine_x;
    bool addr_latch;
    uint8_t scroll_x;
    uint8_t scroll_y;
    uint8_t data_buffer;
} nes_ppu_t;

typedef struct {
    nes_cpu_t cpu;
    nes_ppu_t ppu;
    uint8_t ram[NES_RAM_SIZE];
    uint8_t prg[NES_PRG_SIZE];
    uint8_t controller_state;
    uint8_t controller_shift;
    bool controller_strobe;
} nes_t;

#endif
