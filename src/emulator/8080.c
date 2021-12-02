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
	}
}
