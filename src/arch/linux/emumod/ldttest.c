#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>

#define WANT_WINDOWS
#include "../include/ldt.h" /* dosemu's ldt.h !! */
#include "../include/kernel_space.h"

_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)


#define MODIFY_LDT_CREATE_KERNEL_DESCRIPTOR 0x10

/* NOTE
 * in emumodule LDT stuff there is an new function for modify_ldt (0x10)
 * which creates a discriptor pointing into the kernel space.
 *
 * ====================================
 * THIS CAN ONLY BE DONE UNDER UID ROOT, else -EACCES is returned.
 * ====================================
 *
 * The contents of struct modify_ldt_ldt_s must be set as follows:
 *
 * s.entry_number    = selector >> 3;
 * s.base_addr       = 0, then an descriptor to the LDT itself is created
 * or
 * s.base_addr       > 0 and < high_memory, then a descriptor into kernelspace
 *                       is created. e.g. 0x100000 will point to 'startup_32'
 * s.limit =         as usual, but will be overwritten in case of base_addr=0
 * s.seg_32bit=      as usual
 * s.contents        must be <=1, but will be set to 0 in case of base_addr=0
 * s.read_exec_only  as usual, but NOTE: Be careful not to produce a securety hole.
 * s.limit_in_pages  as usual,  but will be set to 0 in case of base_addr=0
 * s.seg_not_present is allways set to 0
 *
 */

#if 1

/* alternative LDT-access method ======================= */

struct descriptor_struct {
  unsigned short limit0_15;
  unsigned short base0_15;
  unsigned char base16_23;
  unsigned char typebyte;
  unsigned char limit16_19:4;
  unsigned char moretypebits:4;
  unsigned char base24_31;
};


#define selector_base 0x20
unsigned short ldt_sel = selector_base+8 | 7;
unsigned short usr_sel = selector_base | 7;
unsigned short org_usr_sel;

struct descriptor_struct *ldt;

int init_ldt_stuff(void)
{
  struct modify_ldt_ldt_s s;
  struct descriptor_struct d;
  
  /* first create the ldt and the ldt alias descriptor */
  memset(&s,0,sizeof(s));
  s.entry_number = ldt_sel >> 3;
  s.seg_32bit=1;
  s.contents = MODIFY_LDT_CONTENTS_DATA;
  if (modify_ldt(MODIFY_LDT_CREATE_KERNEL_DESCRIPTOR,&s,sizeof(struct modify_ldt_ldt_s))) return -1;

  /* built a pointer to the ldt relative to user space */
  memcopy_from_huge(&d, ldt_sel, (void *)(ldt_sel & ~7), 8);
  ldt = (void *)(d.base0_15 | (d.base16_23 << 16) | (d.base24_31 << 24));
    
  /* now create an 'all rights' descriptor for user space */
  put_huge_long(0xffff, ldt_sel, (void *)(usr_sel & ~7));
  put_huge_long(0xcff300, ldt_sel, (void *)(usr_sel & ~7)+4);
    
  /* save the original user space selector */
  __asm__("movw %%ds,%0" : "=m" (org_usr_sel));
  return 0;
}


inline void unprotect_kernel_space()
{
  __asm__("mov %%ax,%%ds; mov %%ax,%%es; mov %%ax,%%ss"::"a" (usr_sel) );
}


inline void protect_kernel_space()
{
  __asm__("mov %%ax,%%ds; mov %%ax,%%es; mov %%ax,%%ss"::"a" (org_usr_sel) );
}



main()
{
  unsigned long b[4];
  
  if (init_ldt_stuff()) {
    printf("error on ldt initialization\n");
    exit(1);
  }

  /* NOTE: we only can unprotect the kernel space as a whole,
   *       so, we do it only temporary.
   *       Please avoid calling any library routines, as they may
   *       rely on a protected kernel.
   *
   * Advantage of this access method is, we can do normal Cish addressing.
   */
  unprotect_kernel_space();
  memcpy(b,&ldt[usr_sel>>3],16);
  protect_kernel_space();
   /* ... and now w're back to protected kernel space */

  printf("usr_descr= %08x %08x, ldt_descr= %08x %08x\n", b[0], b[1], b[2], b[3]);
}



#else

/* secure LDT access method ===================================== */

main()
{
  struct modify_ldt_ldt_s s;
  int ret;
  unsigned sel= 0x100 | 7;
  unsigned sel2=0x108 | 7;
  unsigned sel3=0x110 | 7;
  unsigned long b[6]={0};
  unsigned long *ldt;
    
  s.entry_number = sel >> 3;
  s.base_addr = 0;
  s.limit = 0;
  s.seg_32bit=1;
  s.contents = MODIFY_LDT_CONTENTS_DATA;
  s.read_exec_only = 0;
  s.limit_in_pages =0;
  s.seg_not_present = 0;
  
  ret = modify_ldt(MODIFY_LDT_CREATE_KERNEL_DESCRIPTOR,&s,sizeof(struct modify_ldt_ldt_s));
  memcopy_from_huge(b, sel, (void *) (sel & ~7), 16);
  printf("descr1= %08x %08x, descr2= %08x %08x\n",b[0],b[1],b[2],b[3]);
  ldt=(unsigned long *)((b[0]>>16) | ((b[1] & 255)<<16) | (b[1] & ~0xffffff));
  printf("ldt address = %08x\n",ldt);

  memcopy_to_huge(b, sel, (void *) (sel2 & ~7), 8);
  memcopy_from_huge(b, sel2, (void *) (sel & ~7), 16);
  printf("descr1= %08x %08x, descr2= %08x %08x\n",b[0],b[1],b[2],b[3]);
  printf("get_huge_long(sel2,(void *) (sel & ~7)) = %08x\n",
          get_huge_long(sel2,(void *) (sel & ~7)));
  printf("get_huge_word(sel2,(void *) (sel & ~7)) = %04x\n",
          get_huge_word(sel2,(void *) (sel & ~7)));
  printf("get_huge_byte(sel2,(void *) ((sel & ~7)+4)) = %04x\n",
          get_huge_byte(sel2,(void *) ((sel & ~7)+4)));
  put_huge_long(get_huge_long(sel2,(void *) (sel & ~7))+((sel & ~7) << 16), sel2, (void *) (sel & ~7));
  b[0]=get_huge_long(sel2,(void *) (sel & ~7));
  printf("descr1= %08x %08x, descr2= %08x %08x\n",b[0],b[1],b[2],b[3]);
  put_huge_byte(0xaa, sel, (void *) 8);
  put_huge_word(0xccbb, sel, (void *) 9);
  memcopy_from_huge(b, sel, (void *) 0, 16);
  printf("descr1= %08x %08x, descr2= %08x %08x\n",b[0],b[1],b[2],b[3]);
  


  s.entry_number = sel3 >> 3;
  s.base_addr = 0;
  s.limit = 0xbffff;
  s.seg_32bit=1;
  s.contents = MODIFY_LDT_CONTENTS_DATA;
  s.read_exec_only = 0;
  s.limit_in_pages =1;
  s.seg_not_present = 0;
  
  ret = modify_ldt(1,&s,sizeof(struct modify_ldt_ldt_s));
  put_huge_byte(get_huge_byte(sel,(void *)16+6) | 0xf, sel,(void *)16+6);
  memcopy_from_huge(b, sel, (void *) 0, 24);
  printf("descr1= %08x %08x, descr2= %08x %08x, descr3= %08x %08x\n",b[0],b[1],b[2],b[3],b[4],b[5]);


  exit(0);
  
/* VERY DANGEROUS access method, I did it just to see if it works ========= */


  /* well, here we *really* are hacking in a diabolic way ... aieiiii...
   * we replace the standard DS, ES, SS with our new ones
   * This IS terribly dangerous, all protection is disabled !!!
   */
  __asm__("mov %%ax,%%ds; mov %%ax,%%es; mov %%ax,%%ss"::"a" ((short)sel3) );



  printf("descr1= %08x %08x, descr2= %08x %08x, descr3= %08x %08x\n",b[0],b[1],b[2],b[3],b[4],b[5]);
  ldt +=(sel >>2) & ~1;
  printf("descr1= %08x %08x, descr2= %08x %08x, descr3= %08x %08x\n", ldt[0],ldt[1],ldt[2],ldt[3],ldt[4],ldt[5]);
  
}
#endif
