#include "nes_cpu.h"
#include "nes_mem.h"

static uint8_t cpu_read(nes_t *nes, uint16_t addr) {
    return nes_cpu_read(nes, addr);
}

static void cpu_write(nes_t *nes, uint16_t addr, uint8_t value) {
    nes_cpu_write(nes, addr, value);
}

static void set_zn(nes_cpu_t *cpu, uint8_t value) {
    if (value == 0) {
        cpu->p |= FLAG_Z;
    } else {
        cpu->p &= ~FLAG_Z;
    }
    if (value & 0x80) {
        cpu->p |= FLAG_N;
    } else {
        cpu->p &= ~FLAG_N;
    }
}

static void push(nes_t *nes, uint8_t value) {
    cpu_write(nes, 0x0100 | nes->cpu.sp, value);
    nes->cpu.sp--;
}

static uint8_t pop(nes_t *nes) {
    nes->cpu.sp++;
    return cpu_read(nes, 0x0100 | nes->cpu.sp);
}

static uint16_t read16(nes_t *nes, uint16_t addr) {
    uint8_t lo = cpu_read(nes, addr);
    uint8_t hi = cpu_read(nes, addr + 1);
    return (uint16_t)hi << 8 | lo;
}

static uint16_t read16_wrap(nes_t *nes, uint16_t addr) {
    uint8_t lo = cpu_read(nes, addr);
    uint8_t hi = cpu_read(nes, (addr & 0xFF00) | ((addr + 1) & 0x00FF));
    return (uint16_t)hi << 8 | lo;
}

void nes_cpu_reset(nes_t *nes) {
    nes_cpu_t *cpu = &nes->cpu;
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->sp = 0xFD;
    cpu->p = FLAG_I | FLAG_U;
    cpu->pc = read16(nes, 0xFFFC);
    cpu->nmi_pending = false;
}

void nes_cpu_nmi(nes_t *nes) {
    nes_cpu_t *cpu = &nes->cpu;
    push(nes, (cpu->pc >> 8) & 0xFF);
    push(nes, cpu->pc & 0xFF);
    push(nes, cpu->p & ~FLAG_B);
    cpu->p |= FLAG_I;
    cpu->pc = read16(nes, 0xFFFA);
}

static void adc(nes_t *nes, uint8_t value) {
    nes_cpu_t *cpu = &nes->cpu;
    uint16_t sum = cpu->a + value + ((cpu->p & FLAG_C) ? 1 : 0);
    if (sum > 0xFF) {
        cpu->p |= FLAG_C;
    } else {
        cpu->p &= ~FLAG_C;
    }
    uint8_t result = (uint8_t)sum;
    if (((cpu->a ^ result) & (value ^ result) & 0x80) != 0) {
        cpu->p |= FLAG_V;
    } else {
        cpu->p &= ~FLAG_V;
    }
    cpu->a = result;
    set_zn(cpu, cpu->a);
}

static void sbc(nes_t *nes, uint8_t value) {
    adc(nes, (uint8_t)(~value));
}

static uint16_t addr_imm(nes_t *nes) {
    return nes->cpu.pc++;
}

static uint16_t addr_zp(nes_t *nes) {
    return cpu_read(nes, nes->cpu.pc++);
}

static uint16_t addr_zp_x(nes_t *nes) {
    return (uint8_t)(cpu_read(nes, nes->cpu.pc++) + nes->cpu.x);
}

static uint16_t addr_zp_y(nes_t *nes) {
    return (uint8_t)(cpu_read(nes, nes->cpu.pc++) + nes->cpu.y);
}

static uint16_t addr_abs(nes_t *nes) {
    uint16_t addr = read16(nes, nes->cpu.pc);
    nes->cpu.pc += 2;
    return addr;
}

static uint16_t addr_abs_x(nes_t *nes) {
    uint16_t base = addr_abs(nes);
    return base + nes->cpu.x;
}

static uint16_t addr_abs_y(nes_t *nes) {
    uint16_t base = addr_abs(nes);
    return base + nes->cpu.y;
}

static uint16_t addr_ind(nes_t *nes) {
    uint16_t ptr = addr_abs(nes);
    return read16_wrap(nes, ptr);
}

static uint16_t addr_ind_x(nes_t *nes) {
    uint8_t zp = (uint8_t)(cpu_read(nes, nes->cpu.pc++) + nes->cpu.x);
    uint8_t lo = cpu_read(nes, zp);
    uint8_t hi = cpu_read(nes, (uint8_t)(zp + 1));
    return (uint16_t)hi << 8 | lo;
}

static uint16_t addr_ind_y(nes_t *nes) {
    uint8_t zp = cpu_read(nes, nes->cpu.pc++);
    uint8_t lo = cpu_read(nes, zp);
    uint8_t hi = cpu_read(nes, (uint8_t)(zp + 1));
    return ((uint16_t)hi << 8 | lo) + nes->cpu.y;
}

static void branch(nes_t *nes, bool condition) {
    int8_t offset = (int8_t)cpu_read(nes, nes->cpu.pc++);
    if (condition) {
        nes->cpu.pc = (uint16_t)(nes->cpu.pc + offset);
    }
}

int nes_cpu_step(nes_t *nes) {
    nes_cpu_t *cpu = &nes->cpu;
    if (cpu->nmi_pending) {
        cpu->nmi_pending = false;
        nes_cpu_nmi(nes);
        return 7;
    }
    uint8_t opcode = cpu_read(nes, cpu->pc++);
    switch (opcode) {
    case 0x00: {
        cpu->pc++;
        push(nes, (cpu->pc >> 8) & 0xFF);
        push(nes, cpu->pc & 0xFF);
        push(nes, cpu->p | FLAG_B);
        cpu->p |= FLAG_I;
        cpu->pc = read16(nes, 0xFFFE);
        return 7;
    }
    case 0x01: {
        uint16_t addr = addr_ind_x(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 6;
    }
    case 0x05: {
        uint16_t addr = addr_zp(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 3;
    }
    case 0x06: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val <<= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 5;
    }
    case 0x08:
        push(nes, cpu->p | FLAG_B | FLAG_U);
        return 3;
    case 0x09: {
        uint8_t val = cpu_read(nes, addr_imm(nes));
        cpu->a |= val;
        set_zn(cpu, cpu->a);
        return 2;
    }
    case 0x0A:
        cpu->p = (cpu->p & ~FLAG_C) | ((cpu->a >> 7) & 1);
        cpu->a <<= 1;
        set_zn(cpu, cpu->a);
        return 2;
    case 0x0D: {
        uint16_t addr = addr_abs(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x0E: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val <<= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x10:
        branch(nes, (cpu->p & FLAG_N) == 0);
        return 2;
    case 0x11: {
        uint16_t addr = addr_ind_y(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 5;
    }
    case 0x15: {
        uint16_t addr = addr_zp_x(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x16: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val <<= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x18:
        cpu->p &= ~FLAG_C;
        return 2;
    case 0x19: {
        uint16_t addr = addr_abs_y(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x1D: {
        uint16_t addr = addr_abs_x(nes);
        cpu->a |= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x1E: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val <<= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 7;
    }
    case 0x20: {
        uint16_t addr = addr_abs(nes);
        uint16_t return_addr = cpu->pc - 1;
        push(nes, (return_addr >> 8) & 0xFF);
        push(nes, return_addr & 0xFF);
        cpu->pc = addr;
        return 6;
    }
    case 0x21: {
        uint16_t addr = addr_ind_x(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 6;
    }
    case 0x24: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~(FLAG_Z | FLAG_N | FLAG_V)) |
                 ((val & 0x80) ? FLAG_N : 0) |
                 ((val & 0x40) ? FLAG_V : 0) |
                 ((cpu->a & val) ? 0 : FLAG_Z);
        return 3;
    }
    case 0x25: {
        uint16_t addr = addr_zp(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 3;
    }
    case 0x26: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val = (uint8_t)((val << 1) | carry);
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 5;
    }
    case 0x28:
        cpu->p = pop(nes);
        cpu->p |= FLAG_U;
        return 4;
    case 0x29: {
        uint8_t val = cpu_read(nes, addr_imm(nes));
        cpu->a &= val;
        set_zn(cpu, cpu->a);
        return 2;
    }
    case 0x2A: {
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | ((cpu->a >> 7) & 1);
        cpu->a = (uint8_t)((cpu->a << 1) | carry);
        set_zn(cpu, cpu->a);
        return 2;
    }
    case 0x2C: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~(FLAG_Z | FLAG_N | FLAG_V)) |
                 ((val & 0x80) ? FLAG_N : 0) |
                 ((val & 0x40) ? FLAG_V : 0) |
                 ((cpu->a & val) ? 0 : FLAG_Z);
        return 4;
    }
    case 0x2D: {
        uint16_t addr = addr_abs(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x2E: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val = (uint8_t)((val << 1) | carry);
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x30:
        branch(nes, (cpu->p & FLAG_N) != 0);
        return 2;
    case 0x31: {
        uint16_t addr = addr_ind_y(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 5;
    }
    case 0x35: {
        uint16_t addr = addr_zp_x(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x36: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val = (uint8_t)((val << 1) | carry);
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x38:
        cpu->p |= FLAG_C;
        return 2;
    case 0x39: {
        uint16_t addr = addr_abs_y(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x3D: {
        uint16_t addr = addr_abs_x(nes);
        cpu->a &= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x3E: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | ((val >> 7) & 1);
        val = (uint8_t)((val << 1) | carry);
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 7;
    }
    case 0x40:
        cpu->p = pop(nes);
        cpu->p |= FLAG_U;
        cpu->pc = (uint16_t)pop(nes);
        cpu->pc |= (uint16_t)pop(nes) << 8;
        return 6;
    case 0x41: {
        uint16_t addr = addr_ind_x(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 6;
    }
    case 0x45: {
        uint16_t addr = addr_zp(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 3;
    }
    case 0x46: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val >>= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 5;
    }
    case 0x48:
        push(nes, cpu->a);
        return 3;
    case 0x49: {
        uint8_t val = cpu_read(nes, addr_imm(nes));
        cpu->a ^= val;
        set_zn(cpu, cpu->a);
        return 2;
    }
    case 0x4A:
        cpu->p = (cpu->p & ~FLAG_C) | (cpu->a & 1);
        cpu->a >>= 1;
        set_zn(cpu, cpu->a);
        return 2;
    case 0x4C:
        cpu->pc = addr_abs(nes);
        return 3;
    case 0x4D: {
        uint16_t addr = addr_abs(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x4E: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val >>= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x50:
        branch(nes, (cpu->p & FLAG_V) == 0);
        return 2;
    case 0x51: {
        uint16_t addr = addr_ind_y(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 5;
    }
    case 0x55: {
        uint16_t addr = addr_zp_x(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x56: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val >>= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x58:
        cpu->p &= ~FLAG_I;
        return 2;
    case 0x59: {
        uint16_t addr = addr_abs_y(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x5D: {
        uint16_t addr = addr_abs_x(nes);
        cpu->a ^= cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0x5E: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr);
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val >>= 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 7;
    }
    case 0x60:
        cpu->pc = (uint16_t)pop(nes);
        cpu->pc |= (uint16_t)pop(nes) << 8;
        cpu->pc++;
        return 6;
    case 0x61: {
        uint16_t addr = addr_ind_x(nes);
        adc(nes, cpu_read(nes, addr));
        return 6;
    }
    case 0x65: {
        uint16_t addr = addr_zp(nes);
        adc(nes, cpu_read(nes, addr));
        return 3;
    }
    case 0x66: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val = (uint8_t)((val >> 1) | (carry << 7));
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 5;
    }
    case 0x68:
        cpu->a = pop(nes);
        set_zn(cpu, cpu->a);
        return 4;
    case 0x69:
        adc(nes, cpu_read(nes, addr_imm(nes)));
        return 2;
    case 0x6A: {
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | (cpu->a & 1);
        cpu->a = (uint8_t)((cpu->a >> 1) | (carry << 7));
        set_zn(cpu, cpu->a);
        return 2;
    }
    case 0x6C:
        cpu->pc = addr_ind(nes);
        return 5;
    case 0x6D: {
        uint16_t addr = addr_abs(nes);
        adc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0x6E: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val = (uint8_t)((val >> 1) | (carry << 7));
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x70:
        branch(nes, (cpu->p & FLAG_V) != 0);
        return 2;
    case 0x71: {
        uint16_t addr = addr_ind_y(nes);
        adc(nes, cpu_read(nes, addr));
        return 5;
    }
    case 0x75: {
        uint16_t addr = addr_zp_x(nes);
        adc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0x76: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val = (uint8_t)((val >> 1) | (carry << 7));
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0x78:
        cpu->p |= FLAG_I;
        return 2;
    case 0x79: {
        uint16_t addr = addr_abs_y(nes);
        adc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0x7D: {
        uint16_t addr = addr_abs_x(nes);
        adc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0x7E: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t carry = (cpu->p & FLAG_C) ? 1 : 0;
        cpu->p = (cpu->p & ~FLAG_C) | (val & 1);
        val = (uint8_t)((val >> 1) | (carry << 7));
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 7;
    }
    case 0x81: {
        uint16_t addr = addr_ind_x(nes);
        cpu_write(nes, addr, cpu->a);
        return 6;
    }
    case 0x84: {
        uint16_t addr = addr_zp(nes);
        cpu_write(nes, addr, cpu->y);
        return 3;
    }
    case 0x85: {
        uint16_t addr = addr_zp(nes);
        cpu_write(nes, addr, cpu->a);
        return 3;
    }
    case 0x86: {
        uint16_t addr = addr_zp(nes);
        cpu_write(nes, addr, cpu->x);
        return 3;
    }
    case 0x88:
        cpu->y--;
        set_zn(cpu, cpu->y);
        return 2;
    case 0x8A:
        cpu->a = cpu->x;
        set_zn(cpu, cpu->a);
        return 2;
    case 0x8C: {
        uint16_t addr = addr_abs(nes);
        cpu_write(nes, addr, cpu->y);
        return 4;
    }
    case 0x8D: {
        uint16_t addr = addr_abs(nes);
        cpu_write(nes, addr, cpu->a);
        return 4;
    }
    case 0x8E: {
        uint16_t addr = addr_abs(nes);
        cpu_write(nes, addr, cpu->x);
        return 4;
    }
    case 0x90:
        branch(nes, (cpu->p & FLAG_C) == 0);
        return 2;
    case 0x91: {
        uint16_t addr = addr_ind_y(nes);
        cpu_write(nes, addr, cpu->a);
        return 6;
    }
    case 0x94: {
        uint16_t addr = addr_zp_x(nes);
        cpu_write(nes, addr, cpu->y);
        return 4;
    }
    case 0x95: {
        uint16_t addr = addr_zp_x(nes);
        cpu_write(nes, addr, cpu->a);
        return 4;
    }
    case 0x96: {
        uint16_t addr = addr_zp_y(nes);
        cpu_write(nes, addr, cpu->x);
        return 4;
    }
    case 0x98:
        cpu->a = cpu->y;
        set_zn(cpu, cpu->a);
        return 2;
    case 0x99: {
        uint16_t addr = addr_abs_y(nes);
        cpu_write(nes, addr, cpu->a);
        return 5;
    }
    case 0x9A:
        cpu->sp = cpu->x;
        return 2;
    case 0x9D: {
        uint16_t addr = addr_abs_x(nes);
        cpu_write(nes, addr, cpu->a);
        return 5;
    }
    case 0xA0:
        cpu->y = cpu_read(nes, addr_imm(nes));
        set_zn(cpu, cpu->y);
        return 2;
    case 0xA1: {
        uint16_t addr = addr_ind_x(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 6;
    }
    case 0xA2:
        cpu->x = cpu_read(nes, addr_imm(nes));
        set_zn(cpu, cpu->x);
        return 2;
    case 0xA4: {
        uint16_t addr = addr_zp(nes);
        cpu->y = cpu_read(nes, addr);
        set_zn(cpu, cpu->y);
        return 3;
    }
    case 0xA5: {
        uint16_t addr = addr_zp(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 3;
    }
    case 0xA6: {
        uint16_t addr = addr_zp(nes);
        cpu->x = cpu_read(nes, addr);
        set_zn(cpu, cpu->x);
        return 3;
    }
    case 0xA8:
        cpu->y = cpu->a;
        set_zn(cpu, cpu->y);
        return 2;
    case 0xA9:
        cpu->a = cpu_read(nes, addr_imm(nes));
        set_zn(cpu, cpu->a);
        return 2;
    case 0xAA:
        cpu->x = cpu->a;
        set_zn(cpu, cpu->x);
        return 2;
    case 0xAC: {
        uint16_t addr = addr_abs(nes);
        cpu->y = cpu_read(nes, addr);
        set_zn(cpu, cpu->y);
        return 4;
    }
    case 0xAD: {
        uint16_t addr = addr_abs(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0xAE: {
        uint16_t addr = addr_abs(nes);
        cpu->x = cpu_read(nes, addr);
        set_zn(cpu, cpu->x);
        return 4;
    }
    case 0xB0:
        branch(nes, (cpu->p & FLAG_C) != 0);
        return 2;
    case 0xB1: {
        uint16_t addr = addr_ind_y(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 5;
    }
    case 0xB4: {
        uint16_t addr = addr_zp_x(nes);
        cpu->y = cpu_read(nes, addr);
        set_zn(cpu, cpu->y);
        return 4;
    }
    case 0xB5: {
        uint16_t addr = addr_zp_x(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0xB6: {
        uint16_t addr = addr_zp_y(nes);
        cpu->x = cpu_read(nes, addr);
        set_zn(cpu, cpu->x);
        return 4;
    }
    case 0xB8:
        cpu->p &= ~FLAG_V;
        return 2;
    case 0xB9: {
        uint16_t addr = addr_abs_y(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0xBA:
        cpu->x = cpu->sp;
        set_zn(cpu, cpu->x);
        return 2;
    case 0xBC: {
        uint16_t addr = addr_abs_x(nes);
        cpu->y = cpu_read(nes, addr);
        set_zn(cpu, cpu->y);
        return 4;
    }
    case 0xBD: {
        uint16_t addr = addr_abs_x(nes);
        cpu->a = cpu_read(nes, addr);
        set_zn(cpu, cpu->a);
        return 4;
    }
    case 0xBE: {
        uint16_t addr = addr_abs_y(nes);
        cpu->x = cpu_read(nes, addr);
        set_zn(cpu, cpu->x);
        return 4;
    }
    case 0xC0:
        {
            uint8_t val = cpu_read(nes, addr_imm(nes));
            uint8_t res = cpu->y - val;
            cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                     ((cpu->y >= val) ? FLAG_C : 0) |
                     ((res == 0) ? FLAG_Z : 0) |
                     ((res & 0x80) ? FLAG_N : 0);
            return 2;
        }
    case 0xC1: {
        uint16_t addr = addr_ind_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 6;
    }
    case 0xC4: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->y - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->y >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 3;
    }
    case 0xC5: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 3;
    }
    case 0xC6: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr) - 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 5;
    }
    case 0xC8:
        cpu->y++;
        set_zn(cpu, cpu->y);
        return 2;
    case 0xC9: {
        uint8_t val = cpu_read(nes, addr_imm(nes));
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 2;
    }
    case 0xCA:
        cpu->x--;
        set_zn(cpu, cpu->x);
        return 2;
    case 0xCC: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->y - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->y >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 4;
    }
    case 0xCD: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 4;
    }
    case 0xCE: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr) - 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0xD0:
        branch(nes, (cpu->p & FLAG_Z) == 0);
        return 2;
    case 0xD1: {
        uint16_t addr = addr_ind_y(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 5;
    }
    case 0xD5: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 4;
    }
    case 0xD6: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr) - 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0xD8:
        cpu->p &= ~FLAG_D;
        return 2;
    case 0xD9: {
        uint16_t addr = addr_abs_y(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 4;
    }
    case 0xDD: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->a - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->a >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 4;
    }
    case 0xDE: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr) - 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 7;
    }
    case 0xE0: {
        uint8_t val = cpu_read(nes, addr_imm(nes));
        uint8_t res = cpu->x - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->x >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 2;
    }
    case 0xE1: {
        uint16_t addr = addr_ind_x(nes);
        sbc(nes, cpu_read(nes, addr));
        return 6;
    }
    case 0xE4: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->x - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->x >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 3;
    }
    case 0xE5: {
        uint16_t addr = addr_zp(nes);
        sbc(nes, cpu_read(nes, addr));
        return 3;
    }
    case 0xE6: {
        uint16_t addr = addr_zp(nes);
        uint8_t val = cpu_read(nes, addr) + 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 5;
    }
    case 0xE8:
        cpu->x++;
        set_zn(cpu, cpu->x);
        return 2;
    case 0xE9:
        sbc(nes, cpu_read(nes, addr_imm(nes)));
        return 2;
    case 0xEA:
        return 2;
    case 0xEC: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr);
        uint8_t res = cpu->x - val;
        cpu->p = (cpu->p & ~(FLAG_C | FLAG_Z | FLAG_N)) |
                 ((cpu->x >= val) ? FLAG_C : 0) |
                 ((res == 0) ? FLAG_Z : 0) |
                 ((res & 0x80) ? FLAG_N : 0);
        return 4;
    }
    case 0xED: {
        uint16_t addr = addr_abs(nes);
        sbc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0xEE: {
        uint16_t addr = addr_abs(nes);
        uint8_t val = cpu_read(nes, addr) + 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0xF0:
        branch(nes, (cpu->p & FLAG_Z) != 0);
        return 2;
    case 0xF1: {
        uint16_t addr = addr_ind_y(nes);
        sbc(nes, cpu_read(nes, addr));
        return 5;
    }
    case 0xF5: {
        uint16_t addr = addr_zp_x(nes);
        sbc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0xF6: {
        uint16_t addr = addr_zp_x(nes);
        uint8_t val = cpu_read(nes, addr) + 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 6;
    }
    case 0xF8:
        cpu->p |= FLAG_D;
        return 2;
    case 0xF9: {
        uint16_t addr = addr_abs_y(nes);
        sbc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0xFD: {
        uint16_t addr = addr_abs_x(nes);
        sbc(nes, cpu_read(nes, addr));
        return 4;
    }
    case 0xFE: {
        uint16_t addr = addr_abs_x(nes);
        uint8_t val = cpu_read(nes, addr) + 1;
        cpu_write(nes, addr, val);
        set_zn(cpu, val);
        return 7;
    }
    default:
        return 2;
    }
}
