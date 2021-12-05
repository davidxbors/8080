#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "../disassembler/disassembler.h"

/*
 * Flags of the machine
 * it isvery important for the flags to be in the exact
 * right bits
 */
typedef struct FLAGS {
	uint8_t cy:1;
	uint8_t pad1:1;
	uint8_t p:1;
	uint8_t pad2:1;
	uint8_t ac:1;
	uint8_t pad3:1;
	uint8_t z:1;
	uint8_t s:1;
} FLAGS;

uint8_t a;
uint8_t b;
uint8_t c;
uint8_t d;
uint8_t e;
uint8_t h;
uint8_t l;
uint16_t sp;
uint16_t pc;
uint8_t *memory;
FLAGS flags;
uint8_t int_enable;

/*
 * function for handling unknown instructions
 */
void unknown_instruction ()
{
	fprintf(stderr, "Unknown instruction!\n");
	pc--;
	disassembler8080(memory, pc);
	printf("\n");
	exit(1);
}

/*
 * Writes to memory, at a given address
 */
void write_mem (uint16_t addr, uint8_t val)
{
	if (addr < 0x2000) {
		/* I always wanted to be the one that gives a segfault
		 * and not the one who receives it.
		 */
		fprintf(stderr, "SEGFAULT: Writing to ROM not allowed!\n");
		return;
	}
	else if (addr >= 0x4000) {
		fprintf(stderr, "SEGFAULT: Writing out of Space Invaders RAM not allowed");
		return;
	}

	memory[addr] = val;
}

/*
 * The n.o. 1 bits is counted and if the total is odd the bit is set to 0
 * otherwise, it is set to 1.
 */
int parity (int x, int size)
{
	int p = 0;
	x = (x & ((1<<size) - 1));
	for (int i = 0; i < size; i++) {
		if (x & 0x1) p++;
		x = x >> 1;
	}
	return (0 == (p & 0x1));
}

/*
 * set flags after an operation
 */
void flags_zsp (uint8_t val)
{
	flags.z = (val == 0);
	flags.s = (0x80 == (val & 0x80));
	flags.p = parity(val, 8);
}

/*
 * reads from HL addres in memory
 */ 
uint8_t read_from_hl ()
{
	uint16_t mem_addr = (h << 8) | l;
	return memory[mem_addr];
}

/*
 * writes to HL addres in memory
 */ 
void write_to_hl (uint8_t val)
{
	uint16_t mem_addr = (h << 8) | l;
	write_mem(mem_addr, val);
}

/*
 * set flags after an arithmetic operation
 */
void arith_flags (uint16_t val)
{
	flags.cy = (val > 0xff);
	flags.z  = ((val & 0xff) == 0);
	flags.s  = (0x80 == (val & 0x80));
	flags.p  = parity(val & 0xff, 8);
}

/*
 * set flags after a logic operation
 */
void logic_flags ()
{
	flags.cy = flags.ac = 0;
	flags.z  = (a == 0);
	flags.s  = (0x80 == (a & 0x80));
	flags.p  = parity(a, 8);
}

/*
 * pop pair of registers from the stack
 * (basically from the memory address pointed
 * by the stack pointer)
 */
void pop (uint8_t *high, uint8_t *low)
{
	*low  = memory[sp];
	*high = memory[sp+1];
	sp += 2;
}

/*
 * push pair of registers in the stack
 */
void push (uint8_t high, uint8_t low)
{
	write_mem(sp-1, high);
	write_mem(sp-2, low);
	sp -= 2;
}

unsigned char cycles8080[] = {
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x00..0x0f
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x10..0x1f
	4, 10, 16, 5, 5, 5, 7, 4, 4, 10, 16, 5, 5, 5, 7, 4, //etc
	4, 10, 13, 5, 10, 10, 10, 4, 4, 10, 13, 5, 5, 5, 7, 4,
	
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, //0x40..0x4f
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5,
	
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, //0x80..8x4f
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	
	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, //0xc0..0xcf
	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, 
	11, 10, 10, 18, 17, 11, 7, 11, 11, 5, 10, 5, 17, 17, 7, 11, 
	11, 10, 10, 4, 17, 11, 7, 11, 11, 5, 10, 4, 17, 17, 7, 11, 
};

int emulate8080 ()
{
	unsigned char *opcode = (memory + pc);
	pc ++;
	
	switch (*opcode) {
		/* nop */
	case 0x00:
		break;
		/* LXI B, D16 */
	case 0x01:
		c = opcode[1];
		b = opcode[2];
		pc += 2;
		break;
		/* STAX B */
	case 0x02:
	{
		uint16_t mem_addr = (b << 8) | c;
		write_mem(mem_addr, a);
		break;
	}
		/* INX B */
	case 0x03:
		c++;
		if (c == 0)
			b++;
		break;
		/* INR B */
	case 0x04:
		b++;
		flags_zsp(b);
		break;
		/* DCR B */
	case 0x05:
		b--;
		flags_zsp(b);
		/* MVI B, byte */
	case 0x06:
		b = opcode[1];
		pc++;
		break;
		/* RLC */
	case 0x07:
	{
		uint8_t aux = a;
		a = ((aux & 0x80) >> 7) | (aux << 1);
		flags.cy = (0x80 == (aux & 0x80));
		break;
	}
	case 0x08:
		unknown_instruction();
		break;
		/* DAD B */
	case 0x09:
	{
		uint32_t hl = (h << 8) | l;
		uint32_t bc = (b << 8) | c;
		uint32_t result = hl + bc;
		h = (result & 0xff00) >> 8;
		l = result & 0xff;
		flags.cy = ((result & 0xffff0000) != 0);
		break;
	}
		/* LDAX B */
	case 0x0a:
	{
		uint16_t mem_addr = (b << 8) | c;
		a = memory[mem_addr];
		break;
	}
		/* DCX B */
	case 0x0b:
		c--;
		if (c == 0xff)
			b--;
		break;
		/* INR C */
	case 0x0c:
		c++;
		flags_zsp(c);
		break;
		/* DCR C */
	case 0x0d:
		c--;
		flags_zsp(c);
		break;
		/* MVI C, byte */
	case 0x0e:
		c = opcode[1];
		pc++;
		break;
		/* RRC */
	case 0x0f:
	{
		uint8_t aux = a;
		a = ((aux & 1) << 7) | (aux >> 1);
		flags.cy = (1 == (aux & 1));
		break;
	}
	case 0x10:
		unknown_instruction();
		break;
		/* LXI D, word */
	case 0x11:
		e = opcode[1];
		d = opcode[2];
		pc += 2;
		break;
		/* STAX D */
	case 0x12:
	{
		uint16_t mem_addr = (d << 8) | e;
		write_mem(mem_addr, a);
		break;
	}
		/* INX D */
	case 0x13:
		e++;
		if (e == 0)
			d++;
		break;
		/* INR D */
	case 0x14:
		d++;
		flags_zsp(d);
		break;
		/* DCR D */
	case 0x15:
		d--;
		flags_zsp(d);
		break;
		/* MVI D, byte */
	case 0x16:
		d = opcode[1];
		pc++;
		break;
		/* RAL */
	case 0x17:
	{
		uint8_t aux = a;
		a = flags.cy | (aux << 1);
		flags.cy = (0x80 == (aux & 0x80));
	}
	case 0x18:
		unknown_instruction();
		break;
		/* DAD D */
	case 0x19:
	{
		uint32_t hl = (h << 8) | l;
		uint32_t de = (d << 8) | e;
		uint32_t result = hl + de;
		h = (result & 0xff00) >> 8;
		l = result & 0xff;
		flags.cy = ((result & 0xffff0000) != 0);
		break;
	}
	/* LDAX D */
	case 0x1a:
	{
		uint16_t mem_addr = (d << 8) | e;
		a = memory[mem_addr];
		break;
	}
	/* DCX D */
	case 0x1b:
		e--;
		if (e == 0xff)
			d--;
		break;
		/* INR E */
	case 0x1c:
		e++;
		flags_zsp(e);
		break;
		/* DCR E */
	case 0x1d:
		e--;
		flags_zsp(e);
		break;
		/* MVI E, byte */
	case 0x1e:
		e = opcode[1];
		pc++;
		break;
		/* RAR */
	case 0x1f:
	{
		uint8_t aux = a;
		a = (flags.cy << 7) | (aux >> 1);
		flags.cy = (1 == (aux & 1));
		break;
	}
	case 0x20:
		unknown_instruction();
		break;
		/* LXI H, word */
	case 0x21:
		l = opcode[1];
		h = opcode[2];
		pc += 2;
		break;
		/* SHLD */
	case 0x22:
	{
		uint16_t mem_addr = opcode[1] | (opcode[2] << 8);
		write_mem(mem_addr, l);
		write_mem(mem_addr+1, l);
		pc += 2;
		break;
	}
	/* INX H */
	case 0x23:
		l++;
		if (l == 0)
			h++;
		break;
		/* INR H */
	case 0x24:
		h += 1;
		flags_zsp(h);
		break;
		/* MVI H, byte */
	case 0x26:
		h = opcode[1];
		pc++;
		break;
		/* DAA */
	case 0x27:
		if ((a & 0xf) > 9)
			a += 6;
		if ((a & 0xf0) > 0x90) {
			uint16_t result = (uint16_t) a + 0x60;
			a = result & 0xff;
			arith_flags(result);
		}
		break;
	case 0x28:
		unknown_instruction();
		break;
		/* DAD H */
	case 0x29:
	{
		uint32_t hl = (h << 8) | l;
		uint32_t result = 2 * hl;
		h = (result & 0xff00) >> 8;
		l = (result & 0xff);
		flags.cy = ((result & 0xffff0000) != 0);
		break;
	}
	/* LHLD addr */
	case 0x2a:
	{
		uint16_t mem_addr = opcode[1] | (opcode[2] << 8);
		l = memory[mem_addr];
		h = memory[mem_addr+1]; 
		pc += 2;
		break;
	}
	/* DCX H */
	case 0x2b:
		l--;
		if (l == 0xff)
			h--;
		break;
		/* INR L */
	case 0x2c:
		l++;
		flags_zsp(l);
		break;
		/* DCR L */
	case 0x2d:
		l--;
		flags_zsp(l);
		break;
		/* MVI L, byte */
	case 0x2e:
		l = opcode[1];
		pc++;
		break;
		/* CMA */
	case 0x2f:
		a = ~a;
		break;
	case 0x30:
		unknown_instruction();
		break;
		/* LXI SP, word */
	case 0x31:
		sp = (opcode[2] << 8) | opcode[1];
		pc += 2;
		break;
		/* STA (word) */
	case 0x32:
	{
		uint16_t mem_addr = (opcode[2] << 8) | opcode[1];
		write_mem(mem_addr, a);
		pc += 2;
		break;
	}	
	/* INX SP */
	case 0x33:
		sp++;
		break;
		/* INR M */
	case 0x34:
	{
		uint8_t result = read_from_hl() + 1;
		flags_zsp(result);
		write_to_hl(result);
		break;
	}
	/* DCR M */
	case 0x35:
	{
		uint8_t result = read_from_hl() - 1;
		flags_zsp(result);
		write_to_hl(result);
		break;
	}
	/* MVI M, byte */
	case 0x36:
	{
		write_to_hl(opcode[1]);
		pc++;
		break;
	}
	/* STC */
	case 0x37:
		flags.cy = 1;
		break;
	case 0x38:
		unknown_instruction();
		break;
		/* DAD SP */
	case 0x39:
	{
		uint32_t hl = (h << 8) | l;
		uint32_t result = hl + sp;
		h = (result & 0xff00) >> 8;
		l = result & 0xff00;
		flags.cy = ((result & 0xffff0000) > 0);
	}
	case 0x3a:
	{
		uint16_t mem_addr = (opcode[2] << 8) | opcode[1];
		a = memory[mem_addr];
		pc += 2;
	}
	/* DCX SP */
	case 0x3b:
		sp--;
		break;
		/* INR A */
	case 0x3c:
		a++;
		flags_zsp(a);
		break;
		/* DCR A */
	case 0x3d:
		a--;
		flags_zsp(a);
		break;
		/* MVI A, byte */
	case 0x3e:
		a = opcode[1];
		pc++;
		break;
		/* MOV B, ? */
	case 0x40: b = b; break;
	case 0x41: b = c; break;
	case 0x42: b = d; break;
	case 0x43: b = e; break;
	case 0x44: b = h; break;
	case 0x45: b = l; break;
	case 0x46: b = read_from_hl(); break;
	case 0x47: b = a; break;
		/* MOV C, ? */
	case 0x48: c = b; break;
	case 0x49: c = c; break;
	case 0x4a: c = d; break;
	case 0x4b: c = e; break;
	case 0x4c: c = h; break;
	case 0x4d: c = l; break;
	case 0x4e: c = read_from_hl(); break;
	case 0x4f: c = a; break;
		/* MOV D, ? */
	case 0x50: d = b; break;
	case 0x51: d = c; break;
	case 0x52: d = d; break;
	case 0x53: d = e; break;
	case 0x54: d = h; break;
	case 0x55: d = l; break;
	case 0x56: d = read_from_hl(); break;
	case 0x57: d = a; break;
		/* MOV E, ? */
	case 0x58: e = b; break;
	case 0x59: e = c; break;
	case 0x5a: e = d; break;
	case 0x5b: e = e; break;
	case 0x5c: e = h; break;
	case 0x5d: e = l; break;
	case 0x5e: e = read_from_hl(); break;
	case 0x5f: e = a; break;
		/* MOV H, ? */
	case 0x60: h = b; break;
	case 0x61: h = c; break;
	case 0x62: h = d; break;
	case 0x63: h = e; break;
	case 0x64: h = h; break;
	case 0x65: h = l; break;
	case 0x66: h = read_from_hl(); break;
	case 0x67: h = a; break;
		/* MOV L, ? */
	case 0x68: l = b; break;
	case 0x69: l = c; break;
	case 0x6a: l = d; break;
	case 0x6b: l = e; break;
	case 0x6c: l = h; break;
	case 0x6d: l = l; break;
	case 0x6e: l = read_from_hl(); break;
	case 0x6f: l = a; break;
		/* MOV M, ? */
	case 0x70: write_to_hl(b); break;
	case 0x71: write_to_hl(c); break;
	case 0x72: write_to_hl(d); break;
	case 0x73: write_to_hl(e); break;
	case 0x74: write_to_hl(h); break;
	case 0x75: write_to_hl(l); break;
	case 0x76: break;
	case 0x77: write_to_hl(a); break;
		/* MOV A, ? */
	case 0x78: a = b; break;
	case 0x79: a = c; break;
	case 0x7a: a = d; break;
	case 0x7b: a = e; break;
	case 0x7c: a = h; break;
	case 0x7d: a = l; break;
	case 0x7e: a = read_from_hl(); break;
	case 0x7f: a = a; break;
		/* ADD ? */
	case 0x80: { uint16_t result = (uint16_t) a + (uint16_t) b;
			arith_flags(result); a = (result & 0xff); break; }
	case 0x81: { uint16_t result = (uint16_t) a + (uint16_t) c;
			arith_flags(result); a = (result & 0xff); break; }
	case 0x82: { uint16_t result = (uint16_t) a + (uint16_t) d;
			arith_flags(result); a = (result & 0xff); break; }
	case 0x83: { uint16_t result = (uint16_t) a + (uint16_t) e;
			arith_flags(result); a = (result & 0xff); break; }
	case 0x84: { uint16_t result = (uint16_t) a + (uint16_t) h;
			arith_flags(result); a = (result & 0xff); break; }
	case 0x85: { uint16_t result = (uint16_t) a + (uint16_t) l;
			arith_flags(result); a = (result & 0xff); break; }
	case 0x86: { uint16_t result = (uint16_t) a + (uint16_t) read_from_hl();
			arith_flags(result); a = (result & 0xff); break; }
	case 0x87: { uint16_t result = (uint16_t) a + (uint16_t) a;
			arith_flags(result); a = (result & 0xff); break; }

		/* ADC ? */
	case 0x88:
	{
		uint16_t result = (uint16_t) a + (uint16_t) b +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x89:
	{
		uint16_t result = (uint16_t) a + (uint16_t) c +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x8a:
	{
		uint16_t result = (uint16_t) a + (uint16_t) d +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x8b:
	{
		uint16_t result = (uint16_t) a + (uint16_t) e +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x8c:
	{
		uint16_t result = (uint16_t) a + (uint16_t) h +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x8d:
	{
		uint16_t result = (uint16_t) a + (uint16_t) l +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x8e:
	{
		uint16_t result = (uint16_t) a + (uint16_t) read_from_hl() +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x8f:
	{
		uint16_t result = (uint16_t) a + (uint16_t) a +
			flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	/* SUB ? */
	case 0x90:
	{
		uint16_t result = (uint16_t) a - (uint16_t) b;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x91:
	{
		uint16_t result = (uint16_t) a - (uint16_t) c;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x92:
	{
		uint16_t result = (uint16_t) a - (uint16_t) d;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x93:
	{
		uint16_t result = (uint16_t) a - (uint16_t) e;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x94:
	{
		uint16_t result = (uint16_t) a - (uint16_t) h;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x95:
	{
		uint16_t result = (uint16_t) a - (uint16_t) l;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x96:
	{
		uint16_t result = (uint16_t) a - (uint16_t) read_from_hl();
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x97:
	{
		uint16_t result = (uint16_t) a - (uint16_t) a;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	/* SBB ? */
	case 0x98:
	{
		uint16_t result = (uint16_t) a - (uint16_t) b - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x99:
	{
		uint16_t result = (uint16_t) a - (uint16_t) c - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x9a:
	{
		uint16_t result = (uint16_t) a - (uint16_t) d - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x9b:
	{
		uint16_t result = (uint16_t) a - (uint16_t) e - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x9c:
	{
		uint16_t result = (uint16_t) a - (uint16_t) h - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x9d:
	{
		uint16_t result = (uint16_t) a - (uint16_t) l - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x9e:
	{
		uint16_t result = (uint16_t) a - (uint16_t) read_from_hl() - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	case 0x9f:
	{
		uint16_t result = (uint16_t) a - (uint16_t) a - flags.cy;
		arith_flags(result);
		a = (result & 0xff);
		break;
	}
	/* ANA ? */
	case 0xa0:
		a = a & b;
		logic_flags();
		break;
	case 0xa1:
		a = a & c;
		logic_flags();
		break;
	case 0xa2:
		a = a & d;
		logic_flags();
		break;
	case 0xa3:
		a = a & e;
		logic_flags();
		break;
	case 0xa4:
		a = a & h;
		logic_flags();
		break;
	case 0xa5:
		a = a & l;
		logic_flags();
		break;
	case 0xa6:
		a = a & read_from_hl();
		logic_flags();
		break;
	case 0xa7:
		a = a & a;
		logic_flags();
		break;
		/* XRA ? */
	case 0xa8:
		a ^= b;
		logic_flags();
		break;
	case 0xa9:
		a ^= c;
		logic_flags();
		break;
	case 0xaa:
		a ^= d;
		logic_flags();
		break;
	case 0xab:
		a ^= e;
		logic_flags();
		break;
	case 0xac:
		a ^= h;
		logic_flags();
		break;
	case 0xad:
		a ^= l;
		logic_flags();
		break;
	case 0xae:
		a ^= read_from_hl();
		logic_flags();
		break;
	case 0xaf:
		a ^= a;
		logic_flags();
		break;
		/* ORA ? */
		/* ^ not a Jojo reference */
	case 0xb0:
		a |= b;
		logic_flags();
		break;
	case 0xb1:
		a |= c;
		logic_flags();
		break;
	case 0xb2:
		a |= d;
		logic_flags();
		break;
	case 0xb3:
		a |= e;
		logic_flags();
		break;
	case 0xb4:
		a |= h;
		logic_flags();
		break;
	case 0xb5:
		a |= l;
		logic_flags();
		break;
	case 0xb6:
		a |= read_from_hl();
		logic_flags();
		break;
	case 0xb7:
		a |= a;
		logic_flags();
		break;
		/* CMP ? */
	case 0xb8:
	{
		uint16_t result = (uint16_t) a - (uint16_t) b;
		arith_flags(result);
		break;
	}
	case 0xb9:
	{
		uint16_t result = (uint16_t) a - (uint16_t) c;
		arith_flags(result);
		break;
	}
	case 0xba:
	{
		uint16_t result = (uint16_t) a - (uint16_t) d;
		arith_flags(result);
		break;
	}
	case 0xbb:
	{
		uint16_t result = (uint16_t) a - (uint16_t) e;
		arith_flags(result);
		break;
	}
	case 0xbc:
	{
		uint16_t result = (uint16_t) a - (uint16_t) h;
		arith_flags(result);
		break;
	}
	case 0xbd:
	{
		uint16_t result = (uint16_t) a - (uint16_t) l;
		arith_flags(result);
		break;
	}
	case 0xbe:
	{
		uint16_t result = (uint16_t) a - (uint16_t) read_from_hl();
		arith_flags(result);
		break;
	}
	case 0xbf:
	{
		uint16_t result = (uint16_t) a - (uint16_t) a;
		arith_flags(result);
		break;
	}
	/* RNZ */
	case 0xc0:
		if (!flags.z) {
			pc = memory[sp] | (memory[sp+1] << 8);
			sp += 2;
		}
		break;
		/* POP B */
	case 0xc1:
		pop(&b, &c);
		break;
		/* JNZ addr */
	case 0xc2:
		if (!flags.z)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* JMP addr */
	case 0xc3:
		pc = (opcode[2] << 8) | opcode[1];
		break;
		/* CNZ addr */
	case 0xc4:
		if (!flags.z) {
			/* will return to the next instruction */
			uint16_t return_addr = pc + 2;
			write_mem(sp-1, (return_addr >> 8) & 0xff);
			write_mem(sp-2, (return_addr & 0xff));
			sp = sp - 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
		/* PUSH B */
	case 0xc5:
		push(b, c);
		break;
		/* ADI byte */
	case 0xc6:
	{
		uint16_t result = (uint16_t) a + (uint16_t) opcode[1];
		flags_zsp(result & 0xff);
		flags.cy = (result > 0xff);
		a = result & 0xff;
		pc++;
		break;
	}
	/* RST 0 */
	case 0xc7:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp-1, (return_addr >> 8) & 0xff);
		write_mem(sp-2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x0000;
		break;
	}
	/* RZ */
	case 0xc8:
		if (flags.z) {
			pc = memory[sp] | (memory[sp+1] << 8);
			sp += 2;
		}
		break;
		/* RET */
	case 0xc9:
		pc = memory[sp] | (memory[sp + 1] << 8);
		sp += 2;
		break;
		/* JZ addr */
	case 0xca:
		if (flags.z) {
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
	case 0xcb:
		unknown_instruction();
		break;
		/* CZ addr */
	case 0xcc:
		if (flags.z == 1) {
			uint16_t return_addr = pc + 2;
			write_mem(sp-1, (return_addr >> 8) & 0xff);
			write_mem(sp-2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
		/* CALL addr */
	case 0xcd:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp-1, (return_addr >> 8) & 0xff);
		write_mem(sp-2, (return_addr & 0xff));
		sp -= 2;
		pc = (opcode[2] << 8) | opcode[1];
		break;
	}
	/* ACI byte */
	case 0xce:
	{
		uint16_t result = a + opcode[1] + flags.cy;
		flags_zsp(result & 0xff);
		flags.cy = (result > 0xff);
		a = result & 0xff;
		pc++;
	}
	/* RST 1 */
	case 0xcf:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp-1, (return_addr >> 8) & 0xff);
		write_mem(sp-2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x0008;
		break;
	}
	/* RNC */
	case 0xd0:
		if (!flags.cy) {
			pc = memory[sp] | (memory[sp+1] << 8);
			sp += 2;
		}
		break;
		/* POP D */
	case 0xd1:
		pop(&d, &e);
		break;
		/* JNC */
	case 0xd2:
		if (!flags.cy)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* OUT d8 */
	case 0xd3:
		pc++;
		break;
		/* CNC addr */
	case 0xd4:
		if (!flags.cy) {
			uint16_t return_addr = pc + 2;
			write_mem(sp-1, (return_addr >> 8) & 0xff);
			write_mem(sp-2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
		/* PUSH D */
	case 0xd5:
		push(d, e);
		break;
		/* SUI byte */
	case 0xd6:
	{
		uint8_t result = a - opcode[1];
		flags_zsp(result & 0xff);
		flags.cy = (a < opcode[1]);
		a = result;
		pc++;
		break;
	}
	/* RST 2 */
	case 0xd7:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp-1, (return_addr >> 8) & 0xff);
		write_mem(sp-2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x10;
		break;
	}
	/* RN */
	case 0xd8:
		if (flags.cy) {
			pc = memory[sp] | (memory[sp+1] << 8);
			sp += 2;
		}
		break;
	case 0xd9:
		unknown_instruction();
		/* JC */
	case 0xda:
		if (flags.cy)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* IN d8 */
	case 0xdb:
		pc++;
		break;
		/* CC addr */
	case 0xdc:
		if (flags.cy) {
			uint16_t return_addr = pc + 2;
			write_mem(sp-1, (return_addr >> 8) & 0xff);
			write_mem(sp-2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
	case 0xdd:
		unknown_instruction();
		break;
		/* SBI byte */
	case 0xde:
	{
		uint16_t result = a - opcode[1] - flags.cy;
		flags_zsp(result & 0xff);
		flags.cy = (result > 0xff);
		a = result & 0xff;
		pc++;
		break;
	}
		/* RST 3 */
	case 0xdf:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp-1, (return_addr >> 8) & 0xff);
		write_mem(sp-2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x18;
		break;
	}
	/* RPO */
	case 0xe0:
		if (flags.p == 0) {
			pc = memory[sp] | (memory[sp + 1] << 8);
			sp += 2;
		}
		break;
		/* POP H */
	case 0xe1:
		pop(&h, &l);
		break;
		/* LPO */
	case 0xe2:
		if (flags.p == 0)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* XTHL */
	case 0xe3:
	{
		uint8_t h = h;
		uint8_t l = l;
		l = memory[sp];
		h = memory[sp+1];
		write_mem(sp, l);
		write_mem(sp + 1, h);
		break;
	}
	/* CPO addr */
	case 0xe4:
		if (flags.p == 0) {
			uint16_t return_addr = pc + 2;
			write_mem(sp - 1, (return_addr >> 8) & 0xff);
			write_mem(sp - 2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
		/* PUSH H */
	case 0xe5:
		push(h, l);
		break;
		/* ANI byte */
	case 0xe6:
	{
		a &= opcode[1];
		logic_flags();
		pc++;
		break;
	}
	/* RST 4 */
	case 0xe7:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp - 1, (return_addr >> 8) & 0xff);
		write_mem(sp - 2, (return_addr & 0xff));
		pc = 0x20;
		break;
	}
	/* RPE */
	case 0xe8:
		if (flags.p) {
			pc = memory[sp] | (memory[sp + 1] << 8);
			sp += 2;
		}
		break;
		/* PCHL */
	case 0xe9:
		pc = (h << 8) | l;
		break;
		/* JPE addr */
	case 0xea:
		if (flags.p)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* XCHG */
	case 0xeb:
	{
		uint8_t aux = d;
		d = h;
		h = aux;
		aux = e;
		e = l;
		l = aux;
		break;
	}
	/* CPE addr */
	case 0xec:
		if (flags.p) {
			uint16_t return_addr = pc + 2;
			write_mem(sp - 1, (return_addr >> 8) & 0xff);
			write_mem(sp - 2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
	case 0xed:
		unknown_instruction();
		/* XRI data */
	case 0xee:
	{
		uint8_t x = a ^ opcode[1];
		flags_zsp(x);
		flags.cy = 0;
		a = x;
		pc++;
		break;
	}
	/* RST 5 */
	case 0xef:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp - 1, (return_addr >> 8) & 0xff);
		write_mem(sp - 2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x28;
	}
	/* RP */
	case 0xf0:
		if (!flags.s) {
			pc = memory[sp] | (memory[sp + 1] << 8);
			sp += 2;
		}
		break;
		/* POP PSW */
	case 0xf1:
		pop(&a, (unsigned char *) &flags);
		break;
		/* JP addr */
	case 0xf2:
		if (!flags.s)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* DI */
	case 0xf3:
		int_enable = 0;
		break;
		/* CP addr */
	case 0xf4:
		if (!flags.s) {
			uint16_t return_addr = pc + 2;
			write_mem(sp - 1, (return_addr >> 8) & 0xff);
			write_mem(sp - 2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
		/* PUSH PSW */
	case 0xf5:
		push(a, *(unsigned char *) &flags);
		break;
		/* ORI byte */
	case 0xf6:
	{
		uint8_t x = a | opcode[1];
		flags_zsp(x);
		flags.cy = 0;
		a = x;
		pc++;
		break;
	}
	/* RST 6 */
	case 0xf7:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp - 1, (return_addr >> 8) & 0xff);
		write_mem(sp - 2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x30;
		break;
	}
	/* RM */
	case 0xf8:
		if (flags.s) {
			pc = memory[sp] | (memory[sp+1] << 8);
			sp += 2;
		}
		break;
		/* SPHL */
	case 0xf9:
		sp = l | (h << 8);
		break;
		/* JM addr */
	case 0xfa:
		if (flags.s)
			pc = (opcode[2] << 8) | opcode[1];
		else
			pc += 2;
		break;
		/* EI */
	case 0xfb:
		int_enable = 1;
		break;
		/* CM addr */
	case 0xfc:
		if (flags.s) {
			uint16_t return_addr = pc + 2;
			write_mem(sp - 1, (return_addr >> 8) & 0xff);
			write_mem(sp - 2, (return_addr & 0xff));
			sp -= 2;
			pc = (opcode[2] << 8) | opcode[1];
		} else
			pc += 2;
		break;
	case 0xfd:
		unknown_instruction();
		break;
		/* CPI byte */
	case 0xfe:
	{
		uint8_t x = a - opcode[1];
		flags_zsp(x);
		flags.cy = (a < opcode[1]);
		pc++;
		break;
	}
		/* RST 7 */
	case 0xff:
	{
		uint16_t return_addr = pc + 2;
		write_mem(sp - 1, (return_addr >> 8) & 0xff);
		write_mem(sp - 2, (return_addr & 0xff));
		sp -= 2;
		pc = 0x38;
		break;
	}
	}
#if PRINTOPS
	printf("\t");
	printf("%c", state->cc.z ? 'z' : '.');
	printf("%c", state->cc.s ? 's' : '.');
	printf("%c", state->cc.p ? 'p' : '.');
	printf("%c", state->cc.cy ? 'c' : '.');
	printf("%c  ", state->cc.ac ? 'a' : '.');
	printf("A $%02x B $%02x C $%02x D $%02x E $%02x H $%02x L $%02x SP %04x\n", state->a, state->b, state->c,
           state->d, state->e, state->h, state->l, state->sp);
#endif
	return cycles8080[*opcode];	
}



int main (void)
{
	printf("MMN 8080 Emulator\n");
	return 0;
}
