#include <cstdint>
#include <cstdio>
#include <fstream>


void mem_read(uint16_t);
void read_image_file(FILE* file);
uint16_t swap16();
void abort();
uint16_t sign_extend(uint16_t, int);
uint16_t mem_read(uint16_t);
void update_flag(uint16_t);

uint16_t memory[UINT16_MAX];

enum Registers
{
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	R_PC,
	R_COND,
	R_COUNT
};

uint16_t registers[R_COUNT];

enum Opcodes
{
	OP_BR = 0, /* branch */
	OP_ADD,    /* add  */
	OP_LD,     /* load */
	OP_ST,     /* store */
	OP_JSR,    /* jump register */
	OP_AND,    /* bitwise and */
	OP_LDR,    /* load register */
	OP_STR,    /* store register */
	OP_RTI,    /* unused */
	OP_NOT,    /* bitwise not */
	OP_LDI,    /* load indirect */
	OP_STI,    /* store indirect */
	OP_JMP,    /* jump */
	OP_RES,    /* reserved (unused) */
	OP_LEA,    /* load effective address */
	OP_TRAP    /* execute trap */
};

enum Flag_Conditions
{
	FL_POSITIVE = 1 << 0,
	FL_ZERO = 1 << 1,
	FL_NEGATIVE = 1 << 2
};

enum TRAP_CODE
{
	TRAP_GETC = 0x20,  /* get character from keyboard */
	TRAP_OUT = 0x21,   /* output a character */
	TRAP_PUTS = 0x22,  /* output a word string */
	TRAP_IN = 0x23,    /* input a string */
	TRAP_PUTSP = 0x24, /* output a byte string */
	TRAP_HALT = 0x25   /* halt the program */
};

void read_image_file(FILE* file)
{
	uint16_t origin;
	fread(&origin, sizeof(uint16_t), 1, file);
	origin = swap16(origin);
	uint16_t max_read = UINT16_MAX - origin;
	uint16_t* p = memory + origin;
	size_t read = fread(p, sizeof(uint16_t), max_read, file);
	while (read-- > 0)
	{
		*p = swap16(*p);
		p++;
	}
}

enum Device_Registers
{
	MR_KBSR = 0xFE00,
	MR_KBDR = 0xFE02
};

void mem_write(uint16_t address, uint16_t value)
{
	memory[address] = value;
}

void mem_read(uint16_t address)
{
	if (address = MR_KBSR)
	{
		if (check_key())
		{
			memory[MR_KBSR] = (1 << 15);
			memory[MR_KBDR] = std::getchar();
		}
		else
			memory[MR_KBSR] = 0;
	}
}

bool read_image(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return 0;
	read_image_file(file);
	fclose(file);
	return 1;
}
uint16_t swap16(uint16_t x)
{
	return (x << 8) | (x >> 8);
}

void update_flag(uint16_t r)
{
	if (registers[r] == 0)
		registers[R_COND] = FL_ZERO;
	else if (registers[r] >> 15 == 1)
		registers[R_COND] = FL_NEGATIVE;
	else
		registers[R_COND] - FL_POSITIVE;

}

uint16_t sign_extend(uint16_t x, int count_bits)
{
	if ((x >> (count_bits - 1)) & 0x1)
		x |= (0xFFFF << count_bits);
	return x;
}
int main()
{
	int PC_START = 0x3000;
	registers[R_PC] = PC_START;

	bool running = 1;

	while (running)
	{
		uint16_t instr = mem_read(registers[R_PC]++);
		uint16_t opcode = instr >> 12;

		switch (opcode)
		{
		case OP_ADD:
			uint16_t destination_register = (instr >> 9) & 0x7;
			uint16_t source_register = (instr >> 6) & 0x7;
			uint16_t immediate_flag = (instr >> 5) & 0x1;
			if (immediate_flag)
			{
				uint16_t lit = sign_extend(instr & 0x1F, 5);
				registers[destination_register] = source_register + lit;
			}
			else
			{
				uint16_t source2_register = instr & 0x7;
				registers[destination_register] = registers[source_register] + registers[source2_register];
			}

			update_flag(destination_register);
		break;
		case OP_AND:
			uint16_t DR = (instr >> 9) & 0x7;
			uint16_t SR1 = (instr >> 6) & 0x7;
			uint16_t immediate_flag = (instr >> 5) & 0x1;
			if (immediate_flag)
			{
				uint16_t lit = sign_extend(instr & 0x1F, 5);
				registers[DR] = registers[SR1] & lit;
			}
			else
			{
				uint16_t SR2 = instr & 0x7;
				registers[DR] = registers[SR1] & registers[SR2];
			}

			update_flag(DR);
		break;
		case OP_NOT:
			uint16_t DR = (instr >> 9) & 0x7;
			uint16_t SR = (instr >> 6) & 0x7;
			registers[DR] = ~registers[SR];
			update_flag(DR);
		break;
		case OP_BR:
			uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
			uint16_t condition_code = (instr >> 9) & 0x7;
			if (condition_code & registers[R_COND])
				registers[R_PC] = registers[R_PC] + pc_offset;
		break;
		case OP_JMP:
			uint16_t BR = (instr >> 6) & 0x7;
			registers[R_PC] = registers[BR];
		break;
		case OP_JSR:
			registers[R_R7] = registers[R_PC];
			if (instr >> 11 & 1)
			{
				uint16_t pc_offset = sign_extend(instr & 0xB, 11);
				registers[R_PC] += pc_offset;
			}
			else
			{
				uint16_t BR = (instr >> 6) & 0x7;
				registers[R_PC] += registers[BR];
			}
		break;
		case OP_LD:
			uint16_t destination_register = (instr >> 9) & 0x7;
			uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
			registers[destination_register] = mem_read(registers[R_PC] + pc_offset);
			update_flag(destination_register);
		break;
		case OP_LDR:
			uint16_t destination_register = (instr >> 9) & 0x7;
			uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
			registers[destination_register] = mem_read(mem_read(registers[R_PC] + pc_offset));
			update_flag(destination_register);
		break;
		case OP_LEA:
		//{LEA, 7}
		break;
		case OP_ST:
		//{ST, 7}
		break;
		case OP_STI:
		//{STI, 7}
		break;
		case OP_STR:
		//{STR, 7}
		break;
		case OP_TRAP:
			switch (instr & 0xFF)
			{
			case TRAP_GETC:
				registers[R_R1] = (uint16_t)std::getchar();
			break;
			case TRAP_OUT:
				putc((char)registers[R_R0], stdout);
			break;
			case TRAP_PUTS:
				uint16_t* ch = memory + registers[R_R0];
				while (*ch)
				{
					putc((char)*ch, stdout);
					ch++;
				}
				std::fflush(stdout);
			break;
			case TRAP_IN:
				std::printf("Enter a character:");
				registers[R_R0] = (uint16_t)std::getchar();
			break;
			case TRAP_PUTSP:
				uint16_t* c = memory + registers[R_R0];
				while (*c)
				{
					char char1 = (*c) & 0xFF;
					putc(char1, stdout);
					char char2 = (*c) >> 8;
					if (char2) putc(char2, stdout);
					++c;
				}
				fflush(stdout);
			break;
			case TRAP_HALT:
				puts("ABORT");
				std::fflush(stdout);
				running = 0;
			break;
			}
		break;
		case OP_RES:
		case OP_RTI:
		default:
			abort();
		break;
		}
		
	}



}