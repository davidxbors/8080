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
	
void arith_flags (uint16_t val)
{
	flags.cy = (val > 0xff);
	flags.z  = ((val & 0xff) == 0);
	flags.s  = (0x80 == (val & 0x80));
	flags.p  = parity(val & 0xff, 8);
}

void emulate8080 ()
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
	}
}
