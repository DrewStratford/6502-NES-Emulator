
#include "6502.h"

/*
 * An implementation of a 6502 cpu (i.e. that which is used in
 * the NES)
 */


/*
 * ============================================
 * PARSING INSTRUCTIONS
 * ============================================
 * 
 * Instructions begin with a byte:
 *   AAA BBB CC
 * Where CC specifies one of four modes; AAA, in conjunction
 * with CC, specifies the opCode; and BBB, in conjunction 
 * with CC, specifies the the addressing mode.
 * NOTE: there are no valid instructions where CC=11
 * so there only 24 instructions in this form.
 * 
 * There are a further 8 conditional jumps of the form
 *   XX Y 10000
 * where XX is the flag we consider and Y is true/false.
 *
 * Finally, there are a further 26 instructions that do not
 * fit in a pattern.
 *
 * in total there 58 'valid' instructions.
 *
 * As many instructions do not follow an easily encoded
 * pattern and there are only 256 possible leading bytes, we instead
 * use hand filled tables to calculate the instruction.
 *
 * These tables are filled using information gathered from:
 *    http://visual6502.org/wiki/index.php?title=6502_all_256_Opcodes
 */


void init_cpu_info(struct cpu_info *cpu, struct memory *mem){
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->pc = 0;
  cpu->s = 0;
  cpu->N = 0; cpu->V = 0; cpu->D=0; cpu->I = 0;
  cpu->Z = 0; cpu->C = 0;
  cpu->finished = 0;
  cpu->visual_dirty = 1;
  cpu->cycles = 0;
  // allocates 2kB of memory
  //cpu->mem = malloc(2048 * sizeof(uint8_t));
  cpu->mem = mem;
}

/*
 * TODO: this will need to be remade to better fit the new memory interface
 */
int load_file_to_mem(FILE *file, struct cpu_info *cpu, int point){
  uint8_t *m = decode_address(cpu->mem, (uint16_t)point);
  return fread(m, sizeof(uint8_t), 2048, file);
}

int print_registers(struct cpu_info *cpu){
  printf("a %x, x %x, y %x, pc %x\n status %d%d11%d%d%d%d\n",
	 cpu->a, cpu->x, cpu->y, cpu->pc,
	 cpu->N, cpu->V, cpu->D, cpu->I, cpu->Z, cpu->C);
}



void push8(uint8_t pushing, struct cpu_info *cpu){
  write8(cpu->mem, 0x100|cpu->s, pushing);
  cpu->s--;
}

void push16(uint16_t pushing, struct cpu_info *cpu){
  uint8_t h = pushing >> 8;
  uint8_t l = pushing;
  push8(h, cpu);
  push8(l, cpu);
}

uint8_t pull8(struct cpu_info *cpu){
  cpu->s++;
  return read8(cpu->mem, 0x100 | cpu->s);
}

uint16_t pull16(struct cpu_info *cpu){
  uint8_t l = pull8(cpu);
  uint8_t h = pull8(cpu);
  return (h << 8) | l;
}


uint8_t rotr(uint8_t i, int n){
  int j = i << n;
  int k = i >> (8 - n);
  return j | k;
}

uint8_t rotl(uint8_t i, int n){
  int j = i << (8 - n);
  int k = i >> n;
  return j | k;
}


void pull_status(struct cpu_info *cpu){
  int pulling = pull8(cpu);

  cpu->N = (pulling >> 7) & 1;
  cpu->V = (pulling >> 6) & 1;
  cpu->D = (pulling >> 3) & 1;
  cpu->I = (pulling >> 2) & 1;
  cpu->Z = (pulling >> 1) & 1;
  cpu->C = pulling & 1;
}

void push_status(struct cpu_info *cpu){
  uint8_t pushing = cpu->N << 7;
  pushing |= cpu->V << 6;
  pushing |= 0b00 << 4;
  pushing |= cpu->D << 3;
  pushing |= cpu->I << 2;
  pushing |= cpu->Z << 1;
  pushing |= cpu->C;
  push8(pushing, cpu);
}

void push_status_brk(struct cpu_info *cpu){
  uint8_t pushing = cpu->N << 7;
  pushing |= cpu->V << 6;
  pushing |= 0b10 << 4;
  pushing |= cpu->D << 3;
  pushing |= cpu->I << 2;
  pushing |= cpu->Z << 1;
  pushing |= cpu->C;
  push8(pushing, cpu);
}

void trigger_nmi(struct cpu_info *cpu){
  push16(cpu->pc, cpu);
  push_status(cpu);
  cpu->I = 1;
  cpu->pc = read8(cpu->mem,0xfffa);
  //todo does this add cycles? (probably)
}

void trigger_irq(struct cpu_info *cpu){
  if(cpu->I == 0){
    push16(cpu->pc, cpu);
    push_status(cpu);
    cpu->I = 1;
    cpu->pc = read8(cpu->mem,0xfffe);
  //todo does this add cycles? (probably)
  }
}

void step(struct cpu_info *cpu){

  //check how fast we can run
  cpu->stats++;
  //account for varying instruction cycles
  // doesn't take pages into account
  if(cpu->cycles > 0){
    cpu->cycles--;
    return;
  }

  int oldPc = cpu->pc;
  uint8_t instr = read8(cpu->mem, oldPc);
  enum OpCode op = int_opcodes[instr];
  enum AddressMode addrMode = int_address_modes[instr];
  int cycles = int_cycles[instr];
  int width = int_width[instr];
  cpu->cycles = cycles;

  /*
   * Here we use the address mode to calculate the arguement to be given to
   * the op code. The argument is passed as a pointer to the arguments value;
   * hence, immediate values point to the address right after the pc and
   * absolute address are found by reading the address stored in the position
   * after the pc.
   */
  int16_t address = 0;
  switch(addrMode){
    //we read a 16 bit absolute address, hence the bitshifting.
  case abso:
    address = read16(cpu->mem, oldPc+1);
    break;
  case abx:
    address = read16(cpu->mem, oldPc+1) + (int8_t)cpu->x;
    break;
  case aby:
    address = read16(cpu->mem, oldPc+1) + (int8_t)cpu->y;
    break;
    // the value is immediately after the pc, so it's pc + 1
  case imm:
    address = oldPc + 1;
    break;
    // used specifically for jumps so based of the pc
  case rel:
    address = oldPc + width + (int8_t)read8(cpu->mem,oldPc+1);
    break;
  case zp:
    address = read8(cpu->mem,oldPc+1);
    break;
  case zpx:
    address = read8(cpu->mem,oldPc+1) + (int8_t)cpu->x;
    break;
  case zpy:
    address = read8(cpu->mem,oldPc+1) + (int8_t)cpu->y;
    break;
    // this uses an absolute address to find another address
    // hence, we copy abso then look that address up in mem
  case ind:
    address = read16(cpu->mem, read16(cpu->mem, oldPc+1));
    break;
  case izx:
    address = read16(cpu->mem, (int16_t)read8(cpu->mem,oldPc+1) + (int16_t)cpu->x);
    break;
  case izy:
    address = read16(cpu->mem, (int16_t)read8(cpu->mem, oldPc+1)) + (int16_t)cpu->y;
    break;
  default:
    break;
  }


  //diagnostic to see what we are doing
  //print_registers(cpu);
  //printf("%s %x %s\n", opcode_strings[op], address, addressMode_strings[addrMode]);
  //getchar();

  //inc pc by instruction length
  cpu->pc +=width;

  switch(op){
  case ADC:
    {
      uint8_t over = cpu->a + read8(cpu->mem,address) + cpu->C; 

      cpu->C = over < cpu->a ;
      cpu->a = over;
      cpu->N = (cpu->a >> 7) & 1;
      cpu->Z = cpu->a == 0;
    }
    break;

  case AND:
    cpu->a = cpu->a & read8(cpu->mem, address);
    cpu->Z = cpu->a == 0 ? 1 : 0;
    cpu->N = (cpu->a >> 7) & 1;
    break;

  case ASL:
	// this is based on the old a so needs to be first
    cpu->C = (cpu->a >> 7) & 1;

    cpu->a = cpu->a << read8(cpu->mem, address);
    cpu->Z = cpu->a == 0 ? 1 : 0;
    cpu->N = (cpu->a >> 7) & 1;
    break;

  case BCC:
    if(cpu->C == 0){
      cpu->pc = address;
    }
    break;

  case BCS:
    if(cpu->C == 1){
      cpu->pc = address;
    }
    break;

  case BEQ:
    if(cpu->Z == 1){
      cpu->pc = address;
    }
    break;

  case BIT:
    {
      uint8_t val = read8(cpu->mem, address);
      cpu->V = (val >> 6) & 1;
      cpu->N = (val >> 7) & 1;
      cpu->Z = (cpu->a &val) == 0 ? 1 : 0;
    }
    break;

  case BMI:
    if(cpu->N == 1){
      cpu->pc = address;
    }
    break;

  case BNE:
    if(cpu->Z == 0){
      cpu->pc = address;
    }
    break;

  case BPL:
    if(cpu->N == 0){
      cpu->pc = address;
    }
    break;

  case BRK:
    //TODO:
    // this is just placeholder behaviour
    cpu->finished = 1;
    break;
    
  case BVC:
    if(cpu->V == 0){
      cpu->pc = address;
    }
    break;

  case BVS:
    if(cpu->V == 1){
      cpu->pc = address;
    }
    break;

  case CLC:
    cpu->C = 0;
    break;

  case CLD:
    cpu->D = 0;
    break;

  case CLI:
    cpu->I = 0;
    break;

  case CLV:
    cpu->V = 0;
    break;

  case CMP:
    {
      uint8_t val = read8(cpu->mem, address);
      cpu->C = cpu->a >= val;
      cpu->Z = cpu->a == val;
      cpu->N = ((cpu->a - val) >> 7) & 1;
    }
    break;

  case CPX:
    {
      uint8_t val = read8(cpu->mem, address);
      cpu->C = cpu->x >= val;
      cpu->Z = cpu->x == val;
      cpu->N = ((cpu->x - val) >> 7) & 1;
    }
    break;

  case CPY:
    {
      uint8_t val = read8(cpu->mem, address);
      cpu->C = cpu->y >= val;
      cpu->Z = cpu->y == val;
      cpu->N = ((cpu->y - val) >> 7) & 1;
    }
    break;

  case DEC:
    {
      uint8_t val = read8(cpu->mem, address) - 1;
      write8(cpu->mem, address, val);
      cpu->Z = val == 0 ? 1 : 0;
      cpu->N = (val >> 7) & 1;
    }
    break;

  case DEX:
    cpu->x -= 1;
    cpu->Z = cpu->x == 0 ? 1 : 0;
    cpu->N = (cpu->x >> 7) & 1;
    break;

  case DEY:
    cpu->y -= 1;
    cpu->Z = cpu->y == 0 ? 1 : 0;
    cpu->N = (cpu->y >> 7) & 1;
    break;

  case EOR:
    {
      uint8_t val = cpu->a ^ read8(cpu->mem, address);
      cpu->Z = cpu->a == 0 ? 1 : 0;
      cpu->N = (cpu->a >> 7) & 1;
    }
    break;

  case INC:
    {
      uint8_t val = read8(cpu->mem, address) + 1;
      write8(cpu->mem, address, val);
      cpu->Z = val == 0 ? 1 : 0;
      cpu->N = (val >> 7) & 1;
    }
    break;

  case INX:
    cpu->x += 1;
    cpu->Z = cpu->x == 0 ? 1 : 0;
    cpu->N = (cpu->x >> 7) & 1;
    break;

  case INY:
    cpu->y += 1;
    cpu->Z = cpu->y == 0 ? 1 : 0;
    cpu->N = (cpu->y >> 7) & 1;
    break;

  case JMP:
    cpu->pc = address;
    break;

  case JSR:
    //push the old pc -1 to stack (this is 16 bit)
    push16(cpu->pc-1, cpu);
    cpu->pc = address;
    break;

  case LDA:
    cpu->a = read8(cpu->mem, address);
    cpu->Z = cpu->a == 0 ? 1 : 0;
    cpu->N = (cpu->a >> 7) & 1;
    break;

  case LDX:
    cpu->x = read8(cpu->mem, address);
    cpu->Z = cpu->x == 0 ? 1 : 0;
    cpu->N = (cpu->x >> 7) & 1;
    break;

  case LDY:
    cpu->y = read8(cpu->mem, address);
    cpu->Z = cpu->y == 0 ? 1 : 0;
    cpu->N = (cpu->y >> 7) & 1;
    break;

  case LSR:
    if(addrMode == noAddressMode){
      // shift the accumulator
      cpu->C = cpu->a & 1;
      cpu->a >>= 1;
      cpu->Z = cpu->a == 0 ? 1 : 0;
      cpu->N = (cpu->a >> 7) & 1;
    } else{
      uint8_t old = read8(cpu->mem, address);
      uint8_t new = old >> 1;
      cpu->C = old & 1;
      write8(cpu->mem, address, old >> 1);
      cpu->Z = new == 0 ? 1 : 0;
      cpu->N = (new >> 7) & 1;
    }

    break;

  case NOP:
    break;

  case ORA:
    cpu->y = cpu->a | read8(cpu->mem, address);
    cpu->Z = cpu->a == 0 ? 1 : 0;
    cpu->N = (cpu->a >> 7) & 1;
    break;

  case PHA:
    push8(cpu->a,cpu);
    break;

  case PHP:
    push_status(cpu);
    break;

  case PLA:
    cpu->a = pull8(cpu);
    break;

  case PLP:
    pull_status(cpu);
    break;

  case ROL:
    //TODO: Check this is correct
    if(addrMode == noAddressMode){
      uint8_t val = cpu->a;
      uint8_t working = val << 1;
      working |= val >> 7;

      cpu->a = working;
      cpu->Z = (working >> 7) & 0x01;
      cpu->C = val & 0x01;
    }else{
      uint8_t val = read8(cpu->mem,address);
      uint8_t working = val << 1;
      working |= val >> 7;

      write8(cpu->mem, address, working);
      cpu->C = val & 0x01;
      cpu->Z = (working >> 7) & 0x01;
    }
    break;
    
  case ROR:
    //TODO: Check this is correct
    if(addrMode == noAddressMode){
      uint8_t val = cpu->a;
      uint8_t working = val >> 1;
      working |= val << 7;

      cpu->a = working;
      cpu->Z = (working >> 7) & 0x01;
      cpu->C = val & 0x01;
    }else{
      uint8_t val = read8(cpu->mem,address);
      uint8_t working = val >> 1;
      working |= val << 7;

      write8(cpu->mem, address, working);
      cpu->C = val & 0x01;
      cpu->Z = (working >> 7) & 0x01;
    }
    break;

  case RTI:
    pull_status(cpu);
    cpu->pc = pull16(cpu);
    break;

  case RTS:
    //pull the old pc -1 from the stack (this is 16 bit)
    cpu->pc = pull16(cpu) + 1;
    break;
  
  case SBC:
    {
      uint8_t c = 1 + (-cpu->C);
      uint8_t over = cpu->a + (-read8(cpu->mem,address)) + (-c); 

      cpu->C = over < cpu->a ;
      cpu->a = over;
      cpu->N = (cpu->a >> 7) & 1;
      cpu->Z = cpu->a == 0;
    }
    break;

  case SEC:
    cpu->C = 1;
    break;

  case SED:
    cpu->D = 1;
    break;

  case SEI:
    cpu->I = 1;
    break;

  case STA:
    write8(cpu->mem, address, cpu->a);
    // sets dirty bit if visual mem is drawn to
    if ((address >= 0x200) && (address <= 0x5ff)) {
      cpu->visual_dirty = 1;
    }
    break;

  case STX:
    write8(cpu->mem, address, cpu->x);
    // sets dirty bit if visual mem is drawn to
    if ((address >= 0x200) && (address <= 0x5ff)) {
      cpu->visual_dirty = 1;
    }
    break;

  case STY:
    write8(cpu->mem, address, cpu->y);
    // sets dirty bit if visual mem is drawn to
    if ((address >= 0x200) && (address <= 0x5ff)) {
      cpu->visual_dirty = 1;
    }
    break;

  case TAX:
    cpu->x = cpu->a;
    cpu->Z = cpu->x == 0 ? 1 : 0;
    cpu->N = (cpu->x >> 7) & 1;
    break;

  case TAY:
    cpu->y = cpu->a;
    cpu->Z = cpu->y == 0 ? 1 : 0;
    cpu->N = (cpu->y >> 7) & 1;
    break;

  case TSX:
    cpu->x = cpu->s;
    cpu->Z = cpu->x == 0 ? 1 : 0;
    cpu->N = (cpu->x >> 7) & 1;
    break;

  case TXA:
    cpu->a = cpu->x;
    cpu->Z = cpu->a == 0 ? 1 : 0;
    cpu->N = (cpu->a >> 7) & 1;
    break;

  case TXS:
    cpu->s = cpu->x;
    cpu->Z = cpu->s == 0 ? 1 : 0;
    cpu->N = (cpu->s >> 7) & 1;
    break;

  case TYA:
    cpu->a = cpu->y;
    cpu->Z = cpu->a == 0 ? 1 : 0;
    cpu->N = (cpu->a >> 7) & 1;
    break;
  }
}

void print_bin(uint8_t i){
  if (i < 2){
    printf("%d", i);
    return;
  }
  int d = i % 2;
  int nxt = i / 2;
  print_bin(nxt);
  printf("%d", i);
}



char *opcode_strings[59] = {
  "BADOP",
  "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK", "BVC", "BVS", "CLC",
  "CLD", "CLI", "CLV", "CMP", "CPX", "CPY", "DEC", "DEX", "DEY", "EOR", "INC", "INX", "INY", "JMP",
  "JSR", "LDA", "LDX", "LDY", "LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL", "ROR", "RTI",
  "RTS", "SBC", "SEC", "SED", "SEI", "STA", "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA"
};


char *addressMode_strings[12] = {
  "abso", "abx", "aby", "zp", "zpx", "zpy", "izx", "izy", "ind", "imm", "rel", "noAddressMode"
};


/*
 * Tables for decoding the instructions
 *
 * These tables are filled using information gathered from:
 *    http://visual6502.org/wiki/index.php?title=6502_all_256_Opcodes
 */

enum AddressMode int_address_modes[256] = {
  noAddressMode, izx, noAddressMode, izx, zp, zp, zp, zp, noAddressMode, imm,
  noAddressMode, imm, abso, abso, abso, abso, rel, izy, noAddressMode, izy,
  zpx, zpx, zpx, zpx, noAddressMode, aby, noAddressMode, aby, abx, abx,
  abx, abx, abso, izx, noAddressMode, izx, zp, zp, zp, zp,
  noAddressMode, imm, noAddressMode, imm, abso, abso, abso, abso, rel, izy,
  noAddressMode, izy, zpx, zpx, zpx, zpx, noAddressMode, aby, noAddressMode,
  aby, abx, abx, abx, abx, noAddressMode, izx, noAddressMode, izx, zp, zp, zp,
  zp, noAddressMode, imm, noAddressMode, imm, abso, abso, abso, abso, rel, izy,
  noAddressMode, izy, zpx, zpx, zpx, zpx, noAddressMode, aby, noAddressMode,		
  aby, abx, abx, abx, abx, noAddressMode, izx, noAddressMode, izx, zp,
  zp, zp, zp, noAddressMode, imm, noAddressMode, imm, ind, abso, abso,
  abso, rel, izy, noAddressMode, izy, zpx, zpx, zpx, zpx, noAddressMode, aby,
  noAddressMode, aby, abx, abx, abx, abx, imm, izx, imm, izx,
  zp, zp, zp, zp, noAddressMode, imm, noAddressMode, imm, abso, abso,
  abso, abso, rel, izy, noAddressMode, izy, zpx, zpx, zpy, zpy, noAddressMode,
  aby, noAddressMode, aby, abx, abx, aby, aby, imm, izx, imm, izx, zp, zp,
  zp, zp, noAddressMode, imm, noAddressMode, imm, abso, abso, abso, abso, rel,
  izy, noAddressMode, izy, zpx, zpx, zpy, zpy, noAddressMode, aby, noAddressMode,
  aby, abx, abx, aby, aby, imm, izx, imm, izx, zp, zp, zp, zp, noAddressMode,
  imm, noAddressMode, imm, abso, abso, abso, abso, rel, izy, noAddressMode,
  izy, zpx, zpx, zpx, zpx, noAddressMode, aby, noAddressMode, aby, abx,
  abx, abx, abx, imm, izx, imm, izx, zp, zp, zp, zp, noAddressMode, imm,
  noAddressMode, imm, abso, abso, abso, abso, rel, izy, noAddressMode, izy,
  zpx, zpx, zpx, zpx, noAddressMode, aby, noAddressMode, aby, abx, abx, abx, abx
  };


// the cycles used per instruction
int int_cycles[256] ={
  7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6, 3, 5, 0, 8,
  4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, 6, 6, 0, 8, 3, 3, 5, 5, 
  4, 2, 2, 2, 4, 4, 6, 6, 2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 
  4, 4, 7, 7, 6, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6, 
  3, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, 6, 6, 0, 8, 
  3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6, 2, 5, 0, 8, 4, 4, 6, 6, 
  2, 4, 2, 7, 4, 4, 7, 7, 2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 
  4, 4, 4, 4, 3, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5, 
  2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4, 2, 5, 0, 5, 
  4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4, 2, 6, 2, 8, 3, 3, 5, 5, 
  2, 2, 2, 2, 4, 4, 6, 6, 3, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 
  4, 4, 7, 7, 2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, 
  2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7 
};


int int_width[256] = {
  1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
  3, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
  1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
  1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 0, 3, 0, 0,
  2, 2, 2, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
  2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0
};

//the opcode
enum OpCode int_opcodes[256] = {
  BRK, ORA, BADOP, BADOP, BADOP, ORA, ASL, BADOP, PHP, ORA, ASL, 
  BADOP, BADOP, ORA, ASL, BADOP, BPL, ORA, BADOP, BADOP, BADOP, ORA, 
  ASL, BADOP, CLC, ORA, BADOP, BADOP, BADOP, ORA, ASL, BADOP, JSR,
  AND, BADOP, BADOP, BIT, AND, ROL, BADOP, PLP, AND, ROL, BADOP, 
  BIT, AND, ROL, BADOP, BMI, AND, BADOP, BADOP, BADOP, AND, ROL, 
  BADOP, SEC, AND, BADOP, BADOP, BADOP, AND, ROL, BADOP, RTI, EOR, 
  BADOP, BADOP, BADOP, EOR, LSR, BADOP, PHA, EOR, LSR, BADOP, JMP, 
  EOR, LSR, BADOP, BVC, EOR, BADOP, BADOP, BADOP, EOR, LSR, BADOP, 
  CLI, EOR, BADOP, BADOP, BADOP, EOR, LSR, BADOP, RTS, ADC, BADOP, 
  BADOP, BADOP, ADC, ROR, BADOP, PLA, ADC, ROR, BADOP, JMP, ADC, 
  ROR, BADOP, BVS, ADC, BADOP, BADOP, BADOP, ADC, ROR, BADOP, SEI, 
  ADC, BADOP, BADOP, BADOP, ADC, ROR, BADOP, BADOP, STA, BADOP, BADOP, 
  STY, STA, STX, BADOP, DEY, BADOP, TXA, BADOP, STY, STA, STX, 
  BADOP, BCC, STA, BADOP, BADOP, STY, STA, STX, BADOP, TYA, STA, 
  TXS, BADOP, BADOP, STA, BADOP, BADOP, LDY, LDA, LDX, BADOP, LDY, 
  LDA, LDX, BADOP, TAY, LDA, TAX, BADOP, LDY, LDA, LDX, BADOP, 
  BCS, LDA, BADOP, BADOP, LDY, LDA, LDX, BADOP, CLV, LDA, TSX, 
  BADOP, LDY, LDA, LDX, BADOP, CPY, CMP, BADOP, BADOP, CPY, CMP,
  DEC, BADOP, INY, CMP, DEX, BADOP, CPY, CMP, DEC, BADOP, BNE, 
  CMP, BADOP, BADOP, BADOP, CMP, DEC, BADOP, CLD, CMP, BADOP, BADOP,
  BADOP, CMP, DEC, BADOP, CPX, SBC, BADOP, BADOP, CPX, SBC, INC, 
  BADOP, INX, SBC, NOP, BADOP, CPX, SBC, INC, BADOP, BEQ, SBC, 
  BADOP, BADOP, BADOP, SBC, INC, BADOP, SED, SBC, BADOP, BADOP, BADOP, 
  SBC, INC, BADOP 
};
