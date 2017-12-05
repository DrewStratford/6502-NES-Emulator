#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

struct memory{
  uint8_t* (*decode_address_I)(struct memory*, uint16_t);
};


void write8(struct memory *mem, uint16_t indx, uint8_t writing);
void write16(struct memory *mem, uint16_t indx, uint16_t writing);
uint8_t read8(struct memory *mem, uint16_t indx);
uint16_t read16(struct memory *mem, uint16_t indx);

uint8_t *decode_address(struct memory*, uint16_t);

struct memory * make_flat_2k_mem();

#endif
