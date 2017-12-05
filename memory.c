#include <stdio.h>
#include <stdlib.h>

#include "memory.h"


uint8_t read8(struct memory *mem, uint16_t indx){
  return *decode_address(mem, indx);
}

uint16_t read16(struct memory *mem, uint16_t ptr){
  uint16_t lo = read8(mem, ptr);
  uint16_t hi = read8(mem, ptr+1) << 8;
  return hi | lo;
}

void write8(struct memory *mem, uint16_t indx, uint8_t writing){
  uint8_t * addr = decode_address(mem, indx);
  *addr = writing;
}

void write16(struct memory *mem, uint16_t indx, uint16_t writing){
  write8(mem, indx, (uint8_t) writing);
  write8(mem, indx+1, (uint8_t) (writing >>8));
}

uint8_t *decode_address(struct memory* mem, uint16_t addr){
  return mem->decode_address_I(mem, addr);
}


/*
 * This defines the memory interface used in the easy 6502 tutorials.
 * It's fairly simple, and provides a good example of how to use the
 * memory interface.
 */
struct flat_2k_mem{
  //IMPORTANT: a valid memory struct has to be the first item in an IFAC
  struct memory mem_iface;
  uint8_t* mem;
};

uint8_t * decode_flat_2k(struct memory *memory, uint16_t addr){
  struct flat_2k_mem * mem_2k = (struct flat_2k_mem *) memory;
  return mem_2k->mem + (addr % 2048);
}

struct memory * make_flat_2k_mem(){
  struct flat_2k_mem * out = malloc(sizeof(struct flat_2k_mem));
  out->mem = malloc(2048 * sizeof(uint8_t));
  out->mem_iface.decode_address_I = decode_flat_2k;

  return (struct memory*)out;
}
