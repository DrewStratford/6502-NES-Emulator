
CFLAGS ?= -O2 -std=gnu11 
NAME ?= ricoh_cpu
CC       := gcc
CC       := gcc
LIBS     := -lm -lX11
INCLUDES := -I.
CFLAGS   := $(CFLAGS) $(INCLUDES) $(LIBS)

all : ricoh

ricoh : 6502.o main.o memory.o
	$(CC) -o $@ $(CFLAGS) 6502.o main.o memory.o

gui : 6502.o gui.o memory.o nes_memory.o
	$(CC) -o $@  $(CFLAGS) 6502.o gui.o memory.o

%.o : %.c
	$(CC) -o $@ -c $(CFLAGS) $<

.PHONY: clean all
clean:
	rm -f ricoh
	rm -f *~
	rm -f *.o
	rm -f gui
	rm -f ricoh
