/*
 * vgaemu.c
 *
 * VGA emulator for dosemu
 *
 * Copyright (C) 1995 1996, Erik Mouw and Arjan Filius
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * email: J.A.K.Mouw@et.tudelft.nl, I.A.Filius@et.tudelft.nl
 *
 *
 * This code emulates a VGA (Video Graphics Array, a video adapter for IBM
 * compatible PC's) for DOSEMU, the Linux DOS emulator. Emulated are the 
 * video memory and the register set (CRTC, DAC, etc.).
 *
 * Lots of VGA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
 *
 *
 * 1996/05/06:
 *   Adam Moss (aspirin@tigerden.com):
 *    - Fixed method 2 of vgaemu_get_changes_in_pages()
 *    - Very simplified vgaemu_get_changes_and_update_XImage_0x13()
 *  Erik Mouw: 
 *    - Split VGAemu in three files
 *    - Some minor bug fixes
 *
 *
 * 1996/05/09:
 *   Adam Moss:
 *    - Changed the way that vgaemu_get_changes() and
 *        vgaemu_get_changes_and_update_XImage_0x13() return an area.
 *    - Minor bug fixes
 *
 * 1996/05/20:
 *   Erik:
 *    - Made VESA modes start to work properly!
 *   Adam:
 *    - Fixed method 0 of vgaemu_get_changes_in_pages()
 *    - Fixed vgaemu_get_changes_in_pages to work faster/better with
 *       SVGA modes
 *
 * DANG_BEGIN_MODULE
 *
 * The VGA emulator for dosemu. Emulated are the video meory and the VGA
 * register set (CRTC, DAC, etc.).
 *
 * DANG_END_MODULE
 *
 */

/*#undef GRAPH
#define GRAPH 2
*/

/*
 * Defines to enable debug information for:
 * DEBUG_IO        -- inb/outb emulation
 * DEBUG_IMAGE     --
 * DEBUG_UPDATE    -- what's updated
 */
#define DEBUG_IO
/* #define DEBUG_IMAGE */
/* #define DEBUG_UPDATE */



/* **************** include files **************** */
#include <sys/mman.h>           /* root@sjoerd*/
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "cpu.h"	/* root@sjoerd: for context structure */
#include "emu.h"
#include "video.h"
#include "vgaemu.h"
#include "vgaemu_inside.h"
#ifdef VESA
#include "vesa.h"
#endif




#if !defined True
#define False 0
#define True 1
#endif




/* **************** General mode data **************** */

/* Table with video mode definitions */
vga_mode_info vga_mode_table[]=
{
  /* The standard CGA/EGA/MCGA/VGA modes */
  {0x00,   TEXT,   360,  400,   9, 16,   40, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x01,   TEXT,   360,  400,   9, 16,   40, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x02,   TEXT,   720,  400,   9, 16,   80, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   720,  400,   9, 16,   80, 25,   16,  0xb8000,  0x8000,  TEXT},

  /* The additional mode 3: 80x21, 80x28, 80x43, 80x50 and 80x60 */
  {0x03,   TEXT,   720,  336,   9, 16,   80, 21,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   720,  448,   9, 16,   80, 28,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   640,  448,   8, 14,   80, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   640,  400,   8,  8,   80, 50,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   640,  480,   8,  8,   80, 60,   16,  0xb8000,  0x8000,  TEXT},

  {0x04,  GRAPH,   320,  200,   8,  8,   40, 25,    4,  0xb8000,  0x8000,   CGA},
  {0x05,  GRAPH,   320,  200,   8,  8,   40, 25,    4,  0xb8000,  0x8000,   CGA},
  {0x06,  GRAPH,   640,  200,   8,  8,   80, 25,    2,  0xb8000,  0x8000,  HERC},
  {0x07,   TEXT,   720,  400,   9, 16,   80, 25,    2,  0xb0000,  0x8000,  TEXT},
  
  /* Forget the PCjr modes (forget the PCjr :-) */
  
  /* Standard EGA/MCGA/VGA modes */
  {0x0d,  GRAPH,   320,  200,   8,  8,   40, 25,   16,  0xa0000, 0x10000,  PL4},
  {0x0e,  GRAPH,   640,  200,   8,  8,   80, 25,   16,  0xa0000, 0x10000,  PL4},
  {0x0f,  GRAPH,   640,  350,   8, 14,   80, 25,    2,  0xa0000, 0x10000,  HERC},
  {0x10,  GRAPH,   640,  350,   8, 14,   80, 25,   16,  0xa0000, 0x10000,   PL4},
  {0x11,  GRAPH,   640,  480,   8, 16,   80, 30,    2,  0xa0000, 0x10000,  HERC},
  {0x12,  GRAPH,   640,  480,   8, 16,   80, 30,   16,  0xa0000, 0x10000,   PL4},
  {0x13,  GRAPH,   320,  200,   8,  8,   40, 25,  256,  0xa0000, 0x10000,    P8},
  
  /* SVGA modes. Maybe we are going to emulate a Trident 8900, so
   * we already use the Trident mode numbers in advance.
   */
   
  {0x50,   TEXT,   640,  480,   8, 16,   80, 30,   16,  0xb8000,  0x8000,  TEXT},
  {0x51,   TEXT,   640,  473,   8, 11,   80, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x52,   TEXT,   640,  480,   8,  8,   80, 60,   16,  0xb8000,  0x8000,  TEXT},
  
  {0x53,   TEXT,  1056,  350,   8, 14,  132, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x54,   TEXT,  1056,  480,   8, 16,  132, 30,   16,  0xb8000,  0x8000,  TEXT},
  {0x55,   TEXT,  1056,  473,   8, 11,  132, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x56,   TEXT,  1056,  480,   8,  8,  132, 60,   16,  0xb8000,  0x8000,  TEXT},
  
  {0x57,   TEXT,  1188,  350,   9, 14,  132, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x58,   TEXT,  1188,  480,   9, 16,  132, 30,   16,  0xb8000,  0x8000,  TEXT},
  {0x59,   TEXT,  1188,  473,   9, 11,  132, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x5a,   TEXT,  1188,  480,   9,  8,  132, 60,   16,  0xb8000,  0x8000,  TEXT},
  
  {0x5b,  GRAPH,   800,  600,   8,  8,  100, 75,   16,  0xa0000, 0x10000,   PL4},
  {0x5c,  GRAPH,   640,  400,   8, 16,   80, 25,  256,  0xa0000, 0x10000,    P8},
  {0x5d,  GRAPH,   640,  480,   8, 16,   80, 30,  256,  0xa0000, 0x10000,    P8},
  {0x5e,  GRAPH,   800,  600,   8,  8,  100, 75,  256,  0xa0000, 0x10000,    P8},
  {0x5f,  GRAPH,  1024,  768,   8, 16,  128, 48,   16,  0xa0000, 0x10000,   PL4},
  {0x60,  GRAPH,  1024,  768,   8, 16,  128, 48,    4,  0xa0000, 0x10000,   CGA}, /* ??? */
  {0x61,  GRAPH,   768, 1024,   8, 16,   96, 64,   16,  0xa0000, 0x10000,   PL4},
  {0x62,  GRAPH,  1024,  768,   8, 16,  128, 48,  256,  0xa0000, 0x10000,    P8},
  {0x63,  GRAPH,  1280, 1024,   8, 16,  160, 64,   16,  0xa0000, 0x10000,   PL4},
  {  -1,     -1,    -1,   -1,  -1, -1,   -1, -1,   -1,       -1,      -1,    -1}
};


/* pointer to the current mode */
static vga_mode_info *current_mode_info=NULL;






/* **************** Video_page dirty registers ************* */
static int vgaemu_graphic_dirty_page[256];
/* { */
/*  True, True, True,True, */
/*  True, True, True,True, */
/*  True, True, True,True, */
/*  True, True, True,True */
/* }; */


static int vgaemu_text_dirty_page[8]=
{
  True, True, True,True,
  True, True, True,True
};



/* *************** The emulated videomemory and some scratch memory *** */

static unsigned char* vga_emu_memory=NULL;
static unsigned char* vga_emu_memory_scratch=NULL;
static int current_bank=0;




/* ****** Own memory to map allocated vga_emu_memory *********/

static int selfmem_fd;





/* **************** VGA emulation routines **************** */

/*
 * DANG_BEGIN_FUNCTION VGA_emulate_outb
 *
 * Emulates writes to VGA ports.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void VGA_emulate_outb(int port, unsigned char value)
{
#ifdef DEBUG_IO
  v_printf("VGAemu: VGA_emulate_outb(): outb(0x%03x, 0x%02x)\n", port, value);
#endif

  switch(port)
    {
    case ATTRIBUTE_INDEX:
      Attr_write_value(value);
      break;
    
    case ATTRIBUTE_DATA:
      /* The attribute controller data port is a read-only register,
       * so don't do anything at all.
       */
#ifdef DEBUG_IO
      v_printf("VGAemu: ERROR: illegal write to the attribute controller "
               "data port!\n");
#endif
      break;
    
    case DAC_PEL_MASK:
      DAC_set_pel_mask(value);
      break;
    
    case DAC_READ_INDEX:
      DAC_set_read_index(value);
      break;

    case DAC_WRITE_INDEX:
      DAC_set_write_index(value);
      break;

    case DAC_DATA:
      DAC_write_value(value);
      break;

    default:
#ifdef DEBUG_IO
      v_printf("VGAemu: not (yet) smart enough to emulate write of 0x%02x to"
	       " port 0x%04x\n", value, port);
#endif
      break;
    }
}




/*
 * DANG_BEGIN_FUNCTION VGA_emulate_inb
 *
 * Emulates reads from VGA ports.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char VGA_emulate_inb(int port)
{
#ifdef DEBUG_IO
  v_printf("VGAemu: VGA_emulate_inb(): inb(0x%03x)\n", port);
#endif

  switch(port)
    {
    case ATTRIBUTE_INDEX:
      return(Attr_get_index());        /* undefined, in fact */
      break;
    
    case ATTRIBUTE_DATA:
      return(Attr_read_value());
      break;
    
    case DAC_PEL_MASK:
      return(DAC_get_pel_mask());
      break;

    case DAC_STATE:
      return(DAC_get_state());
      break;

    case DAC_WRITE_INDEX: /* this is undefined, but we have to do something */
      return(0);
      break;

    case DAC_DATA:
      return(DAC_read_value());
      break;

    case INPUT_STATUS_1:
      return(Attr_get_input_status_1());
      break;

    default:
#ifdef DEBUG_IO
      v_printf("VGAemu: not (yet) smart enough to emulate read from"
	       " port 0x%04x\n", port);
#endif
      return(0); /* do something */
      break;
    }
}




/*
 * DANG_BEGIN_FUNCTION vga_emu_fault(struct sigcontext_struct *scp)        
 *
 * description:
 *  vga_emu_fault() is used to catch video acces, and handle it.
 *  This function is called from dosemu/sigsegv.c:dosemu_fault()
 *  The sigcontext_struct is defined in include/cpu.h
 *  Now it catches only changes in a 4K page, but maybe it is useful to
 *  catch each video acces. The problem when you do that is, you have to 
 *  simulate each intruction which could write to the video-memory.
 *  it is easy to get the place where the exeption happens (scp->cr2),
 *  but what are those changes?
 *  An other problem is, it could eat a lot time, but it does now also.
 *  
 * DANG_END_FUNCTION                        
 */     

int vga_emu_fault(struct sigcontext_struct *scp)
{
  int page_fault_number=0;
  int dirty_page_number=0;

/*printf("exeption adr: 0x%0lx\n",scp->cr2);  
*/
  page_fault_number=(scp->cr2-0xA0000)/(4*1024);
  dirty_page_number=page_fault_number + current_bank*16;

#ifdef DEBUG_UPDATE
  v_printf("VGAemu: vga_emu_fault(): address=0x%0lx, page=%i, dirty=%i\n",
	   scp->cr2, page_fault_number, dirty_page_number);
#endif

  if( (page_fault_number >=0) && (page_fault_number <16) )
    {
      vgaemu_graphic_dirty_page[dirty_page_number]=True;
      /*      printf("(%d)\t",dirty_page_number);*/
      mprotect((void *)(0xA0000+page_fault_number*0x1000),0x1000,
	       PROT_READ|PROT_WRITE);

      return True;
    } 
  else /* Exeption not in the vga_emu_ram, maybe in the text mode ram */
    {
      page_fault_number=(scp->cr2-0xB8000)/(4*1024);
      if( (page_fault_number >=0) && (page_fault_number <8) )
	{
	  vgaemu_text_dirty_page[page_fault_number]=True;
	  mprotect((void *)(0xB8000+page_fault_number*0x1000),0x1000,
		   PROT_READ|PROT_WRITE);
	  
	  return True;
	}
      else /* Exeption not in the vga_emu_ram, maybe in the vga_emu_rom*/
	if(vesa_emu_fault(scp)==True)
	  return True;
	else 
	  v_printf("VGAemu: vga_emu_fault: Not in 0xB8000-0xC0000range\n"
	           "page: 0x%02x  adress: 0x%lx \n",
	           page_fault_number,scp->cr2); 
    }
  return False;
}
 
static inline caddr_t vga_mmap(caddr_t  addr,  size_t  len,
                        int prot , int flags, int fd, off_t offset )
{
  int i;
  /* Touch all pages before mmap()-ing,
   * else Linux >= 1.3.78 will return -EINVAL. (Hans, 96/04/16)
   */
  for (i=0; i < len; i+=4096) *((volatile char *)(offset+i));
  return mmap(addr, len, prot, flags, fd, offset);
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_init(void)        
 *
 * description:
 *  vga_emu_init() is used to emulate video.
 *  This function is only called from video/X.c at the moment.
 *  This function has to set a global variable to detect it in other functions
 *  it has to map the right video-bank to the 0xA0000 adress.
 *
 * DANG_END_FUNCTION                        
 *
 * I don't think it is wise to return a pointer to the video memory...
 * I'd like to program object oriented, data hiding, etc. -- Erik
 *
 */     
unsigned char* vga_emu_init(void)
{
  vga_emu_memory=(unsigned char*)valloc(VGAEMU_BANK_SIZE*VGAEMU_BANKS);
  if(vga_emu_memory==NULL)
    v_printf("VGAemu: vga_emu_init: Allocated memory is NULL\n");

  vga_emu_memory_scratch=(unsigned char*)valloc(VGAEMU_BANK_SIZE*VGAEMU_BANKS);
  if(vga_emu_memory_scratch==NULL)
    v_printf("VGAemu: vga_emu_init: Allocated memory is NULL\n");

  /* mapping the bank of the allocated memory */
  selfmem_fd = open("/proc/self/mem", O_RDWR);
  if (selfmem_fd < 0)
    v_printf("VGAemu: vga_emu_init: cannot open /proc/self/mem:\n");

  *vga_emu_memory=0;	/* touch it, or it can't be mapped */
  *vga_emu_memory_scratch=0; /* idem dito */
 

  if(vga_mmap((caddr_t)0xA0000, VGAEMU_BANK_SIZE, PROT_READ|PROT_WRITE,
          MAP_SHARED | MAP_FIXED,selfmem_fd,(off_t)vga_emu_memory )<0)
    v_printf("VGAemu: vga_emu_init: Mapping failed\n");

  mprotect((void *)0xA0000,0xFFFF,PROT_READ);

  /* initialize the dirty page flags */
  dirty_all_video_pages();
  current_bank=0;

/* add here something like for text-mode */

  DAC_init();
  Attr_init();
#ifdef VESA
  vesa_init();
#endif

  return vga_emu_memory;
}



 
/*
 * DANG_BEGIN_FUNCTION int vgaemu_get_changes_in_pages
 *
 * description:
 *  vgaemu_get_changes_in_pages() is vgaemu_get_changes() is used 
 *  to get the changed 4K pages .
 *  This function is only called from video/vgaemu.c .
 *  It has to called several times to make sure grabbing all the changed
 *  pages.
 *
 * should be updated for other video modes than 0x13
 *
 * DANG_END_FUNCTION                        
 */     

int vgaemu_get_changes_in_pages(int method,int *first_dirty_page,int *last_dirty_page)
{
  int low, high, i, highest;

  switch(current_mode_info->type)
    { 
    case GRAPH:

      /* FIXME: This will need to be revised for non-8bpp or weird modes */
      highest = (current_mode_info->x_res * current_mode_info->y_res)/0x1000;

      /*      printf("[H:%d]\t",highest);*/
      
      low=high=highest+1;

      /*      for (i=0;i<256;i++) printf("%d:%d\t",i,vgaemu_graphic_dirty_page[i]);
      printf("\n-----------------------------\n");*/

      /* Find the first dirty one */
      for(i=0;i<(highest+1);i++) /* now split in more areas */
	{
	  if(vgaemu_graphic_dirty_page[i]==True)
	    {
	      if(i<16)
		mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
	      
	      vgaemu_graphic_dirty_page[i]=False;
	      low=i;
	      break;
	    }   
	}
      
      /* No dirty pages */
      if(low==(highest+1)) 
	{ 
	  /*mprotect((void *)0xA0000,0xFFFF,PROT_READ);*/	/* protect whole area */
	  return False;
	}
      
      switch(method)
	{
	case 0:			/* find all dirty pages which are connected */
	  for(i=high=low+1;i<(highest+1);i++)
	    {
	      if(vgaemu_graphic_dirty_page[i]==True)
		{
		  if(i<16)
		    mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
		  
		  vgaemu_graphic_dirty_page[i]=False;
		  high=i+1;
		}   
	      else
		{
		  break;
		}
	    }
	  break;
	  
	case 1:		/* Get only the first dirty page */
	  high=low+1;;
	  break;
	  
	case 2:		/* Get first and last page */
          high=low+1;
          for(i=highest;i>=low;i--)
            {
	      if(vgaemu_graphic_dirty_page[i]==True)
		{
		  if(i<16)
		    mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
		  
		  vgaemu_graphic_dirty_page[i]=False;
		  high=i+1;
		  break;
		}
	    }
	  for(;i>low;i--)	/* clear dirty flags and protect it */
	    {
	      if(i<16)
		mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
	      
	      vgaemu_graphic_dirty_page[i]=False;
	    }    
	  break;
	  
	default:
	  v_printf("VGAemu: vgaemu_get_changes_in_pages: No such method\n");
	  break;
	}
 
      *first_dirty_page=low;
      *last_dirty_page=high;  
      
      return True;	/* False= nothing has changed */
      break;
      
    case TEXT:
      low=8;
      high=8;
      
      /* Find the first dirty one */
      for(i=0;i<8;i++) /* now split in more areas */
	{
	  if(vgaemu_text_dirty_page[i]==True)
	    {
	      mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
	      vgaemu_text_dirty_page[i]=False;
	      low=i;
	      break;
	    }   
	}
      
      /* No dirty pages */
      if(low==8) 
	{ 
	  /*mprotect((void *)0xA0000,0xFFFF,PROT_READ);*/	/* protect whole area */
	  return False;
	}
      
      switch(method)
	{
	case 0:			/* find all dirty pages, which are connected */
	  for(++i;i<8;i++)
	    {
	      if(vgaemu_text_dirty_page[i]==True)
		{
		  mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
		  vgaemu_text_dirty_page[i]=False;
		}   
	      else
		{
		  high=i--;
		  break;
		}
	    }
	  break;
	  
	case 1:		/* Get only the first dirty page */
	  high=low+1;;
	  break;
	  
	case 2:		/* Get first and last page */
	  for(i=7;i>0;i--)
	    {
	      if(vgaemu_text_dirty_page[i]==True)
		{
		  mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
		  vgaemu_text_dirty_page[i]=False;
		  break;
		}
	    }
	  for(;i>low;i--)	/* clear dirty flags and protect it */
	    {
	      mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
	      vgaemu_text_dirty_page[i]=False;
	    }    
	  break;
	  
	default:
	  v_printf("VGAemu: vgaemu_get_changes_in_pages: No such method\n");
	  break;
	}
      
      *first_dirty_page=low;
      *last_dirty_page=high;  
      
      return True;	/* False= nothing has changed */
      break;

    default:
      break;
    }
  
  return True;	/* False= nothing has changed */
}




/*
 * DANG_BEGIN_FUNCTION vgaemu_get_changes_and_update_XImage_0x13
 *
 * description:
 *  vgaemu_get_changes() is used to get the changed area and update the image.
 *  This function is only called from video/X.c at the moment.
 *  It has to called several times to make sure grabbing all the changed
 *  areas.
 *
 * This is only for mode 0x13: 256 colors
 *
 * DANG_END_FUNCTION                        
 */
int vgaemu_get_changes_and_update_XImage_0x13(unsigned char **base, unsigned long int *offset, unsigned long int *len, int method, int *modewidth)
{
  int first_dirty_page,last_dirty_page;

#ifdef DEBUG_IMAGE
  v_printf("VGAemu: vgaemu_get_changes_and_update_XImage: Ximage is:\n"
	 "width = %d, height = %d\n"
	 "depth = %d, b/l = %d, b/p = %d\n"
	 "bitmap_pad = %d\n"
	 "*data= %p\n",
	 image->width,image->height,image->depth,
	 image->bytes_per_line,image->bits_per_pixel,image->bitmap_pad,image->data);       
#endif
  
  if(vgaemu_get_changes_in_pages(VGAEMU_UPDATE_METHOD_G_C_IN_PAGES,
				 &first_dirty_page,&last_dirty_page)==True)
    {
      /*      *xx=0;
      *ww=current_mode_info->x_res;
      *yy=(first_dirty_page*0x1000)/current_mode_info->x_res;
      *hh=((last_dirty_page-first_dirty_page)*0x1000+current_mode_info->x_res-1)/current_mode_info->x_res;
      
      if((*yy+*hh)>current_mode_info->y_res )
	*hh=current_mode_info->y_res-*yy; */
      
      /* Do a really quick couldn't-care-less-which-pixels-changed copy of
	 the dirty pages into the (Shm)XImage */
      /*      memcpy(
	     (unsigned char*)&data[(*yy)*(current_mode_info->x_res)],
	     (unsigned char*)(current_mode_info->bufferstart+(current_mode_info->x_res*(*yy))),
	     (*hh)*(current_mode_info->x_res)
	     );*/

#ifdef DEBUG_IMAGE
      v_printf("VGAemu: dirty pages from %i to %i\n",
	       first_dirty_page, last_dirty_page);
#endif

      *modewidth = current_mode_info->x_res;
      *base = (unsigned char*) vga_emu_memory;
      *offset = first_dirty_page*0x1000;
      *len = (unsigned long int)((last_dirty_page-first_dirty_page) * 0x1000);

      if(*offset + *len > current_mode_info->x_res*current_mode_info->y_res)
	*len=current_mode_info->x_res*current_mode_info->y_res - *offset;

/*
      if (
	  (*offset + *len > current_mode_info->bufferlen) ||
	  (*offset + *len > current_mode_info->x_res*current_mode_info->y_res)
	  )
	{
	  if (current_mode_info->bufferlen < current_mode_info->x_res*current_mode_info->y_res) *len = current_mode_info->bufferlen - *offset;
	  else *len = current_mode_info->x_res*current_mode_info->y_res - *offset;
	}*/

      return True;    
    }
  else
    return False;
  
  return False;	/* False= nothing has changed */
}



/*
 * DANG_BEGIN_FUNCTION vga_emu_switch_page(unsigned int pagenumber)        
 *
 * description:
 *  vga_emu_switch_page() is used to emulate video-bankswitching.
 *  This function isn't called anywhere, but has to be used, with
 *  other videomodes.
 *  This function just remaps his 'own' memory into the 0xA000-0xB0000
 *  area and returns True on succes and False on error.
 *
 *  At the moment just a stupid function, but it is a start.
 *  Jou must be sure, you've got all changes before you switch a bank!
 *
 * DANG_END_FUNCTION                        
 */     
int vgaemu_switch_page(unsigned int pagenumber)
{
  /* remapping the one bank of the allocated memmory */

 /* Is this < or <= ? -- Erik */
  if((pagenumber<=VGAEMU_BANKS) && (pagenumber>=0))
    {
      current_bank=pagenumber;

      if(vga_mmap((caddr_t)0xA0000, VGAEMU_BANK_SIZE, PROT_READ|PROT_WRITE,
	      MAP_SHARED | MAP_FIXED,selfmem_fd,
	      (off_t)(vga_emu_memory+current_bank*VGAEMU_BANK_SIZE) )<0)
	{
	  v_printf("VGAemu: vga_emu_switch_page: Remapping failed\n");
	  return False;
	}
    }
  else
    {
      v_printf("VGAemu: vga_emu_switch_page(): Invalid page number %i\n",
               pagenumber);
    }
  
  mprotect((void *)0xA0000,0xFFFF,PROT_READ); /* should not be needed, but... */
  return True;
}


/*
 * Only used by set_vgaemu_mode
 */

inline static unsigned mode_area(unsigned mode_index)
{
  return vga_mode_table[mode_index].x_char *
         vga_mode_table[mode_index].y_char;
}

int set_vgaemu_mode(int mode, int width, int height)
{
  int index=0;
  int i;
  int found=False;
  
  /* unprotect vgaemu memory, maybe should also be executable ???? */
  mprotect((void*)current_mode_info->bufferstart,current_mode_info->bufferlen,PROT_READ|PROT_WRITE);
  
  /* Search for the first valid mode */
  for(i=0; (vga_mode_table[i].mode!=-1) && (found==False); i++)
    {
      if(vga_mode_table[i].mode==(mode&0x7F))
        {
          if(vga_mode_table[i].type==TEXT)
            {
              /* TEXT modes can use different char boxes, like the
               * mode 0x03 (80x25). Mode 0x03 uses a 9x16 char box
               * for 80x25 and a 8x8 charbox for 80x50
               */
              if( (vga_mode_table[i].x_char==width) &&
                  (vga_mode_table[i].y_char==height) )
                {
                  found=True;
                  index=i;
                }
            }
          else /* type==GRAPH */
            {
              /* GRAPH modes use only one format */
              found=True;
              index=i;
            }
        }
    }

  if(found==True)
    v_printf("VGAemu: set_vgaemu_mode(): mode found in first run!\n");


  /* Play it again, Sam!
   * This is when we can't find the textmode with the appropriate sizes.
   * Use the best matching text mode
   */
  if(found==False)
    {
      for(i=0; (vga_mode_table[i].mode!=-1); i++)	
        {
          if(vga_mode_table[i].mode==(mode&0x7f) &&
	     /* make sure everything is visible! */
             (vga_mode_table[i].x_char>=width) &&
             (vga_mode_table[i].y_char>=height) )
            {
	      if ((found == True) && (mode_area(i) >= mode_area(index))) 
		{
		  continue;
		}
	      else 
		{
		  found=True;
		  index=i;
		}
            }
        }
        
      if(found==True)
        v_printf("VGAemu: set_vgaemu_mode(): mode found in second run!\n");
    }
    
    
  v_printf("VGAemu: set_vgaemu_mode(): mode=0x%02x, (%ix%i, %ix%i, %ix%i)\n",
         vga_mode_table[index].mode,
         vga_mode_table[index].x_res,
         vga_mode_table[index].y_res,
         vga_mode_table[index].x_char,
         vga_mode_table[index].y_char,
         vga_mode_table[index].x_box,
         vga_mode_table[index].y_box);
         
  if(found==True)
    {
      current_mode_info=&vga_mode_table[index];
  
      /* Some applications expect memory to be initialised to
       all-zero after a BIOS mode-set... Erik says it only applies
       to modes below 0x80. --adm */

      /* Does the BIOS clear all of video memory, or just enough
       for the viewport?  This clears all of it... */


      /* Actually if the bios has any sense it clears fills the screen
       * with spaces (Background Black, Foreground White).  This
       * happens to be the same as all zeros in graphics mode.
       * I've patched the bios end of it to handle the text mode case
       * so don't do that here.  There are no real video modes >= 0x80
       * so test the incoming mode number instead of the name after
       * the stripping.
       */
      /* FIXME: the interface between the bios setmode and this
	 setmode are weird!! */

      if (vga_mode_table[index].type != TEXT && mode < 0x80)
	memset((void*)vga_emu_memory, 0, 
	       (size_t)VGAEMU_BANK_SIZE*VGAEMU_BANKS);

      current_bank=0;

      /* map first page */
      vga_mmap((caddr_t)current_mode_info->bufferstart, 
	       current_mode_info->bufferlen,
	       PROT_READ|PROT_WRITE,
	       MAP_SHARED|MAP_FIXED,
	       selfmem_fd,
	       (off_t)vga_emu_memory);

      /*protect the vgaemu memory, so it is possible to trap everything */
      /* PROT_READ defined in /usr/include/asm/mman.h  root@sjoerd*/
      mprotect((void*)current_mode_info->bufferstart,current_mode_info->bufferlen,PROT_READ);
      /* I think we have to write something to the bios area */

      /* Re-initialize the DAC */
      DAC_init();

      return(True);
    }
  else
    return(False);
}


int get_vgaemu_width(void)
{
  /* printf("get_vgaemu_width= %d\n",current_mode_info->x_res);
     return 320; */
  return current_mode_info->x_res;
}

int get_vgaemu_heigth(void)
{
  /* printf("get_vgaemu_heigth= %d\n",current_mode_info->y_res);
     return 200;*/
  return current_mode_info->y_res;
}


void print_vgaemu_mode(int mode)
{
  /* int mode;*/
  for(mode=0;(vga_mode_table[mode]).mode!=-1;mode++)
    v_printf("VGAemu: mode = 0x%02x  mode = 0x%02x type = %d w = %d h = %d\n",mode,
	   (vga_mode_table[mode]).mode,
	   (vga_mode_table[mode]).type,
	   
	   (vga_mode_table[mode]).x_res,
	   (vga_mode_table[mode]).y_res);
}




/*
 * DANG_BEGIN_FUNCTION get_vga_mode_info
 *
 * Returns a pointer to the vga_mode_info structure for the
 * requested mode or NULL if an invalid mode was given.
 *
 * DANG_END_FUNCTION
 */
vga_mode_info* get_vgaemu_mode_info(int mode)
{
  int i;

  for(i=0; 1; i++)	
    {
      if(vga_mode_table[i].mode==mode)
	  return(&vga_mode_table[i]);
	
      if(vga_mode_table[i].mode==-1)
	  return NULL;
    }
  
  /* catch all */
  return NULL;
}



int get_vgaemu_tekens_x(void)
{
  return current_mode_info->x_char;
}

int get_vgaemu_tekens_y(void)
{
  return current_mode_info->y_char;
}

int get_vgaemu_type(void)
{
  return current_mode_info->type;
}

int vgaemu_update(unsigned char **base, unsigned long int *offset, unsigned long int *len, int method, int *modewidth)
{

/*  if(vgaemu_info->update!=NULL)
    return vgaemu_info->update(image, method,x, y,width,heigth);
  else*/ 
  
  switch(current_mode_info->memorymodel)
    {
    case P8:
      return(vgaemu_get_changes_and_update_XImage_0x13(base, offset, len, method, modewidth));
      break;
    
    default:
      v_printf("VGAemu: vgaemu_update(): No update function for memory model 0x%02x\n", 
             current_mode_info->memorymodel);
      return False;
      break;
    }
}
     

void dirty_all_video_pages(void)
{
  int i;
  for (i=0;i<256;i++) vgaemu_graphic_dirty_page[i]=True;
}

     
int set_vgaemu_page(unsigned int page)
{
  v_printf("VGAemu: set_vgaemu_page %d\n",page);
/*  if(page<=current_mode_info->pages)
    {
      vgaemu_info->active_page=page;
 */     
      /* some bankswitching to do */
   /*   
      
      return True;
    }*/
  return False;
}     
