/*
 * video/vga.c - This file contains function for VGA-cards only 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"
#include "emu.h" 
#include "port.h"
#include "memory.h"
#include "video.h"
#include "vc.h"
#include "vga.h"

#include "et4000.h"
#include "s3.h"
#include "trident.h"
#if 0
#include "hgc.h"
#endif

/* Here are the REGS values for valid dos int10 call */

unsigned char vregs[34][60] =
{
/* BIOS mode 0x12 */
  {
  0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
  0x3C, 0x3D, 0x3E, 0x3F, 0x01, 0x00, 0x0F, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0F, 0xFF,
  0x03, 0x21, 0x0F, 0x00, 0x06,
  0xE3
  }
};

/* These are dummy calls */
void save_ext_regs_dummy(u_char xregs[], u_short xregs16[])
{
  return;
}

void restore_ext_regs_dummy(u_char xregs[], u_short xregs16[])
{
  return;
}

void set_bank_read_dummy(u_char bank)
{
  return;
}

void set_bank_write_dummy(u_char bank)
{
  return;
}

u_char dummy_ext_video_port_in(int port)
{
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void dummy_ext_video_port_out(u_char value, int port)
{
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

int vga_screenoff(void)
{
  v_printf("vga_screenoff\n");
  /* synchronous reset on */
  port_out(0x00, SEQ_I);
  port_out(0x01, SEQ_D);

  /* turn off screen for faster VGA memory acces */
  port_out(0x01, SEQ_I);
  port_out(port_in(SEQ_D) | 0x20, SEQ_D);

  /* synchronous reset off */
  port_out(0x00, SEQ_I);
  port_out(0x03, SEQ_D);

  return 0;
}

int vga_screenon(void)
{
  v_printf("vga_screenon\n");

  /* synchronous reset on */
  port_out(0x00, SEQ_I);
  port_out(0x01, SEQ_D);

  /* turn screen back on */
  port_out(0x01, SEQ_I);
  port_out(port_in(SEQ_D) & 0xDF, SEQ_D);

  /* synchronous reset off */
  port_out(0x00, SEQ_I);
  port_out(0x03, SEQ_D);

  return 0;
}

int vga_setpalvec(int start, int num, u_char * pal)
{
  int i, j;

  /* select palette register */
  port_out(start, PEL_IW);

  for (j = 0; j < num; j++) {
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
  }

  /* Upon Restore Videos, also restore current Palette REgs */
  /*  port_out(dosemu_regs.regs[PELIW], PEL_IW);
  port_out(dosemu_regs.regs[PELIR], PEL_IR); */

  return j;
}

int vga_getpalvec(int start, int num, u_char * pal)
{
  int i, j;

  /* Save Palette Regs */
  /* dosemu_regs.regs[PELIW]=port_in(PEL_IW);
  dosemu_regs.regs[PELIR]=port_in(PEL_IR); */

  /* select palette register */
  port_out(start, PEL_IR);

  for (j = 0; j < num; j++) {
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
  }

  /* Put Palette regs back */
  /* port_out(dosemu_regs.regs[PELIW], PEL_IW);
  port_out(dosemu_regs.regs[PELIR], PEL_IR); */
  return 0;
}
/* Prepare to do business with the EGA/VGA card */
__inline__ void disable_vga_card(void)
{
  /* enable video */
  port_in(IS1_R);
  port_out(0x00, ATT_IW);
}

/* Finish doing business with the EGA/VGA card */
__inline__ void enable_vga_card(void)
{
  port_in(IS1_R);
  port_out(0x20, ATT_IW);
  /* disable video */
}
/* Store current EGA/VGA regs */
int store_vga_regs(char regs[])
{
  int i;

  /* Start with INDEXS */
  regs[CRTI] = port_in(CRT_I);
  regs[GRAI] = port_in(GRA_I);
  regs[SEQI] = port_in(SEQ_I);
  regs[FCR] = port_in(FCR_R);
  regs[ISR1] = port_in(IS1_R) | 0x09;
  regs[ISR0] = port_in(IS0_R);

  /* save VGA registers */
  for (i = 0; i < CRT_C; i++) {
    port_out(i, CRT_I);
    regs[CRT + i] = port_in(CRT_D);
  }

  for (i = 0; i < ATT_C; i++) {
    port_in(IS1_R);
    port_out(i, ATT_IW);
    regs[ATT + i] = port_in(ATT_R);
  }
  for (i = 0; i < GRA_C; i++) {
    port_out(i, GRA_I);
    regs[GRA + i] = port_in(GRA_D);
  }
  for (i = 1; i < SEQ_C; i++) {
    port_out(i, SEQ_I);
    regs[SEQ + i] = port_in(SEQ_D);
  }
  regs[SEQ + 1] = regs[SEQ + 1] | 0x20;

  regs[MIS] = port_in(MIS_R);

  port_out(regs[CRTI], CRT_I);
  port_out(regs[GRAI], GRA_I);
  port_out(regs[SEQI], SEQ_I);
  v_printf("Store regs complete!\n");
  return 0;
}

/* Store EGA/VGA display planes (4) */
void store_vga_mem(u_char * mem, u_char mem_size[], u_char banks)
{

  int p[4], bsize, position;
  u_char cbank, plane, counter;

  p[0] = mem_size[0] * 1024;
  p[1] = mem_size[1] * 1024;
  p[2] = mem_size[2] * 1024;
  p[3] = mem_size[3] * 1024;
  bsize = p[0] + p[1] + p[2] + p[3];
#if 0
  dump_video_regs();
#endif
  if (config.chipset == ET4000) {
/*
 * The following is from the X files
 * we need this here , cause we MUST disable the ROM SYNC feature
*/
    u_char temp1, temp2;

    port_out(0x34, CRT_I);
    temp1 = port_in(CRT_D);
    port_out(temp1 & 0x0F, CRT_D);
    temp2 = port_in(0x3cd);
    port_out(0x00, 0x3cd);
  }
  if (!config.chipset || banks == 1) {
    set_regs((u_char *) vregs);
    position = 0;
    for (plane = 0; plane < 4; plane++) {

      /* Store planes */
      port_out(0x04, GRA_I);
      port_out(plane, GRA_D);

/*      memcpy((caddr_t) (mem + position), (caddr_t) GRAPH_BASE, p[plane]); */
      MEMCPY_2UNIX((caddr_t) (mem + position), (caddr_t) GRAPH_BASE, p[plane]);
      v_printf("READ Bank=%d, plane=0x%02x, *mem=0x%x, GRAPH_BASE=%08x, mem=0x%lx\n",
	 banks, plane, *(int *)(mem + position), *(int *) GRAPH_BASE, (Bit32u) (mem + position));
      position = position + p[plane];
    }
  }
  else {
    for (cbank = 0; cbank < banks; cbank++) {
      position = 0;
      if (cbank < 10) {
	for (plane = 0; plane < 4; plane++) {
	  set_bank_read((cbank * 4) + plane);
	  /*memcpy((caddr_t) (mem + (bsize * cbank) + position), (caddr_t) GRAPH_BASE, p[plane]);*/
	  MEMCPY_2UNIX((caddr_t) (mem + (bsize * cbank) + position), (caddr_t) GRAPH_BASE, p[plane]);
	  position = position + p[plane];
	  for (counter = 0; counter < 80; counter++) {
	    v_printf("%c", *(u_char *) (GRAPH_BASE + counter));
	  }
	  v_printf("\n");
	  for (counter = 0; counter < 80; counter++)
	    v_printf("0x%02x ", *(u_char *) (GRAPH_BASE + counter));
	  v_printf("\n");
	  v_printf("BANK READ Bank=%d, plane=0x%02x, mem=%08x\n", cbank, plane, *(int *) GRAPH_BASE);
	}
      }
    }
  }
  v_printf("GRAPH_BASE to mem complete!\n");
  if (config.chipset == ET4000) {
    port_out(0x00, 0x3cd);
  }
}

/* Restore EGA/VGA display planes (4) */
void restore_vga_mem(u_char * mem, u_char mem_size[], u_char banks)
{
  int p[4], bsize, position, plane;
  u_char cbank, counter;

  p[0] = mem_size[0] * 1024;
  p[1] = mem_size[1] * 1024;
  p[2] = mem_size[2] * 1024;
  p[3] = mem_size[3] * 1024;
  bsize = p[0] + p[1] + p[2] + p[3];

#if 0
  dump_video_regs();
#endif

  if (config.chipset == ET4000)
    port_out(0x00, 0x3cd);
  if (!config.chipset || banks == 1) {
    set_regs((u_char *) vregs);
    position = 0;
    for (plane = 0; plane < 4; plane++) {

      /* disable Set/Reset Register */
      port_out(0x01, GRA_I);
      port_out(0x00, GRA_D);

      /* Store planes */
      port_out(0x02, SEQ_I);
      port_out(1 << plane, SEQ_D);

/*      memcpy((caddr_t) GRAPH_BASE, (caddr_t) (mem + position), p[plane]); */
      MEMCPY_2DOS((caddr_t) GRAPH_BASE, (caddr_t) (mem + position), p[plane]);
      v_printf("WRITE Bank=%d, plane=0x%02x, *mem=%x, mem=%p\n", banks, plane, *(int *) (mem + position), (caddr_t) (mem + position));
      position = position + p[plane];
    }
  }
  else {
    plane = 0;
    for (cbank = 0; cbank < banks; cbank++) {
      position = 0;
      if (cbank < 10) {
	for (plane = 0; plane < 4; plane++) {
	  set_bank_write((cbank * 4) + plane);
	  /*memcpy((caddr_t) GRAPH_BASE, (caddr_t) (mem + (bsize * cbank) + position), p[plane]);*/
	  MEMCPY_2DOS((caddr_t) GRAPH_BASE, (caddr_t) (mem + (bsize * cbank) + position), p[plane]);
	  for (counter = 0; counter < 20; counter++) {
	    v_printf("0x%02x ", *(u_char *) (mem + (bsize * cbank) + position + counter));
	  }
	  v_printf("\n");
	  position = position + p[plane];
	}
      }
      v_printf("BANK WRITE Bank=%d, plane=0x%02x, mem=%08x\n", cbank, plane, *(int *) (mem + (bsize * cbank)));
    }
  }
  v_printf("mem to GRAPH_BASE complete!\n");
}

/* Restore EGA/VGA regs */
int restore_vga_regs(char regs[], u_char xregs[], u_short xregs16[])
{
  set_regs(regs);
  restore_ext_regs(xregs, xregs16);
  v_printf("Restore_vga_regs completed!\n");
  return 0;
}

/* Save all necessary info to allow switching vt's */
void save_vga_state(struct video_save_struct *save_regs)
{

  v_printf("Saving data for %s\n", save_regs->video_name);
  vga_screenoff();
  disable_vga_card();
  store_vga_regs(save_regs->regs);
  save_ext_regs(save_regs->xregs, save_regs->xregs16);
  v_printf("ALPHA mode save being attempted\n");
  port_out(0x06, GRA_I);
  if (!(port_in(GRA_D) & 0x01)) {
    save_regs->save_mem_size[0] = 8;
    save_regs->save_mem_size[1] = 8;
    save_regs->save_mem_size[2] = 8;
    save_regs->save_mem_size[3] = 8;
    save_regs->banks = 1;
    v_printf("ALPHA mode save being performed\n");
  }
  else {
    switch (save_regs->video_mode) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 0x51:
      save_regs->save_mem_size[0] = 64;
      save_regs->save_mem_size[1] = 64;
      save_regs->save_mem_size[2] = 64;
      save_regs->save_mem_size[3] = 64;
      save_regs->banks = 1;
      break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
      save_regs->save_mem_size[0] = 64;
      save_regs->save_mem_size[1] = 64;
      save_regs->save_mem_size[2] = 64;
      save_regs->save_mem_size[3] = 64;
      save_regs->banks = 1;
      break;
    default:
      save_regs->save_mem_size[0] = 64;
      save_regs->save_mem_size[1] = 64;
      save_regs->save_mem_size[2] = 64;
      save_regs->save_mem_size[3] = 64;
      save_regs->banks =
	(config.gfxmemsize + 255) / 256;
      break;
    }
  }
  v_printf("Mode  == %d\n", save_regs->video_mode);
  v_printf("Banks == %d\n", save_regs->banks);
  if (!save_regs->mem) {
    save_regs->mem = malloc((save_regs->save_mem_size[0] +
			     save_regs->save_mem_size[1] +
			     save_regs->save_mem_size[2] +
			     save_regs->save_mem_size[3]) * 1024 *
			    save_regs->banks
      );
   }

  store_vga_mem(save_regs->mem, save_regs->save_mem_size, save_regs->banks);
  vga_getpalvec(0, 256, save_regs->pal);
  restore_vga_regs(save_regs->regs, save_regs->xregs, save_regs->xregs16);
  enable_vga_card();

  v_printf("Store_vga_state complete\n");
}

/* Restore all necessary info to allow switching back to vt */
void restore_vga_state(struct video_save_struct *save_regs)
{

  v_printf("Restoring data for %s\n", save_regs->video_name);
  vga_screenoff();
  disable_vga_card();
  restore_vga_regs(save_regs->regs, save_regs->xregs, save_regs->xregs16);
  restore_vga_mem(save_regs->mem, save_regs->save_mem_size, save_regs->banks);
  if (save_regs->release_video) {
    v_printf("Releasing video memory\n");
    free(save_regs->mem);
    save_regs->mem = NULL;
  }
  vga_setpalvec(0, 256, save_regs->pal);
  restore_vga_regs(save_regs->regs, save_regs->xregs, save_regs->xregs16);
  v_printf("Permissions=%d\n", permissions);
  enable_vga_card();
  vga_screenon();

  v_printf("Restore_vga_state complete\n");
}

int vga_initialize(void)
{
  linux_regs.mem = NULL;
  dosemu_regs.mem = NULL;
  get_perm();

  switch (config.chipset) {
  case TRIDENT:
    vga_init_trident();
    v_printf("Trident CARD in use\n");
    break;
  case ET4000:
    vga_init_et4000();
    v_printf("ET4000 CARD in use\n");
    break;
  case S3:
    vga_init_s3();
    v_printf("S3 CARD in use\n");
    break;
  default:
    save_ext_regs = save_ext_regs_dummy;
    restore_ext_regs = restore_ext_regs_dummy;
    set_bank_read = set_bank_read_dummy;
    set_bank_write = set_bank_write_dummy;
    ext_video_port_in = dummy_ext_video_port_in;
    ext_video_port_out = dummy_ext_video_port_out;
    v_printf("Unspecific VIDEO selected = 0x%04x\n", config.chipset);
  }

  linux_regs.video_name = "Linux Regs";
  linux_regs.save_mem_size[0] = 8;
  linux_regs.save_mem_size[1] = 8;
  linux_regs.save_mem_size[2] = 8;
  linux_regs.save_mem_size[3] = 8;
  linux_regs.banks = 1;
  linux_regs.video_mode = 3;
  linux_regs.release_video = 0;

  dosemu_regs.video_name = "Dosemu Regs";
  dosemu_regs.save_mem_size[0] = 8;
  dosemu_regs.save_mem_size[1] = 8;
  dosemu_regs.save_mem_size[2] = 8;
  dosemu_regs.save_mem_size[3] = 8;
  dosemu_regs.banks = (config.gfxmemsize + 255) / 256;
  dosemu_regs.video_mode = 3;
  dosemu_regs.regs[CRTI] = 0;
  dosemu_regs.regs[SEQI] = 0;
  dosemu_regs.regs[GRAI] = 0;

  /* don't release it; we're trying text mode restoration */
  dosemu_regs.release_video = 1;

  v_printf("VGA: mem size %d, banks %d\n", config.gfxmemsize,
	   dosemu_regs.banks);

  save_vga_state(&linux_regs);
#if 0
  save_vga_state(&dosemu_regs);
  restore_vga_state(&dosemu_regs);
#endif
  vga_screenon();
  memset((caddr_t) linux_regs.mem, ' ', 8 * 1024);
  dump_video_linux();
  video_initialized = 1;
  /* release_perm(); */
  return 0;
}

/* Store current actuall EGA/VGA regs */
void dump_video_regs()
{
  int i;

  /* save VGA registers */
  v_printf("CRT=");
  for (i = 0; i < CRT_C; i++) {
    port_out(i, CRT_I);
    v_printf("%02x ", (u_char) port_in(CRT_D));
  }
  v_printf("\n");
  v_printf("ATT=");
  for (i = 0; i < ATT_C; i++) {
    port_in(IS1_R);
    port_out(i, ATT_IW);
    v_printf("%02x ", (u_char) port_in(ATT_R));
  }
  v_printf("\n");
  v_printf("GRA=");
  for (i = 0; i < GRA_C; i++) {
    port_out(i, GRA_I);
    v_printf("%02x ", (u_char) port_in(GRA_D));
  }
  v_printf("\n");
  v_printf("SEQ=");
  for (i = 0; i < SEQ_C; i++) {
    port_out(i, SEQ_I);
    v_printf("%02x ", (u_char) port_in(SEQ_D));
  }
  v_printf("\n");
  v_printf("MIS=0x%02x\n", (u_char) port_in(MIS_R));

  /* et4000 regs should be added here .... :-) */
  if (config.chipset == TRIDENT) {
    trident_set_old_regs();
    port_out(0x0c, SEQ_I);
    v_printf("0C=0x%02x ", (u_char) port_in(SEQ_D));
    port_out(0x0d, SEQ_I);
    v_printf("0D=0x%02x ", (u_char) port_in(SEQ_D));
    port_out(0x0e, SEQ_I);
    v_printf("0E=0x%02x ", (u_char) port_in(SEQ_D));
    trident_set_new_regs();
    port_out(0x0d, SEQ_I);
    v_printf("0D=0x%02x ", (u_char) port_in(SEQ_D));
    port_out(0x0e, SEQ_I);
    v_printf("0E=0x%02x ", (u_char) port_in(SEQ_D));
    port_out(0x0f, SEQ_I);
    v_printf("0F=0x%02x ", (u_char) port_in(SEQ_D));
    port_out(0x1e, CRT_I);
    v_printf("CRT 1E=0x%02x ", (u_char) port_in(CRT_D));
    port_out(0x1f, CRT_I);
    v_printf("CRT 1F=0x%02x ", (u_char) port_in(CRT_D));
    port_out(0x0f, GRA_I);
    v_printf("GRA 0F=0x%02x\n", (u_char) port_in(GRA_D));
  }
  else if (config.chipset == ET4000) {
    v_printf("ET4000 port 0x3c3=0x%02x\n", (u_char) port_in(0x3c3));
    v_printf("ET4000 port 0x3cd=0x%02x\n", (u_char) port_in(0x3cd));
    port_out(0x24, CRT_I);
    v_printf("ET4000 CRT 0x24  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x32, CRT_I);
    v_printf("ET4000 CRT 0x32  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x33, CRT_I);
    v_printf("ET4000 CRT 0x33  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x34, CRT_I);
    v_printf("ET4000 CRT 0x34  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x35, CRT_I);
    v_printf("ET4000 CRT 0x35  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x36, CRT_I);
    v_printf("ET4000 CRT 0x36  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x37, CRT_I);
    v_printf("ET4000 CRT 0x37  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x3f, CRT_I);
    v_printf("ET4000 CRT 0x3f  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x6, SEQ_I);
    v_printf("ET4000 SEQ 0x06  =0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x7, SEQ_I);
    v_printf("ET4000 SEQ 0x07  =0x%02x\n", (u_char) port_in(SEQ_D));
    port_in(IS1_R);
    port_out(0x16, ATT_IW);
    v_printf("ET4000 ATT 0x16  =0x%02x\n", (u_char) port_in(ATT_R));
    v_printf("ET4000 port 0x3cb=0x%02x\n", (u_char) port_in(0x3cb));
    v_printf("ET4000 port 0x3d8=0x%02x\n", (u_char) port_in(0x3d8));
    v_printf("ET4000 port 0x3bf=0x%02x\n", (u_char) port_in(0x3bf));
    port_out(0xd, GRA_I);
    v_printf("ET4000 GRA 0x0d  =0x%02x\n", (u_char) port_in(GRA_D));
    port_out(0xe, GRA_I);
    v_printf("ET4000 GRA 0x0e  =0x%02x\n", (u_char) port_in(GRA_D));
  }
}

/* init_vga_card - Inititalize a VGA-card */

void init_vga_card(void)
{

  unsigned char *ssp;
  unsigned long sp;

#if 0
#define ADDR 0x0000
  u_char *bios_mem, buffer[0x1000];

#endif

  if (!config.mapped_bios) {
    error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
    return;
  }
  if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
    warn("couldn't get range!\n");
  config.vga = 1;
  set_vc_screen_page(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));

#if 0
  open_kmem();
  bios_mem =
    (char *) mmap(
		   (caddr_t) (0x0),
		   (size_t) 0x1000,
		   PROT_READ,
		   MAP_SHARED /* | MAP_FIXED */ ,
		   mem_fd,
		   (off_t) (ADDR)
    );
  if ((caddr_t) bios_mem == (caddr_t) (-1)) {
    perror("OOPS");
    leavedos(0);
  }

  v_printf("Video interrupt is at %04x, mode=0x%02x\n",
	   *(unsigned int *) (bios_mem + 0x40),
	   (u_char) bios_mem[0x449]);

  memmove(buffer, bios_mem, 0x1000);
  munmap((caddr_t) bios_mem, 0x1000);

  close_kmem();
  v_printf("SEG=0x%02x, OFF=0x%02x\n", *(u_short *) (buffer + 0x42), *(u_short *) (buffer + 0x40));
  SETIVEC(0x10, *(u_short *) (buffer + 0x42), *(u_short *) (buffer + 0x40));
  memmove((caddr_t) 0x449, (caddr_t) (buffer + 0x449), 0x25);

#else

  ssp = (unsigned char *)(READ_SEG_REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);
  pushw(ssp, sp, READ_SEG_REG(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 4;
  WRITE_SEG_REG(cs, INT10_SEG);
  LWORD(eip) = INT10_OFF;
#endif
}

/* End of video/vga.c */
