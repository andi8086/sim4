// http://e4004.szyc.org/iset.html

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define READREG (opa & 1) ? *(reg(opa)) & 0x0F : (*(reg(opa)) & 0xF0) >> 4
#define WRITEREG (opa & 1) ? *(reg(opa)) & 0xF0 | tmp : *(reg(opa)) & 0x0F | (tmp << 4)

#define READREGP *(reg(opa))
#define WRITEREGP tmp

uint8_t memory[4096];

#pragma pack(1)
typedef struct {
	union { struct { uint8_t R1: 4; uint8_t R0: 4; }; uint8_t P0; };
	union { struct { uint8_t R3: 4; uint8_t R2: 4; }; uint8_t P1; };
	union { struct { uint8_t R5: 4; uint8_t R4: 4; }; uint8_t P2; };
	union { struct { uint8_t R7: 4; uint8_t R6: 4; }; uint8_t P3; };
	union { struct { uint8_t R9: 4; uint8_t R8: 4; }; uint8_t P4; };
	union { struct { uint8_t R11: 4; uint8_t R10: 4; }; uint8_t P5; };
	union { struct { uint8_t R13: 4; uint8_t R12: 4; }; uint8_t P6; };
	union { struct { uint8_t R15: 4; uint8_t R14: 4; }; uint8_t P7; };
	uint8_t romaddr;
	uint8_t ramaddr;
	uint8_t rom:4;
	uint8_t ram:4;
	uint8_t stack[4];
	uint8_t stack2[4];
	uint8_t A:4;
	uint8_t sp:2;
	uint8_t CY:1;
	uint8_t T:1;
} CPU;

CPU cpu;

uint8_t *reg(uint8_t opa)
{
	return &cpu.P0 + (opa >> 1);
}

void simulate()
{
	uint16_t rom_addr = cpu.rom*256 + cpu.romaddr;
	uint8_t byte = memory[rom_addr];
	uint8_t opr = (byte & 0xF0) >> 4;
	uint8_t opa = (byte & 0x0F);
	bool condition;
	uint8_t tmp, tmp2, page;
	switch(opr) {
		case 0: break;		// NOP
		case 0xD:		// LDM
			cpu.A = opa;
			cpu.romaddr++;
			break;
		case 0xA:		// LD
			cpu.A = READREG;
			cpu.romaddr++;
			break;
		case 0xB:		// XCH
			tmp = cpu.A;
			cpu.A = READREG;
			*(reg(opa)) = WRITEREG;
			cpu.romaddr++;
			break;
		case 0x8:		// ADD
			tmp = READREG;
			tmp += cpu.A + cpu.CY;
			cpu.CY = tmp & 0x10;
			cpu.A = tmp & 0x0F;
			cpu.romaddr++;
			break;
		case 0x9:		// SUB
			tmp = READREG;
			tmp = ~tmp & 0x0F;
			tmp += cpu.A + cpu.CY;
			cpu.CY = tmp & 0x10;
			cpu.A = tmp & 0x0F;
			cpu.romaddr++;
			break;
		case 0x6:		// INC
			tmp = READREG;
			tmp = (tmp + 1) & 0x0F;
			*(reg(opa)) = WRITEREG;
			cpu.romaddr++;
			break;
		case 0xC:		// BBL
			cpu.A = opa & 0x0F;
			if (cpu.sp == 0) cpu.sp = 3; else cpu.sp--;
			tmp = cpu.stack[cpu.sp];
			cpu.romaddr = tmp;
			tmp = cpu.stack2[cpu.sp];
			cpu.rom = tmp;
			break;
		case 0x03:		// JIN
			if (opa & 1) {
				tmp = READREGP;
				if (cpu.romaddr = 0xFF) cpu.rom++;
				cpu.romaddr = tmp;
			} else {	// FIN
				tmp = *reg(0);
				page = cpu.rom;
				if (cpu.romaddr == 0xFF) {
					page = (page + 1) & 0xF;
				}
				tmp = memory[page*256 + tmp];
				*reg(opa) = tmp;
			}
			cpu.romaddr++;
			break;
		case 0x2:		// SRC
			cpu.romaddr++;
			if (!cpu.romaddr) cpu.rom++;
			if (opa & 1) {
				tmp = READREGP;
				cpu.ram = tmp >> 14;
				cpu.ramaddr = tmp & 0x3F;
			} else {	// FIM
				tmp = memory[cpu.rom * 256 + cpu.romaddr];
				*(reg(opa)) = WRITEREGP;
				cpu.romaddr++;
				if (!cpu.romaddr) cpu.rom++;
			}
			break;
		case 0x4:		// JUN
			page = opa;
			cpu.romaddr++;
			if (cpu.romaddr == 0) cpu.rom += 1;
			cpu.romaddr = memory[cpu.rom*256 + cpu.romaddr];
			cpu.rom = page;
			break;
		case 0x5:		// JMS
			page = opa;
			tmp = cpu.romaddr;
			tmp2 = cpu.rom;
			cpu.romaddr++;
			if (cpu.romaddr == 0) cpu.rom += 1;
			cpu.romaddr = memory[cpu.rom*256 + cpu.romaddr];
			cpu.rom = page;
			cpu.stack[cpu.sp] = cpu.romaddr;
			cpu.stack2[cpu.sp] = cpu.rom; 
			if (cpu.sp == 0) cpu.sp = 3; else cpu.sp--;
			break;
		case 0x01:		// JCN
			cpu.romaddr++;
			condition = false;
			if (opa & 1) if (cpu.T == 0) condition = true;
			if (opa & 2) if (cpu.CY == 1) condition = true;
			if (opa & 4) if (cpu.A == 0) condition = true;
			if (opa & 8) condition = !condition;
			page = cpu.rom;
			if (cpu.romaddr == 255) {
				page++;
			}
			if (condition) {
				cpu.romaddr = memory[page * 256 + cpu.romaddr];
			} else {
				cpu.romaddr++;
			}
			cpu.rom = page;
			break;
		case 0x7:		// ISZ
			cpu.romaddr++;
			page = cpu.rom;
			if (cpu.romaddr == 255) {
				page++;
			}
			tmp = READREG;
			if (tmp == 15) {
				tmp = 0;
				*(reg(opa)) = WRITEREG;
				cpu.romaddr++;
			} else
			{
				tmp++;
				*(reg(opa)) = WRITEREG;
				cpu.romaddr = memory[page * 256 + cpu.romaddr];
			}
			cpu.rom = page;
			break;
	}

}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Missing rom file");
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");

	size_t r = fread(memory, 1, 4096, f);
	if (ferror(f)) {
		fprintf(stderr, "Could not read file.\n");
	}
	fclose(f);
	cpu.A = 7;
	cpu.R3 = 3;
	memory[0] = 0xB3;
	printf(" A = %d, R3 = %d \n", cpu.A, cpu.R3);
	simulate();
	printf(" A = %d, R3 = %d \n", cpu.A, cpu.R3);
}
