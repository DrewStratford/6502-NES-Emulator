#include <stdlib.h>

#include "nes_memory.h"

struct nes_memory{
  struct memory mem_iface;
  uint8_t *ram;
  uint8_t *ppu;
  
};


#define RAM_END 0x1FFF
#define PPU_END 0x3FFF
#define APU_END 0x4017
#define APU_DISABLED_END 0x401F
#define CART_END 0xFFFF

uint8_t * decode_nes(struct memory *memory, uint16_t addr){
  struct nes_memory * mem_nes = (struct nes_memory*) memory;

  if(addr <= RAM_END){
    //address ram
    return mem_nes->ram + (addr & 0x7FF);

  } else if(addr <= PPU_END){
    //address PPU registers
  } else if(addr <= APU_END){
    //address APU/IO registers
  } else if(addr <= APU_DISABLED_END){
  } else{
    //address cartridge
  }
}

struct memory* make_nes_mem(){
  struct nes_memory* out = malloc(sizeof(struct nes_memory));
  out->mem_iface.decode_address_I = decode_nes;

  return (struct memory*)out;
}
