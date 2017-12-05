# NES / 6502 emulator

Currently this is an emulator for the 6502 cpu, written in C using X11.
The aim of this project is to create a nes emulator with a modular
core design, in order to allow us to branch out into emulating other 6502 based 
machines (eg commodore 64, atari 6000).

## Current Progress

Core 6502 emulation is complete except for a few nich features like decimal mode
(which is disabled on the nes). Currently, we are correctly able to emulate the machine
described at [easy 6502](http://skilldrick.github.io/easy6502/), including the snake clone.


## Building / Running

This project requires a C compiler and the X11 libraries. The project is built by a makefile, so
you can enter the projects folder and enter:
	'make gui'

This will create an exectuable named gui, which can be run like so:
	'./gui binary/snake.bin'

There are a few test programs in 'binary', which are mainly taken from [easy 6502](http://skilldrick.github.io/easy6502/).
The most interesting on is, by far, snake.bin (use wasd to move).

## Creating new programs

I use the assembler at [easy 6502](http://skilldrick.github.io/easy6502/), though this ouputs hex,
whereas my emulator runs on binary files. The solution is to remove spaces from the output and
convert it to binary (ie. using xxd -r -p on linux).

---

## Useful links

	* [wiki.nesdev.com](wiki.nesdev.com) - general nes info
	* [http://obelisk.me.uk/6502/reference.html](http://obelisk.me.uk/6502/reference.html) - 6502 instruction set
	* [easy 6502](http://skilldrick.github.io/easy6502/) - 6502 tutorial, assembler and debugger.
