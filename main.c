#include <stdio.h>
#include <stdlib.h>

#include "6502.h"

int main(int argc, char **argv){

  if(argc < 2) return 0;
  /*
   * The NES maps the rom to 0x8000 - 0xFFFF
   * For the easy NES tutorial, The PC begins at 0x0600, so we load the code there.
   */
  struct cpu_info cpu;
  init_cpu_info(&cpu);
  FILE * file = fopen(argv[1], "r");
  load_file_to_mem(file, &cpu, 0x0600);
  fclose(file);
  cpu.pc = 0x0600;
  cpu.s = 0xFF;
  
  int i = 0;
  while(!cpu.finished){
    step(&cpu);
    i++;
    getchar();
  }


  return 0;
}
