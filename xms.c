/* xms.c for the DOS emulator 
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/02/16 00:21:29 $
 * $Source: /usr/src/dos/RCS/xms.c,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: xms.c,v $
 * Revision 1.3  1993/02/16  00:21:29  root
 * DOSEMU 0.48 DISTRIBUTION
 *
 * Revision 1.2  1993/02/13  23:37:20  root
 * latest version, no time to document!
 *
 * Revision 1.1  1993/02/10  20:55:01  root
 * Initial revision
 *
 * NOTE: I keep the BYTE size of EMB's in a field called "size" in the EMB
 *    structure.  Most XMS calls specify/expect size in KILOBYTES.
 */

#define XMS_C

#include <stdlib.h>
#include <sys/types.h>
#include "emu.h"
#include "xms.h"

static char RCSxms[]="$Header: /usr/src/dos/RCS/xms.c,v 1.3 1993/02/16 00:21:29 root Exp $";

extern int xms, vga, graphics, mapped_bios;
extern int extmem_size;

int a20=0;
int freeC=1,     /* is 0xc000 free? */ 
    freeA=0,
    freeD=1,
    freeF=0,
    freeHMA=1;   /* is HMA free? */

struct UMB umbs[]={
  { 0xa000 },
  { 0xc000 },
  { 0xd000 },
  { 0xf000 }
};

static struct Handle handles[NUM_HANDLES+1];
static int    handle_count=0;
static long   handle_mask=0;  /* 32 bits for 32 handles */

static int xms_grab_int15=0;  /* non-version XMS call been made yet? */

struct EMM get_emm(unsigned int, unsigned int);
void show_emm(struct EMM);
void xms_query_freemem(void),
     xms_allocate_EMB(void),
     xms_free_EMB(void),
     xms_move_EMB(void),
     xms_lock_EMB(int),
     xms_EMB_info(void),
     xms_realloc_EMB(void),
     xms_request_UMB(void),
     xms_release_UMB(void);

int FindFreeHandle(int);


void xms_init(void)
{
  int i;

  x_printf("XMS: initializing XMS...\nRCS: %s\n%d handles\n\n", 
	   RCSxms, NUM_HANDLES);

  freeHMA = 1;
  freeA = graphics ? 0 : 1;  /* no A000 block if vga mode */
  freeC = freeF = mapped_bios ? 0 : 1;
  
  a20=0;
  xms_grab_int15=0;

  handle_count=0;
  handle_mask=0;
  for (i=0; i<NUM_HANDLES+1; i++)
      handles[i].valid=0;
}

void xms_control(void)
{
  /* the int15 get memory size call (& block copy, unimplemented) are
   * supposed to be unrevectored until the first non-version XMS call
   * to "maintain compatibility with existing device drivers."
   * hence this blemish.  if Microsoft could act, say, within 3 years of
   * a problem's first appearance, maybe we wouldn't have these kinds
   * of compatibility hacks.
   */
  if (HI(ax) != 0) xms_grab_int15=1;

  switch(HI(ax))
    {
       case 0:   /* Get XMS Version Number */
         _regs.eax=XMS_VERSION;
	 _regs.ebx=XMS_DRIVER_VERSION;
	 _regs.edx=1;  /* HMA exists */
         x_printf("XMS driver version. XMS Spec: %04x dosemu-XMS: %04x\n",
		  _regs.eax, _regs.ebx);
	 x_printf(RCSxms);
	 break;

       case 1:    /* Request High Memory Area */
	 x_printf("XMS request HMA size 0x%04x\n", WORD(_regs.edx));
	 if (freeHMA)
	   {
	     x_printf("XMS: allocating HMA\n");
	     freeHMA=0;
	     _regs.eax=1;
	   }
	 else
	   {
	     x_printf("XMS: HMA already allocated\n");
	     _regs.eax=0;
	     LO(bx) = 0x91;
	   }
	 break;

       case 2:    /* Release High Memory Area */
	 x_printf("XMS release HMA\n");
	 if (freeHMA)
	   {
	     x_printf("XMS: HMA already free\n");
	     _regs.eax=0;
	     LO(bx) = 0x93;
	   }
	 else
	   {
	     x_printf("XMS: freeing HMA\n");
	     _regs.eax=1;
	     freeHMA=1;
	   }
	 break;

       case 3:    /* Global Enable A20 */
	 x_printf("XMS global enable A20\n");
	 a20=1;
	 _regs.eax=1;
	 break;

       case 4:    /* Global Disable A20 */
	 x_printf("XMS global disable A20\n");
	 a20=0;
	 _regs.eax=1;
	 break;

       case 5:    /* Local Enable A20 */
	 a20++;
	 _regs.eax=1;
	 break;

       case 6:    /* Local Disable A20 */
	 x_printf("XMS LOCAL disable A20\n");
	 if (a20) a20--;
	 _regs.eax=1;
	 break;

	 /* DOS, if loaded as "dos=high,umb" will continually poll
	  * the A20 line.
          */
       case 7:    /* Query A20 */
	 _regs.eax = a20 ? 1 : 0;
	 _regs.ebx=0;  /* no error */
	 break;

       case 8:    /* Query Free Extended Memory */
	 xms_query_freemem();
	 break;

       case 9:    /* Allocate Extended Memory Block */
	 xms_allocate_EMB();
	 break;

       case 0xa:   /* Free Extended Memory Block */
	 xms_free_EMB();
	 break;

       case 0xb:   /* Move Extended Memory Block */
	 xms_move_EMB();
	 break;

       case 0xc:   /* Lock Extended Memory Block */
	 xms_lock_EMB(1);
	 break;

       case 0xd:   /* Unlock Extended Memory Block */
	 xms_lock_EMB(0);
	 break;

       case 0xe:   /* Get EMB Handle Information */
	 xms_EMB_info();
	 return;

       case 0xf:   /* Realocate Extended Memory Block */
	 xms_realloc_EMB();
	 break;

       case 0x10:  /* Request Upper memory Block */
	 xms_request_UMB();
	 break;

       case 0x11:
	 xms_release_UMB();
	 break;

       case 0x12:
	 x_printf("XMS realloc UMB segment 0x%04x size 0x%04x\n",
		  WORD(_regs.edx), WORD(_regs.ebx));
	 _regs.eax=0;    /* failure */
	 LO(bx) = 0x80;  /* function unimplemented */
	 break;

       default:
	 error("Unimplemented XMS function AX=0x%04x", WORD(_regs.eax));
	 show_regs();  /* if you delete this, put the \n on the line above */
	 _regs.eax=0;  /* failure */
	 LO(bx)=0x80;  /* function not implemented */

    }
}


int FindFreeHandle(int start)
{
  int i,h=0;

  /* first free handle is 1 */
  for (i=start; (i<=NUM_HANDLES) && (h == 0); i++)
    {
      if (! handles[i].valid)
	{
	  x_printf("\nXMS: found free handle: %d\n", i);
	  h=i;
	  break;
	}
      else x_printf("XMS: unfree handle %d ", i);
    }
  return h;
}

int ValidHandle(unsigned short h)
{
  if ((h <= NUM_HANDLES) && (handles[h].valid)) return 1;
  else return 0;
}


void xms_query_freemem(void)
{
  unsigned long totalBytes=0;
  int h;

  /* total XMS mem and largest block under DOSEMU will always be the
   * same, thanks to the wonderful operating environment
   */

  totalBytes=0;
  for (h=FIRST_HANDLE; h<= NUM_HANDLES; h++)
    {
      if (ValidHandle(h))
	totalBytes += handles[h].size;
    }

  /* total free is max allowable XMS - the number of K already allocated */
  _regs.eax = _regs.edx = MAX_XMS - (totalBytes / 1024);

  LO(bx)=0;  /* no error */
  x_printf("XMS query free memory: %dK %dK\n", _regs.eax, _regs.edx);
}

void xms_allocate_EMB(void)
{
  int h;

  x_printf("XMS alloc EMB size 0x%04x\n", WORD(_regs.edx));
  if (! (h=FindFreeHandle(FIRST_HANDLE)) )
    {
      x_printf("XMS: out of handles\n");
      _regs.eax=0;
      LO(bx) = 0xa1;
    } else {

      handles[h].num=h;
      handles[h].valid=1;
      handles[h].size=WORD(_regs.edx) * 1024;
      if (handles[h].size) handles[h].addr=malloc(handles[h].size);
      else {
	x_printf("XMS WARNING: allocating 0 size EMB\n");
	handles[h].addr=0;
      }
      handles[h].lockcount=0;
      handle_count++;
      handle_mask |= (1 << h);  /* set mask for handle */

      x_printf("XMS: allocated EMB %d at %08x\n", h, handles[h].addr);

      _regs.edx=h;  /* handle # */
      _regs.eax=1;  /* success */
    }
}



void xms_free_EMB(void)
{
  unsigned short int h=WORD(_regs.edx);

  if (! ValidHandle(h))
    {
      x_printf("XMS: free EMB bad handle %d\n", h);
      _regs.eax=0;
      LO(bx)=0xa2;
    } else {
      
      if (handles[h].addr) free(handles[h].addr);
      else x_printf("XMS WARNING: freeing handle w/no address, size 0x%08x\n",
		    handles[h].size);
      handles[h].valid=0;
      handle_count--;
      handle_mask &= ~(1 << h); /* clear handle bit */

      x_printf("XMS: free'd EMB %d\n", h);
      _regs.eax=1;
    }
}


void xms_move_EMB(void)
{
  char *src, *dest;
  struct EMM e = get_emm(_regs.ds, _regs.esi);

  x_printf("XMS move extended memory block\n");
  show_emm(e);

  if (e.SourceHandle == 0)
    {
      src =  (char *)(((e.SourceOffset >> 16) << 4) + \
		      (e.SourceOffset & 0xffff));
    } else {
      if (handles[e.SourceHandle].valid == 0)
	error("XMS: invalid source handle\n");
      src = handles[e.SourceHandle].addr + e.SourceOffset;
    }

  if (e.DestHandle == 0)
    {
      dest =  (char *)(((e.DestOffset >> 16) << 4) + \
		       (e.DestOffset & 0xffff));
    } else {
      if (handles[e.DestHandle].valid == 0)
	error("XMS: invalid dest handle\n");
      dest = handles[e.DestHandle].addr + e.DestOffset;
    }

  x_printf("XMS: block move from %08x to %08x len %08x\n", 
	   src, dest, e.Length);

  memmove(dest, src, e.Length);
  _regs.eax=1;  /* success */

  x_printf("XMS: block move done\n");

  return;
}


void xms_lock_EMB(int flag)
{
 int h=WORD(_regs.edx);

 if (flag) x_printf("XMS lock EMB %d\n", h);
 else x_printf("XMS unlock EMB %d\n", h);

  if (ValidHandle(h))
    {
      /* flag=1 locks, flag=0 unlocks */

      if (flag) handles[h].lockcount++;
      else handles[h].lockcount--;

      _regs.ebx = (int)handles[h].addr >> 16;
      _regs.edx = (int)handles[h].addr & 0xffff;

      _regs.eax=1;
    }
  else {
    x_printf("XMS: invalid handle %d, can't (un)lock\n", h);
    _regs.eax=0;
    LO(bx)=0xa2;  /* invalid handle */
  }
}


void xms_EMB_info(void)
{
  int h=WORD(_regs.edx);

  LO(bx) = NUM_HANDLES - handle_count;
  if (ValidHandle(h))
    {
      HI(bx) = handles[h].lockcount;
      _regs.edx = handles[h].size / 1024;
      _regs.eax=1;
      x_printf("XMS Get EMB info %d\n", h); 
    }
  else {
    /* x_printf("XMS: invalid handle %d, no info\n", h); */
    _regs.eax=0;
#if 0
    LO(bx)=0xa2;  /* invalid handle */
#endif
  }
}


void xms_realloc_EMB(void)
{
  int h;
  unsigned short oldsize;
  unsigned char *oldaddr;

  h=WORD(_regs.edx);
  x_printf("XMS realloc EMB %d new size 0x%04x\n", h, WORD(_regs.ebx));

  if (! ValidHandle(h))
    {
      x_printf("XMS: invalid handle %d, cannot realloc\n", h);

      LO(bx) = 0xa2;
      _regs.eax = 0;
      return;
    }
  else if (handles[h].lockcount)
    {
      x_printf("XMS: handle %d locked (%d), cannot realloc\n", 
	       h, handles[h].lockcount);
      
      LO(bx) = 0xab;
      _regs.eax = 0;
      return;
    }
    
  oldsize=handles[h].size;
  oldaddr=handles[h].addr;

  handles[h].size=WORD(_regs.ebx) * 1024;
  handles[h].addr=malloc(handles[h].size);

  /* memcpy() the old data into the new block, but only
   * min(new,old) bytes.
   */

  memcpy(handles[h].addr, oldaddr, 
	 (oldsize <= handles[h].size) ? oldsize : handles[h].size);

  /* free the old EMB */
  free(oldaddr);

  _regs.eax=1;  /* success */
}


void xms_int15(void)
{
  if (HI(ax) == 0x88)
    {
      NOCARRY;
      if (xms_grab_int15)
	{
	  x_printf("XMS int 15 free mem returning 0 to protect HMA\n");
	  _regs.eax &= ~0xffff;
	}
      else
	{
	  x_printf("XMS early int15 call...%dK free\n", extmem_size);
	  _regs.eax=extmem_size;
	}
    }
  else {   /* AH = 0x87 */
    x_printf("XMS int 15 block move failed\n");
    _regs.eax&=0xFF;
    HI(ax) = 3;	/* say A20 gate failed */
    CARRY;
  }
}


struct EMM get_emm(unsigned int seg, unsigned int off)
{
  struct EMM e;
  unsigned char *p;

  p=(char *)((seg << 4) + off);

  e.Length=*(unsigned long *)p;
  p+=4;
  e.SourceHandle=*(unsigned short *)p;
  p+=2;
  e.SourceOffset=*(unsigned long *)p;
  p+=4;
  e.DestHandle=*(unsigned short *)p;
  p+=2;
  e.DestOffset=*(unsigned long *)p;

  return e;
}


void show_emm(struct EMM e)
{
  int i;

  x_printf("XMS show_emm:\n");

  x_printf("L: 0x%08x\nSH: 0x%04x  SO: 0x%08x\nDH: 0x%04x  DO: 0x%08x\n",
	   e.Length, e.SourceHandle, e.SourceOffset,
	   e.DestHandle, e.DestOffset);
}


void xms_request_UMB(void)
{
  int size=WORD(_regs.edx);

  x_printf("XMS request UMB size 0x%04x\n", size);
  if ((size > 0xfff) && (freeA||freeC||freeD||freeF))
    {
      x_printf("XMS: request UMB too large, but one available.\n");

      _regs.edx=0xfff;  /* 0xfff paragraphs is 0xfff0 bytes */
      LO(bx) = 0xb0;
      _regs.eax=0;
      return;
    }
  else if (freeC)
    {
      x_printf("XMS: giving UMB at 0xc000 size 0x%04x bytes\n",
	       WORD(_regs.edx) * PARAGRAPH);
      _regs.ebx=0xc000;
      /* DX stays the same... */
      freeC=0;
      _regs.eax=1;
    }
  else if (freeA)
    {
      x_printf("XMS: giving UMB at 0xA000 size 0x%04x bytes\n",
	       WORD(_regs.edx) * PARAGRAPH);
      _regs.ebx=0xA000;
      /* DX stays the same... */
      freeA=0;
      _regs.eax=1;
    }
  else if (freeF)
    {
      x_printf("XMS: giving UMB at 0xF000 size 0x%04x bytes\n",
	       WORD(_regs.edx) * PARAGRAPH);
      _regs.ebx=0xF000;
      /* DX stays the same... */
      freeF=0;
      _regs.eax=1;
    }
  else if (freeD)
    {
      x_printf("XMS: giving UMB at 0xD000 size 0x%04x bytes\n",
	       WORD(_regs.edx) * PARAGRAPH);
      _regs.ebx=0xD000;
      /* DX stays the same... */
      freeD=0;
      _regs.eax=1;
    }
  else {
    x_printf("XMS: no UMB's available\n");
    _regs.eax=0;    /* failure */
    LO(bx) = 0xb1;  /* no UMBs available */
  }
}

void xms_release_UMB(void)
{
  x_printf("XMS release UMB segment 0x%04x\n", WORD(_regs.edx));
  if (!freeC && (WORD(_regs.edx) == 0xc000))
    {
      x_printf("XMS: releasing 0xc000 UMB\n");
      _regs.eax=1;
      freeC=1;
    }
  else if (!freeA && (WORD(_regs.edx) == 0xa000))
    {
      x_printf("XMS: releasing 0xa000 UMB\n");
      _regs.eax=1;
      freeA=1;
    }
  else if (!freeF && (WORD(_regs.edx) == 0xf000))
    {
      x_printf("XMS: releasing 0xf000 UMB\n");
      _regs.eax=1;
      freeF=1;
    }
  else if (!freeD && (WORD(_regs.edx) == 0xD000))
    {
      x_printf("XMS: releasing 0xd000 UMB\n");
      _regs.eax=1;
      freeD=1;
    }
  else {
    x_printf("XMS: release invalid UMB 0x%04x\n", WORD(_regs.edx));
    _regs.eax=0;
    LO(bx)=0xb2;
  }
}

#undef XMS_C
