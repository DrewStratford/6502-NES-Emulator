#ifndef NES_CPU_H
#define NES_CPU_H

#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

// the NES only has 2KiB of RAM memory
// so this mask is used to implement mirroring
#define NES_RAM_MASK 0x07FF
// the higher 3 bits of an address determine what
// memory chip is accessed. They are as follows.
#define RAM 0b000
#define PPU 0b001 //PPU registers, not PPU ram.
#define ROM 0b100 //Technically 0b1XX
// The PPU registers is also mirrored; though, every 8 bytes.
// At this stage I think it's best to calculate this using (%)

struct cpu_info{
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint16_t pc;
  //stack register
  uint8_t s;
  //ptr to mem
  struct memory *mem;

  //status registers
  uint8_t N;
  uint8_t V;

  uint8_t D;
  uint8_t I;
  uint8_t Z;
  uint8_t C;

  int cycles;
  int visual_dirty;

  int finished;
  int stats;
};


/*
void write8(struct cpu_info *cpu, uint16_t indx, uint8_t writing);
uint8_t read8(struct cpu_info *cpu, uint16_t indx);
*/

void init_cpu_info(struct cpu_info *cpu, struct memory*);
int load_file_to_mem(FILE *file, struct cpu_info *cpu, int point);
void step(struct cpu_info *cpu);

void trigger_nmi(struct cpu_info *cpu);
void trigger_irq(struct cpu_info *cpu);


//the 58 valid opcodes, plus an additional error code
enum OpCode {
  BADOP,
  ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
  CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
  JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
  RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA
};

extern char *opcode_strings[59];

enum AddressMode {
  abso, abx, aby, zp, zpx, zpy, izx, izy, ind, imm, rel, noAddressMode
};

extern char *addressMode_strings[12]; 

/*
 * Tables for decoding the instructions
 *
 * These tables are filled using information gathered from:
 *    http://visual6502.org/wiki/index.php?title=6502_all_256_Opcodes
 */

extern enum AddressMode int_address_modes[256];


// the cycles used per instruction
extern int int_cycles[256];
extern int int_width[256];
//the opcode
extern enum OpCode int_opcodes[256];
  
#endif
