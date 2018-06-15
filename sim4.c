// http://e4004.szyc.org/iset.html

#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <termcap.h>

#define READREG (opa & 1) ? *(reg(opa)) & 0x0F : (*(reg(opa)) & 0xF0) >> 4
#define WRITEREG (opa & 1) ? *(reg(opa)) & 0xF0 | tmp : *(reg(opa)) & 0x0F | (tmp << 4)

#define READREGP *(reg(opa))
#define WRITEREGP tmp

#define INCIP	{ cpu.romaddr++; if (cpu.romaddr == 0) cpu.rom++; }

#define BINARY(n) (n) & 8 ? '1' : '0', \
		  (n) & 4 ? '1' : '0', \
		  (n) & 2 ? '1' : '0', \
		  (n) & 1 ? '1' : '0'

uint8_t memory[4096];

#define MAXTRACE 30
int maxtrace;

char *trace[MAXTRACE];

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

typedef struct {
	struct {
		uint8_t character[16]; // actually 4 bit... but no need to use bitfields here
		uint8_t status[4];	
	} ramreg[4]; 
	uint8_t port;
} RAM;

RAM ram[16];

uint8_t rom_ports[16];

CPU cpu;

uint8_t *reg(uint8_t opa)
{
	return &cpu.P0 + (opa >> 1);
}

char buffer[255];

void simulate(bool traceonly)
{
	uint16_t rom_addr = cpu.rom*256 + cpu.romaddr;
	uint8_t byte = memory[rom_addr];
	uint8_t opr = (byte & 0xF0) >> 4;
	uint8_t opa = (byte & 0x0F);
	bool condition;
	uint8_t tmp, tmp2, page;
	switch(opr) {
		case 0: if (traceonly) {
				sprintf(buffer, "NOP");
				return;
			}
			INCIP;
			break;		// NOP
		case 0xD:		// LDM
			if (traceonly) {
				sprintf(buffer, "LDM $%02X", opa);
				return;
			}
			cpu.A = opa;
			INCIP;
			break;
		case 0xA:		// LD
			if (traceonly) {
				sprintf(buffer, "LD %d", opa);
				return;
			}
			cpu.A = READREG;
			INCIP;
			break;
		case 0xB:		// XCH
			if (traceonly) {
				sprintf(buffer, "XCH");
				return;
			}
			tmp = cpu.A;
			cpu.A = READREG;
			*(reg(opa)) = WRITEREG;
			INCIP;
			break;
		case 0x8:		// ADD
			if (traceonly) {
				sprintf(buffer, "ADD %d", opa);
				return;
			}
			tmp = READREG;
			tmp += cpu.A + cpu.CY;
			cpu.CY = (tmp & 0x10) >> 4;
			cpu.A = tmp & 0x0F;
			INCIP;
			break;
		case 0x9:		// SUB
			if (traceonly) {
				sprintf(buffer, "SUB %d", opa);
				return;
			}
			tmp = READREG;
			tmp = ~tmp & 0x0F;
			tmp += cpu.A + cpu.CY;
			cpu.CY = (tmp & 0x10) >> 4;
			cpu.A = tmp & 0x0F;
			INCIP;
			break;
		case 0x6:		// INC
			if (traceonly) {
				sprintf(buffer, "INC %d", opa);
				return;
			}
			tmp = READREG;
			tmp = (tmp + 1) & 0x0F;
			*(reg(opa)) = WRITEREG;
			INCIP;
			break;
		case 0xC:		// BBL
			if (traceonly) {
				sprintf(buffer, "BBL %d", opa);
				return;
			}
			cpu.A = opa & 0x0F;
			if (cpu.sp == 0) cpu.sp = 3; else cpu.sp--;
			tmp = cpu.stack[cpu.sp];
			cpu.romaddr = tmp;
			tmp = cpu.stack2[cpu.sp];
			cpu.rom = tmp;
			break;
		case 0x03:		// JIN
			if (opa & 1) {
				if (traceonly) {
					sprintf(buffer, "JIN %d<", opa >> 1);
					return;
				}
				tmp = READREGP;
				if (cpu.romaddr = 0xFF) cpu.rom++;
				cpu.romaddr = tmp;
			} else {	// FIN
				if (traceonly) {
					sprintf(buffer, "FIN %d<", opa >> 1);
					return;
				}
				tmp = *reg(0);
				page = cpu.rom;
				if (cpu.romaddr == 0xFF) {
					page = (page + 1) & 0xF;
				}
				tmp = memory[page*256 + tmp];
				*reg(opa) = tmp;
			}
			INCIP;
			break;
		case 0x2:		// SRC
			if (opa & 1) {
				if (traceonly) {
					sprintf(buffer, "SRC %d<", opa >> 1);
					return;
				}
				INCIP;
				tmp = READREGP;
				cpu.ramaddr = tmp;
			} else {	// FIM
				if (traceonly) {
					tmp = cpu.romaddr;
					tmp2 = cpu.rom;
					tmp++;
					if (!tmp) tmp2++;
					tmp = memory[tmp2*256+tmp];
					sprintf(buffer, "FIM %d< %02X", opa >> 1, tmp);
					return;
				}
				INCIP;	
				tmp = memory[cpu.rom * 256 + cpu.romaddr];
				*(reg(opa)) = WRITEREGP;
				INCIP;
			}
			break;
		case 0x4:		// JUN
			if (traceonly) {
				tmp = cpu.romaddr;
				tmp2 = cpu.rom;
				tmp++;
				if (tmp == 0) tmp2++;
				sprintf(buffer, "JUN $%03X", opa*256 + memory[tmp2*256+tmp]);
				return;
			}
			page = opa;
			INCIP;
			cpu.romaddr = memory[cpu.rom*256 + cpu.romaddr];
			cpu.rom = page;
			break;
		case 0x5:		// JMS
			if (traceonly) {
				tmp = cpu.romaddr;
				tmp2 = cpu.rom;
				tmp++;
				if (tmp == 0) tmp2++;
				sprintf(buffer, "JMS $%03X", opa*256 + memory[tmp2*256+tmp]);
				return;
			}
			page = opa;
			INCIP;
			tmp = cpu.romaddr;
			INCIP;
			tmp2 = cpu.romaddr;
			cpu.romaddr = memory[cpu.rom*256 + tmp];
			cpu.stack2[cpu.sp] = cpu.rom; 
			cpu.rom = page;
			cpu.stack[cpu.sp] = tmp2;
			if (cpu.sp == 3) cpu.sp = 0; else cpu.sp++;
			break;
		case 0x01:		// JCN
			if (traceonly) {
				tmp2 = cpu.rom;
				tmp = cpu.romaddr;
				tmp++;
				if (tmp == 0) tmp2++;
				page = tmp2;
				if (tmp == 254) tmp2++;
				sprintf(buffer, "JCN %1X $%03X", opa, tmp2*256 + memory[page*256+tmp]);
				return;
			}
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
			if (traceonly) {
				tmp2 = cpu.rom;
				tmp = cpu.romaddr;
				tmp++;
				if (tmp == 0) tmp2++;
				page = tmp2;
				if (tmp == 254) tmp2++;
				sprintf(buffer, "ISZ %d %3X", opa, tmp2*256 + memory[page*256+tmp]);
				return;
			}
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
		//**********************************************************//
		case 0xE:
			switch(opa) {
			case 0x9:	// RDM
				if (traceonly) {
					sprintf(buffer, "RDM");
					return;
				}
				cpu.A = ram[cpu.ramaddr >> 6].ramreg[(cpu.ramaddr & 0x30) >> 4].character[cpu.ramaddr & 0x0F] & 0x0F;
				break;
			case 0xC:	// RD0
			case 0xD:	// RD1
			case 0xE:	// RD2
			case 0xF:	// RD3
				if (traceonly) {
					sprintf(buffer, "RD%d", opa-0xC);
					return;
				}
				cpu.A = ram[cpu.ramaddr >> 6].ramreg[(cpu.ramaddr & 0x30) >> 4].status[opa & 3] & 0x0F;
				break;
			case 0xA:	// RDR
				if (traceonly) {
					sprintf(buffer, "RDR");
					return;
				}
				cpu.A = rom_ports[cpu.ramaddr >> 6];
				break;
			case 0x0:	// WRM
				if (traceonly) {
					sprintf(buffer, "WRM");
					return;
				}
				ram[cpu.ramaddr >> 6].ramreg[(cpu.ramaddr & 0x30) >> 4].character[cpu.ramaddr & 0x0F] = cpu.A;
				break;
			case 0x4:	// WR0
			case 0x5:	// WR1
			case 0x6:	// WR2
			case 0x7:	// WR3
				if (traceonly) {
					sprintf(buffer, "WR%d", opa-0x4);
					return;
				}
				ram[cpu.ramaddr >> 6].ramreg[(cpu.ramaddr & 0x30) >> 4].status[opa & 3] = cpu.A;
				break;	
			case 0x2:	// WRR
				if (traceonly) {
					sprintf(buffer, "WRR");
					return;
				}
				rom_ports[cpu.ramaddr >> 6] = cpu.A;
				break;
			case 0x1:	// WMP
				if (traceonly) {
					sprintf(buffer, "WMP");
					return;
				}
				ram[cpu.ramaddr >> 6].port = cpu.A;
				break;
			case 0xB:	// ADM
				if (traceonly) {
					sprintf(buffer, "ADM");
					return;
				}
				tmp = ram[cpu.ramaddr >> 6].ramreg[(cpu.ramaddr & 0x30) >> 4].character[cpu.ramaddr & 0x0F] & 0x0F;
				tmp += cpu.A;
				tmp += cpu.CY;
				cpu.A = tmp & 0xF;
				cpu.CY = (tmp & 0x10) >> 4;
				break;
			case 0x8:	// SBM
				if (traceonly) {
					sprintf(buffer, "SBM");
					return;
				}
				tmp = ram[cpu.ramaddr >> 6].ramreg[(cpu.ramaddr & 0x30) >> 4].character[cpu.ramaddr & 0x0F] & 0x0F;
				tmp = ~tmp & 0xF;
				tmp += cpu.A;
				tmp += cpu.CY;
				cpu.A = tmp & 0xF;
				cpu.CY = (tmp & 0x10) >> 4;
			}
			INCIP;
			break;
		case 0xF:
			switch(opa) {
			case 0:		// CLB
				if (traceonly) {
					sprintf(buffer, "CLB");
					return;
				}
				cpu.A = 0;
				cpu.CY = 0;
				break;
			case 1:		// CLC
				if (traceonly) {
					sprintf(buffer, "CLC");
					return;
				}
				cpu.CY = 0;
				break;
			case 3:		// CMC
				if (traceonly) {
					sprintf(buffer, "CMC");
					return;
				}
				cpu.CY = ~cpu.CY;
				break;
			case 0xA:	// STC
				if (traceonly) {
					sprintf(buffer, "STC");
					return;
				}
				cpu.CY = 1;
				break;
			case 4: 	// CMA
				if (traceonly) {
					sprintf(buffer, "CMA");
					return;
				}
				cpu.A = ~cpu.A;
				break;
			case 2:		// IAC
				if (traceonly) {
					sprintf(buffer, "IAC");
					return;
				}
				tmp = cpu.A;
				tmp++;
				cpu.A = tmp & 0xF;
				cpu.CY = tmp > 15;
				break;
			case 8:		// DAC
				if (traceonly) {
					sprintf(buffer, "DAC");
					return;
				}
				tmp = 0x10;
				tmp |= cpu.A;
				tmp--;
				cpu.A = tmp & 0xF;
				cpu.CY = (tmp & 0x10) >> 4;
				break;
			case 6:		// RAR
				if (traceonly) {
					sprintf(buffer, "RAR");
					return;
				}
				tmp = cpu.A;
				tmp |= cpu.CY * 16;
				cpu.CY = tmp & 1;
				tmp >>= 1;
				cpu.A = tmp & 0xF;
				break;
				
			case 5:		// RAL
				if (traceonly) {
					sprintf(buffer, "RAL");
					return;
				}
				tmp = cpu.A;
				tmp <<= 1;
				tmp |= cpu.CY;
				cpu.CY = (tmp & 0x10) >> 4;
				cpu.A = tmp & 0xF;
				break;
			case 7:		// TCC
				if (traceonly) {
					sprintf(buffer, "TCC");
					return;
				}
				cpu.A = cpu.CY;
				cpu.CY = 0;						
				break;
			case 0xB:	// DAA
				if (traceonly) {
					sprintf(buffer, "DAA");
					return;
				}
				tmp = cpu.A;
				if (tmp > 9 | cpu.CY) tmp += 6;
				cpu.CY = tmp > 15;
				cpu.A = tmp & 0xF;
				break;
			case 0x9:	// TCS
				if (traceonly) {
					sprintf(buffer, "TCS");
					return;
				}
				if (cpu.CY) cpu.A = 10;
				if (cpu.CY == 0) cpu.A = 9;
				cpu.CY = 0;
				break;
			case 0xC:	// KBP
				if (traceonly) {
					sprintf(buffer, "KBP");
					return;
				}
				if (cpu.A == 0) cpu.A = 0; else
				if (cpu.A == 1) cpu.A = 1; else
				if (cpu.A == 2) cpu.A = 2; else
				if (cpu.A == 4) cpu.A = 3; else
				if (cpu.A == 8) cpu.A = 4; else
				cpu.A = 0xF;
				break;
			case 0xD:	// DCL
				if (traceonly) {
					sprintf(buffer, "DCL");
					return;
				}
				cpu.ram = cpu.A & 3;
				break;
			}
			INCIP;
			break;
	}

}

void locate(int y, int x)
{
	printf("\033[%d;%dH", y, x);
}

void clrscr()
{
	printf("\033[H\033[2J");
}

void color(int i)
{
	printf("\033[0;%dm",i);
}

void show_status()
{
	color(36);
	for (int i = 0; i < 4; i++) {
		locate(2+i*7, 2);
		printf("---- 4002 #%02d ----", i);
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 16; k++) {	
				locate(3+i*7 + j, 3+k);
				printf("%x", ram[i].ramreg[j].character[k]);
			}	
		}
		locate(2+i*7+5, 4);
		printf("Port: %c%c%c%c", BINARY(ram[i].port));
	}
	
	color(35);
	locate(2, 24); printf("---- 4004 CPU ----");
	locate(3, 24); printf(" A = %c%c%c%c (%X)", BINARY(cpu.A), cpu.A);
	locate(4, 24); printf(" CY = %c   T = %c", cpu.CY ? '1' : '0', cpu.T ? '1' : '0');
	for (int i = 0; i < 8; i++) {
		uint8_t opa = i*2;
		uint8_t rl = READREG;
		opa = i*2+1;
		uint8_t rr = READREG;
		opa = i*2;
		uint8_t p = READREGP;
		locate(6+i, 24); printf("R%d = %c%c%c%c R%d = %c%c%c%c (%02X)", i*2,
					BINARY(rl), i*2+1, BINARY(rr), p);
	}
	locate(15, 24); printf(" PC %03X", cpu.rom * 256 + cpu.romaddr);
	for (int i = 0; i < 4; i++) {
		if (i == cpu.sp) color(32); else color(35);
		locate(17+i, 24); printf(" STACK[%d] = %03X ", i, cpu.stack2[i] * 256 + cpu.stack[i]);
	}


	color(33);
	for (int i = 0; i < 4; i++) {
		locate(23+i*4, 24); printf("---- 4001 #%02d ----", i);
		locate(24+i*4, 26); printf(" Port: %c%c%c%c", BINARY(rom_ports[i]));
	}


	asprintf(&trace[maxtrace], "%03X  %-20s", cpu.rom * 256 + cpu.romaddr, buffer);
	maxtrace++;
	if (maxtrace == MAXTRACE) {
		free(trace[0]);
		for (int i = 1; i < MAXTRACE; i++) {
			trace[i-1] = trace[i];
		}
		maxtrace = MAXTRACE-1;
	}
	trace[MAXTRACE-1] = NULL;
	for (int i = 0; i < MAXTRACE; i++) {
		if (trace[i]) {
			locate(2+i, 55); printf(trace[i]);
		} else {
		  locate(2+i,55); printf("%-25s", " ");
		}
	}		


}

struct termios orig_termios;

void reset_terminal_mode()
{
	tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
	struct termios new_termios;

	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(struct termios));

	atexit(reset_terminal_mode);
	//cfmakeraw(&new_termios);
	new_termios.c_lflag &= ~ICANON;
	new_termios.c_lflag &= ~ECHO;
//	new_termios.c_lflag &= ~ISIG;
	new_termios.c_cc[VMIN] = 0;
	new_termios.c_cc[VTIME] = 0;


	tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
	int r;
	unsigned char c;
	if ((r = read(0, &c, sizeof(c))) < 0) {
		return r;
	} else {
		return c;
	}

}


int main(int argc, char **argv)
{
	char c;

	setvbuf(stdout, (char *)NULL, _IONBF, 0);

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

	set_conio_terminal_mode();
	color(34);
	clrscr();
	while(true) {
		simulate(true);
		show_status();
		while (!kbhit()) {
		}
		c = getch();
		if (c == 'c') simulate(false);
		if (c == 'q') break;		
	}
}
