/*
 * vesa.c
 *
 * VESA BIOS enhancements for the Linux dosemu VGA emulator (vgaemu)
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
 * The VESA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * VESA BIOS enhancements for vgaemu.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 */

int set_video_mode(int mode);  /* root@sjoerd from int10.c,dangerous*/


/* **************** Definitions **************** */

/*
 * Defines to enable debug information for:
 * DEBUG               -- General debug info
 * DEBUG_SUBFUNCTION   -- VESA subfunctions
 * DEBUG_TRAP          -- Exceptions on video BIOS
 */
/* #define DEBUG */
#define DEBUG_SUBFUNCTION
/* #define DEBUG_TRAP */

#if !defined False
#define False 0
#define True 1
#endif




/* **************** Include files **************** */
#include <features.h>
#if __GLIBC__ > 1
#include <sigcontext.h>
#endif
#include <sys/mman.h>
#include "cpu.h"        /* root@sjoerd: for context structure */

#include "emu.h"
#include "video.h"
#include "vgaemu.h"

#ifdef NEW_X_CODE          
/* to be removed soon -- sw */
#define VGAEMU_BANK_SIZE 0x10000
#define VGAEMU_GRANULARITY     0x10000   /* granularity-size */
#else
  #define vga_emu_setmode set_vgaemu_mode
  #define vga_emu_switch_bank vgaemu_switch_page
#endif

/*extern struct vga_info vga_mode_info[];
*/
/* **************** Data **************** */

/* VESA compatible BIOS is in vesabios.S */
/* WARNING: The following two functions are no real functions, they're
 * pointers to the DOS part of the VESA BIOS!
 */
extern void vgaemu_VESA_BIOS_label_start();
extern void vgaemu_VESA_BIOS_label_end();

unsigned char *vgaemu_VESA_BIOS=(unsigned char *)vgaemu_VESA_BIOS_label_start;




/* Structure to translate VESA modes to (own) OEM modes */
typedef struct
{
  int vesamode;
  int oemmode;
} mode_trans;




/* Translation table from VESA to (own) OEM modes */
const mode_trans mode_translate_table[]=
{
  { 0x100, 0x5c },	/*  640x400x256 */
  { 0x101, 0x5d },	/*  640x480x256 */
  { 0x103, 0x5e },	/*  800x600x256 */
  { 0x105, 0x62 },	/* 1024x768x256 */
  { 0x108, 0x52 },	/*        80x60 */
  { 0x109, 0x53 },	/*       132x25 */
  { 0x10a, 0x55 },	/*       132x43 */
  { 0x10c, 0x56 },	/*       132x60 */
  {    -1,   -1 }	/* end of table */
};




/* **************** Function prototypes **************** */

int vesa_translate_mode(int mode);
int vesa_get_SVGA_info(void);		/* vesa function 0 */
int vesa_get_SVGA_mode_info(void);	/* vesa function 1 */
int vesa_set_SVGA_mode(void);		/* vesa function 2 */
int vesa_SVGA_memory_control(void);	/* vesa function 5 */




/* **************** Function implementations **************** */

/* DANG_BEGIN_FUNCTION vesa_init
 *
 * Initializes the VESA emulator, i.e. sets up the VESA BIOS.
 *
 * DANG_END_FUNCTION
 */
 void vesa_init(void)
 {
   int i;
   unsigned char* dos_VGA_BIOS=(unsigned char *)0xc0000;
   
   for(i=0; i<512; i++)
     dos_VGA_BIOS[i]=vgaemu_VESA_BIOS[i];
     
   /* make it readonly ! (it is a ROM), this will need an exeption_handler:
    * some dirty programs try to write to the ROM ! */  
   mprotect(dos_VGA_BIOS, VGAEMU_ROM_SIZE ,PROT_READ );  /* only 4 K at the moment */

 }
 
 
/* 
 * DANG_BEGIN_FUNCTION vesa_emu_fault(struct sigcontext_struct *scp)
 *
 * description:
 *  vesa_emu_fault() is used to handle video ROM acces.
 *  This function is called from ./video/vgaemu.c:vga_emu_fault().
 *  The sigcontext_struct is defined in include/cpu.h
 *  It just jumps over the intruction (LWORD (eip)+=instr-len)
 *  which caused the write exeption to the video ROM. It is needed for 
 *  some dirty programs that try to write to a ROM (dos=high,umb seems 
 *  to do this, but not on all PC's) We're sure now, nobody can write to 
 *  the ROM and we don't crash on it, just ignore as it should be!
 *
 * DANG_END_FUNCTION
 *
 * We'll remove the v_printf() statements in the future.
 *
 */
int vesa_emu_fault(struct sigcontext_struct *scp)
{
  unsigned char *csp;
  int page_fault_number=0;
  page_fault_number=(scp->cr2-0xC0000)/(VGAEMU_ROM_SIZE);
  if( (page_fault_number >=0) && (page_fault_number <1) )	/* only one page */
    {
#ifdef DEBUG_TRAP
      v_printf("vesa: vesa_emu_fault():\n"
	       "  trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	       "  eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	       "  cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
	       _trapno, scp->err, scp->cr2,
	       _eip, _esp, _eflags, _cs, _ds, _es, _ss);
     
      v_printf("Writing to ROM:");
#endif /* DEBUG_TRAP */
      csp = SEG_ADR ((unsigned char *), cs, ip);

      switch(csp[0])  /* catch prefix */
        {
        case 0x26 :
#ifdef DEBUG_TRAP
          v_printf("prefix 0x%x ",csp[0]);
#endif
          csp++;
          break;
	  
        case 0xf3 :
#ifdef DEBUG_TRAP
          v_printf("REP(E): ");
#endif
          csp++;
          break;

	default:
	  break;
        }

      switch(csp[0])
        {
        case 0x6c :
#ifdef DEBUG_TRAP
          v_printf("insb,     Yb, indirDX\n");
#endif
          break;
	  
        case 0x88 :
#ifdef DEBUG_TRAP
          v_printf("movb,     Eb, Gb \n");
#endif
          LWORD (eip)++;
          break;
	  
        case 0x89 :
#ifdef DEBUG_TRAP
          v_printf("movS,     Ev, Gv \n");
#endif
          LWORD (eip)++;
          break;

        case 0x8a :
#ifdef DEBUG_TRAP
          v_printf("movb,     Gb, Eb \n");
#endif
          break;

        case 0x8b :
#ifdef DEBUG_TRAP
          v_printf("movS,     Gv, Ev \n");
#endif
          LWORD (eip)++;
          break;

        case 0x8c :
#ifdef DEBUG_TRAP
          v_printf("movw,     Ew, Sw \n");
#endif
          break;

        case 0x8d :
#ifdef DEBUG_TRAP
          v_printf("leaS,     Gv, M \n");
#endif
          break;

        case 0x8e :
#ifdef DEBUG_TRAP
          v_printf("movw,     Sw, Ew \n");
#endif
          break;

        case 0x8f :
#ifdef DEBUG_TRAP
          v_printf("popS,     Ev \n");
#endif
          break;

        case 0xa4 :
#ifdef DEBUG_TRAP
          v_printf("movsb,    Yb, Xb \n");
#endif
          LWORD (eip)++;
          break;

        case 0xa5 :
#ifdef DEBUG_TRAP
          v_printf("movsS,    Yv, Xv \n");
#endif
          LWORD (eip)++;
          break;

        case 0xaa :
#ifdef DEBUG_TRAP
          v_printf("stosb,    Yb, AL \n");
#endif
          LWORD (eip)++;
          break;

        case 0xab :
#ifdef DEBUG_TRAP
          v_printf("stosS,    Yv, eAX \n");
#endif
          LWORD (eip)++;
          break;

        case 0xac :
#ifdef DEBUG_TRAP
          v_printf("lodsb,    AL, Xb \n");
#endif
          break;

        case 0xad :
#ifdef DEBUG_TRAP
          v_printf("lodsS,    eAX, Xv \n");
#endif
          break;

        case 0xae :
#ifdef DEBUG_TRAP
          v_printf("scasb,    AL, Yb \n");
#endif
          break;

        case 0xaf :
#ifdef DEBUG_TRAP
          v_printf("scasS,    eAX, Yv \n");
#endif
          break;

        case 0xc6 :
#ifdef DEBUG_TRAP
          v_printf("movb,     Eb, Ib \n");
#endif
          LWORD (eip)++;
          break;

        case 0xc7 :
#ifdef DEBUG_TRAP
          v_printf("movS,     Ev, Iv \n");
#endif
          LWORD (eip)++;

          break;

        default :
          v_printf("vesa: vesa_emu_fault(): unknown instruction, "
		   "csp= 0x%02x 0x%02x 0x%02x 0x%02x\n",
		   csp[0],csp[1],csp[2],csp[3]);
          break;
        }
     
      return True;
    }
  else
    v_printf("vesa: vesa_emu_fault(): Not in ROM range\n page= 0x%02x"
	     " adress: 0x%lx\n",page_fault_number,scp->cr2);
  
  return False;
}




/*
 * DANG_BEGIN_FUNCTION vesa_translate_mode
 *
 * Translates a VESA mode number to an (own) OEM mode number using
 * the mode_translate_table. Returns the OEM mode if succesful or
 * -1 otherwise.
 *
 * DANG_END_FUNCTION
 */
int vesa_translate_mode(int mode)
{
  int i;

  for(i=0; 1; i++)
    {
      if(mode_translate_table[i].vesamode==mode)
        return(mode_translate_table[i].oemmode);
      
      if(mode_translate_table[i].vesamode==-1)
        return(-1);
    }
    
  /* catch all */
  return(-1);
}




/*
 * DANG_BEGIN_FUNCTION do_vesa_int
 *
 * This is the VESA interrupt handler. It is called from int10.c: int10().
 * The VESA interrupt is called with 0x4f in AH and the function number
 * in AL.
 *
 * DANG_END_FUNCTION
 */
void do_vesa_int(void)
{
  int rv=False;
  
#ifdef DEBUG
  v_printf("vesa: do_vesa_int(): ax=0x%04x bx=0x%04x cx=0x%04x\n", 
	   LWORD(eax),LWORD(ebx),LWORD(ecx));
#endif

  if(HI(ax)!=0x4f)
    {
      v_printf("vesa: do_vesa_int(): called with ah!=0x4f\n");
      return;
    }

  switch(LO(ax))
    {
    case 0x00:	/* INT 10 - VESA SuperVGA BIOS - GET SuperVGA INFORMATION */
      rv=vesa_get_SVGA_info();
      break;
      
    case 0x01: /* INT 10 - VESA SuperVGA BIOS - GET SuperVGA MODE INFORMATION */
      rv=vesa_get_SVGA_mode_info();
      break;  
      
    case 0x02:	/* INT 10 - VESA SuperVGA BIOS - SET SuperVGA VIDEO MODE */
      rv=vesa_set_SVGA_mode();	
      break;

    case 0x03:	/* INT 10 - VESA SuperVGA BIOS - GET CURRENT VIDEO MODE */
      break;
      
    case 0x04:	/* INT 10 - VESA SuperVGA BIOS - SAVE/RESTORE SuperVGA VIDEO STATE */    
      break;
      
    case 0x05:	/* INT 10 - VESA SuperVGA BIOS - CPU VIDEO MEMORY CONTROL */
      rv=vesa_SVGA_memory_control();
      break;
             
    case 0x06:	/* INT 10 - VESA SuperVGA BIOS 1.1 - GET/SET LOGICAL SCAN LINE LENGTH */
      break;
             
    case 0x07:	/* INT 10 - VESA SuperVGA BIOS 1.1 - GET/SET DISPLAY START */
      break;
             
    case 0x08:	/* INT 10 - VESA SuperVGA BIOS v1.2+ - GET/SET DAC PALETTE CONTROL*/
      break;
             
    case 0xFF:	/* INT 10 - VESA SuperVGA BIOS - Everex - TURN VESA ON/OFF */
      /* I'm not sure if we have to support this. Maybe it is a handy 
       * function, on the other hand, why should you use it? Could anyone
       * tell me why Everex implemented this function? 
       */
      break;
             
    default:
      rv=False;    /* Function not supported */
      break;
    }
    
#ifdef DEBUG
  v_printf("vesa: do_vesa_int(): subfunction returned 0x%02x\n", rv);
#endif
  if(rv==True)
    LWORD(eax)=0x004f;
  else /* rv==False */
    LWORD(eax)=0x014f;
}


/*
 * DANG_BEGIN_FUNCTION vesa_get_SVGA_info
 *
 * Fills out a table with SuperVGA information. AH=0 if succesfull.
 *
 * DANG_END_FUNCTION
 */
int vesa_get_SVGA_info(void)
{
  unsigned char* table;
  unsigned int* word_p;
  unsigned long int* dword_p;
  
#ifdef DEBUG_SUBFUNCTION
  v_printf("vesa: vesa_get_SVGA_info(): Filling buffer at 0x%04x:0x%04x\n",
	   LWORD(es), LWORD(edi));
#endif

  table=SEG_ADR((unsigned char*), es, di);
    
  /* signature */
  table[0]='V';
  table[1]='E';
  table[2]='S';
  table[3]='A';
  
  /* VESA version number */
  word_p=(unsigned int*)(table+0x04);
  *word_p=VESA_VERSION;

  /* pointer to OEM name */
  dword_p=(unsigned long int*)(table+0x06);
  *dword_p=0xc0000000+VGAEMU_BIOS_VESASTRING;
  
  /* capabilities */
  table[0x0a]=0x00;		/* Only the first bit is defined (sigh) */
  table[0x0b]=0x00;
  table[0x0c]=0x00;
  table[0x0d]=0x00;
  
  /* pointer to list of supported VESA and OEM video modes terminated
   * with 0xffff
   */
  dword_p=(unsigned long int*)(table+0x0e);
  *dword_p=0xc0000000+VGAEMU_BIOS_VESA_MODELIST;
 
  /* amount of video memory in 64K blocks */
  word_p=(unsigned int*)(table+0x12);
#ifdef NEW_X_CODE          
  *word_p=(vga.mem.pages >> 4);			/* VGA_EMU_BANKS */
#else
  *word_p=VGAEMU_BANKS;			/* VGA_EMU_BANKS * 64= ? Kb */
#endif
 
  /* All OK */
  LWORD(eax)=0x004f;
 
  return(True);
}


/*
 * DANG_BEGIN_FUNCTION vesa_get_SVGA_mode_info
 *
 * Fills out a table with SuperVGA information. AH=0 if succesfull.
 *
 * DANG_END_FUNCTION
 */
int vesa_get_SVGA_mode_info(void )
{
  unsigned char* table;
  unsigned int* word_p;
  unsigned long int* dword_p;
  int mode;
  vga_mode_info* modeinfo;
  
#ifdef DEBUG_SUBFUNCTION
  v_printf("vesa: vesa_get_SVGA_info(): Filling buffer at 0x%04x:0x%04x\n",
	   LWORD(es), LWORD(edi));
#endif

  /* get the requested mode. translate if neccessary */
  mode=LWORD(ecx);
  if(mode>=0x100)
    mode=vesa_translate_mode(mode);
    
  if(mode<0)
    {
      LWORD(eax)=0x014f;	/* function failed */
      return(False);
    }
  
  /* get a pointer to the vga_mode_info for the particular mode */
  modeinfo=get_vgaemu_mode_info(mode);
  if(modeinfo==NULL)
    {
      LWORD(eax)=0x014f;	/* function failed */
      return(False);
    }
  
  /* construct pointer to the user data table */
  table=SEG_ADR((unsigned char*), es, di);
  word_p=(unsigned int*)table;
      

  /* mode information:
   *   bit 0: mode supported
   *   bit 1: optional information available
   *   bit 2: BIOS output supported
   *   bit 3: set if color, clear if monochrome
   *   bit 4: set if graphics mode, clear if text mode
   */
  *word_p=0x0007;
  if(modeinfo->colors>2)
    *word_p|=0x0008;
    
  if(modeinfo->type==GRAPH)
    *word_p|=0x0010;

  /* window A attributes:
   *   bit 0: exists
   *   bit 1: readable
   *   bit 2: writable
   *   bits 3-7 reserved
   */
  table[2]=0x07;
  
  /* window B attributes (as for window A) */
  /* window B doesn't exist */
  table[3]=0x00;
  
  /* window granularity in K */
  word_p=(unsigned int*)(table+0x04);
  *word_p=VGAEMU_GRANULARITY/1024;
  
  /* window size in K */
  word_p=(unsigned int*)(table+0x06);
  *word_p=VGAEMU_BANK_SIZE/1024;
  /* FIX THIS: this is not true for all modes! */
  
  /* start segment of window A */
  word_p=(unsigned int*)(table+0x08);
  *word_p=(unsigned int)modeinfo->bufferstart/16;
  
  /* start segment of window B */
  word_p=(unsigned int*)(table+0x0a);
  *word_p=(unsigned int)modeinfo->bufferstart/16;
  
  /* FAR pointer to window positioning function (equivalent to AX=4F05h) */
  dword_p=(unsigned long int*)(table+0x0c);
  *dword_p=0xc0000000+VGAEMU_BIOS_WINDOWFUNCTION;

  /* bytes per scan line */
  word_p=(unsigned int *)(table+0x10);
  *word_p=modeinfo->x_res;
  /* FIX THIS: this is not true for all modes! depends on memory model */
  
  /* width in pixels */
  word_p=(unsigned int *)(table+0x12);
  *word_p=modeinfo->x_res;
  
  /* height in pixels */
  word_p=(unsigned int *)(table+0x14);
  *word_p=modeinfo->y_res;
    
  /* width of character cell in pixels */
  table[0x16]=(unsigned char)modeinfo->x_box;
  
  /* height of character cell in pixels */
  table[0x17]=(unsigned char)modeinfo->y_box;
  
  /* number of memory planes */
  table[0x18]=1;
  /* FIX THIS: this depends on the memory model */
  
  switch(modeinfo->memorymodel)
    {
    case TEXT:
      table[0x19]=16;
      table[0x1a]=1;
      break;
      
    case CGA:
      table[0x19]=2;
      table[0x1a]=(modeinfo->y_res*modeinfo->x_res/4)/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res/4))
        table[0x1a]++;
      break;
      
    case HERC:
      table[0x19]=1;
      table[0x1a]=(modeinfo->y_res*modeinfo->x_res/8)/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res/8))
        table[0x1a]++;
      break;
      
    case PL4:
      table[0x19]=4;
      table[0x1a]=(modeinfo->y_res*modeinfo->x_res/2)/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res/2))
        table[0x1a]++;
      break;
      
    case PACKEDPIXEL:
      table[0x19]=9;
      table[0x1a]=modeinfo->y_res*modeinfo->x_res/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res))
        table[0x1a]++;
      break;
      
    case NONCHAIN4:
      table[0x19]=8;
      table[0x1a]=(modeinfo->y_res*modeinfo->x_res)/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res))
        table[0x1a]++;
      break;
      
    case DIRECT:
      table[0x19]=24;
      table[0x1a]=(modeinfo->y_res*modeinfo->x_res*3)/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res*3))
        table[0x1a]++;
      break;
      
    case YUV:
      table[0x19]=24;
      table[0x1a]=(modeinfo->y_res*modeinfo->x_res*3)/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res*3))
        table[0x1a]++;
      break;
      
    default:
#ifdef DEBUG_SUBFUNCTION
      v_printf("vesa: vesa_get_SVGA_mode_info(): Unknown memory model\n");
#endif
      table[0x19]=8;
      table[0x1a]=modeinfo->y_res*modeinfo->x_res/VGAEMU_BANK_SIZE;
      table[0x1a]=modeinfo->y_res*modeinfo->x_res/VGAEMU_BANK_SIZE;
      if((VGAEMU_BANK_SIZE*table[0x1a]) < (modeinfo->y_res*modeinfo->x_res))
        table[0x1a]++;
      break;
    }
  
  table[0x1b]=modeinfo->memorymodel;
  
  /* size of bank in K */
  table[0x1c]=(unsigned char)(VGAEMU_BANK_SIZE/1024);
  
  /* number of image pages */
  table[0x1d]=1;
  
  /* All OK */
  LWORD(eax)=0x004f;
  
  return(True);
}




/*
 * DANG_BEGIN_FUNCTION vesa_set_SVGA_mode
 * 
 * Is called from int10.c:int10->vesa.c:do_vesa_int
 * Calls int10:set_video_mode, which calls X_setmode
 * pff...
 * But it has this way to be I think / root@sjoerd /
 * The dangerous thing is when the functions disagree
 * DANG_END_FUNCTION
 */
int vesa_set_SVGA_mode(void)
{
#if 0
  if( (vga_emu_setmode(LWORD(ebx))==True) &&    /* vgaemu.c */
     (set_video_mode(LWORD(ebx))==1) )          /* int10.c */
    {
      v_printf("EBX=vesa_mode= 0x%03x\n",LWORD(ebx));
      LWORD(eax)=0x004f;
      return True;
    }
  else 
    {
      LWORD(eax)=0x014f;
#ifdef DEBUG
      v_printf("Unknown video mode %d\n",LWORD(ebx));
#endif
    }
#else
  int mode;
  
  mode=LWORD(ebx);
  
  if(mode>=0x100)
    mode=vesa_translate_mode(mode);
    
  if(mode<0)
    return(False);
    
  v_printf("vesa_set_SVGA_mode(): mode=0x%02x\n", mode);
  
  if(set_video_mode(mode)==1)
    {
      LWORD(eax)=0x014f;
      return(True);
    }
  else
    return(False);
#endif /* 0 */

  return(False);
}



int vesa_SVGA_memory_control(void)
{
  static unsigned int window_addressA=0;
/*  static unsigned int window_addressB=0;*/

  int sub=HI(bx);
  int window=LO(bx);

  /* Window B isn't supported */
  if(window==0x01)
    return(False);

  switch(sub)
    {
    case 0x00:  /* set */
      if(vga_emu_switch_bank(LWORD(edx))==True)
        {
	  window_addressA=LWORD(edx);
	  vga_emu_switch_bank(LWORD(edx));
	  return(True);
	}
      break;

    case 0x01:  /* get */
      LWORD(edx)=window_addressA;
      break;
      
    default:
      return(False);
    }
  
  return False;
}
