#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "disassembler.h"

int main (int argc, char *argv[])
{
	FILE *rom = fopen(argv[1], "rb");
	if (NULL == rom) {
		fprintf(stderr, "Couldn't open file: %s\n", argv[1]);
		return -1;
	}

	/* get file size */
	fseek(rom, 0, SEEK_END);
	int fsize = ftell(rom);
	rewind(rom);

	/* read file into memory */
	unsigned char *buffer = malloc(fsize);
	if (NULL == buffer) {
		fprintf(stderr, "Failed to alloc mem for ROM\n");
		goto error1;
	}

	size_t result = fread(buffer, sizeof(char), fsize, rom);
	if (result != fsize) {
		fprintf(stderr, "Failed to read ROM!\n");
		goto error1;
	}

	/* clean up */
	fclose(rom);
	printf("Loaded ROM into buffer.\n");

	/* diassemble file */
	int pc = 0;
	while (pc < fsize)
		pc += disassembler8080(buffer, pc);
	return 0;

error1:
	fclose(rom);
	return -1;
}
