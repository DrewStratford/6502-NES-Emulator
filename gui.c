#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include "6502.h"

#define WIDTH 32
#define HEIGHT 32

long my_event_mask = KeyPressMask;

Display *dis;
int screen;
Window win;
GC gc;

uint32_t rgba8 (uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0));
}
uint32_t rgb8 (uint8_t r, uint8_t g, uint8_t b) {
  return rgba8(r,g,b,0xff);
}


//stuff for timing
//gets time in ms
long long get_timestamp() {
    struct timeval t; 
    gettimeofday(&t, NULL); 
    long long ms = t.tv_sec*1000LL + t.tv_usec/1000; 
    return ms;
}

// we put this in atexit
void close_x() {
  //ensure we only run this once
  static Bool cont = True;
  if(cont){
    XFreeGC(dis, gc);
    XDestroyWindow(dis,win);
    XCloseDisplay(dis);
    cont = False;
  }
}


void init_x() {
  unsigned long black,white;

  dis        = XOpenDisplay((char *)0);
  screen     = DefaultScreen(dis);
  black      = BlackPixel(dis,screen);
  white      = WhitePixel(dis, screen);

  win = XCreateSimpleWindow(dis,DefaultRootWindow(dis),0,0,
			    500, 500, 5, white, black);

  XSetStandardProperties(dis,win,"6502 emu","6502 emu",None,NULL,0,NULL);

  XSelectInput(dis, win, ExposureMask|ButtonPressMask|KeyPressMask);

  gc=XCreateGC(dis, win, 0,0);

  XSetBackground(dis,gc,white);
  XSetForeground(dis,gc,black);

  XClearWindow(dis, win);
  XMapRaised(dis, win);
}

uint32_t pallette[16] = {
  0xff000000, 0xffffffff, 0xff880000, 0xffaaffee,
  0xffcc44cc, 0xff00cc55, 0xff0000aa, 0xffeeee77,
  0xffdd8855, 0xff664400, 0xffff7777, 0xff333333,
  0xff777777, 0xffaaff66, 0xff0088ff, 0xffbbbbbb
};

void convert_to_image(struct cpu_info *cpu){
  XWindowAttributes wa;
  Status wc = XGetWindowAttributes(dis, win, &wa);
  
  int w_scale = wa.width / WIDTH;
  int h_scale = wa.height / HEIGHT;
  
  for(int i = 0x200; i <= 0x5ff; i++){
    uint32_t colour = pallette[read8(cpu->mem,i) & 0x0f];
    int y = (i - 0x200) / HEIGHT;
    int x = (i - 0x200) % WIDTH;

    XSetForeground(dis,gc, colour);
    XFillRectangle(dis, win, gc, x*w_scale, y*h_scale, w_scale, h_scale);
  }
}


int main(int argc, char **argv){
  if(argc < 2) return 0;
  /*
   * The NES maps the rom to 0x8000 - 0xFFFF
   * For the easy NES tutorial, The PC begins at 0x0600, so we load the code there.
   */
  struct cpu_info cpu;
  struct memory * mem = make_flat_2k_mem();
  init_cpu_info(&cpu, mem);
  FILE * file = fopen(argv[1], "r");
  load_file_to_mem(file, &cpu, 0x0600);
  fclose(file);
  cpu.pc = 0x0600;
  cpu.s = 0xFF;


  init_x();
  atexit(close_x);
  XEvent event;		
  KeySym key;	
  char text[255];

	
  int scale = 10;
  XImage *img = NULL;

  //small programs can exit to quickly
  // display properly, so we wait 1 second
  // for the X window to pop up
  XFlush(dis);
  struct timespec wait;
  wait.tv_sec = 1;
  wait.tv_nsec = 0;
  nanosleep(&wait, NULL);
  
  int breakPt = INT32_MAX;
  Bool breaking = False;
  while(1) {		
    long long start = get_timestamp();
    //XNextEvent(dis, &event);
    //A non blocking version of XNextEvent, returns true if event is there.
    Bool isEvent = XCheckMaskEvent(dis, my_event_mask, &event);
    if(cpu.visual_dirty){
      convert_to_image(&cpu);
      cpu.visual_dirty = 0;
    }

    write8(cpu.mem, 0xfe, rand() % 256);
    if(isEvent){
      // handle key events etc
      if (event.type==Expose && event.xexpose.count==0) {
	convert_to_image(&cpu);
      }
      if (event.type==KeyPress&& XLookupString(&event.xkey,text,255,&key,0)==1) {
	/* use the XLookupString routine to convert the invent
	 */
	if (text[0]=='w') {
	  write8(cpu.mem, 0xff, 0x77);
	} else if (text[0]=='s') {
	  write8(cpu.mem, 0xff, 0x73);
	} else if (text[0]=='d') {
	  write8(cpu.mem, 0xff, 0x64);
	} else if (text[0]=='a') {
	  write8(cpu.mem, 0xff, 0x61);
	}

	if (text[0]=='q') {
	  break;
	}

      }
    }
    if(!cpu.finished){
      // here we handle a cycle like normal
      // my poor calculations say we should wait around 600 ns per cycle
      
      step(&cpu);

      if(cpu.pc == breakPt){
	breaking = True;
	printf("==============================\n");
	printf("\nreached break point\n");
	printf("snakedirections %x, snake length %x\n", cpu.mem[0x02], cpu.mem[0x03]);
	printf("==============================\n");
      }

	    
      if(breaking){
	cpu.cycles = 0;
	char c = getchar();
	if(c == 'c'){
	  breaking = False;
	} else if (c == 'q'){
	  return 0;
	}
      }
    }
    long long end = get_timestamp();
    struct timespec t;
    t.tv_sec =0;
    t.tv_nsec = (600) - (end - start);
    nanosleep(&t, NULL);
  }

  return 0;
}
