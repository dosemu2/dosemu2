/* VGAlib version 1.1 - Copyright 1992 Tommy Frandsen 		   */
/*								   */
/* This library is free software; you can redistribute it and/or   */
/* modify it without any restrictions. This library is distributed */
/* in the hope that it will be useful, but without any warranty;   */
/* without even the implied warranty of merchantability or fitness */
/* for a particular purpose.					   */
/* $Log:                  */

#ifndef VIDEO_H
#define VIDEO_H

#define uchar unsigned char
#define TEXT 	     0
#define G320x200x16  1
#define G640x200x16  2
#define G640x350x16  3
#define G640x480x16  4
#define G320x200x256 5
#define G320x240x256 6
#define G320x400x256 7
#define G360x480x256 8
#define G640x480x2   9
#define G640x480x256  10
#define G800x600x256  11
#define G1024x768x256 12

#define CRT_IC  0x3D4		/* CRT Controller Index - color emulation */
#define CRT_IM  0x3B4		/* CRT Controller Index - mono emulation */
#define ATT_IW  0x3C0		/* Attribute Controller Index & Data Write Register */
#define GRA_I   0x3CE		/* Graphics Controller Index */
#define SEQ_I   0x3C4		/* Sequencer Index */
#define PEL_IW  0x3C8		/* PEL Write Index */
#define PEL_IR  0x3C7		/* PEL Read Index */

/* VGA data register ports */
#define CRT_DC  0x3D5		/* CRT Controller Data Register - color emulation */
#define CRT_DM  0x3B5		/* CRT Controller Data Register - mono emulation */
#define ATT_R   0x3C1		/* Attribute Controller Data Read Register */
#define GRA_D   0x3CF		/* Graphics Controller Data Register */
#define SEQ_D   0x3C5		/* Sequencer Data Register */
#define MIS_R   0x3CC		/* Misc Output Read Register */
#define MIS_W   0x3C2		/* Misc Output Write Register */
#define IS0_R   0x3C2		/* Input Status Register 0 */
#define IS1_RC  0x3DA		/* Input Status Register 1 - color emulation */
#define IS1_RM  0x3BA		/* Input Status Register 1 - mono emulation */
#define PEL_D   0x3C9		/* PEL Data Register */
#define FCR_R   0x3CA		/* Feature Control Read */
#define FCR_WM  0x3BA		/* Feature Control Write Mono */
#define FCR_WC  0x3DA		/* Feature Control Write Color */
#define PEL_M   0x3C6		/* Palette MASK                */
#define GR1_P   0x3CC		/* Graphics 1                  */
#define GR2_P   0x3CA		/* Graphics 2                  */

/* VGA indexes max counts */
#define CRT_C   24		/* 24 CRT Controller Registers */
#define ATT_C   21		/* 21 Attribute Controller Registers */
#define GRA_C   9		/* 9  Graphics Controller Registers */
#define SEQ_C   5		/* 5  Sequencer Registers */
#define MIS_C   1		/* 1  Misc Output Register */
#define ISR1_C  1		/* 1  ISR reg */
#define GRAI_C  1		/* 1  GRAphic Index */
#define CRTI_C  1		/* 1  CRT Index */
#define SEQI_C  1		/* 1  SEQ Index */
#define FCR_C   1		/* 1  Feature Control Register */
#define ISR0_C  1		/* 1  Input Status Register 0  */
#define PELIR_C 1		/* 1  Pallette Read Index */
#define PELIW_C 1		/* 1  Pallette Write Index */
#define PELM_C  1		/* 1  Pallette MASK        */
#define GR1P_C  1		/* 1  Pallette MASK        */
#define GR2P_C  1		/* 1  Pallette MASK        */

/* VGA registers saving indexes */
#define CRT     0		/* CRT Controller Registers start */
#define ATT     CRT+CRT_C	/* Attribute Controller Registers start */
#define GRA     ATT+ATT_C	/* Graphics Controller Registers start */
#define SEQ     GRA+GRA_C	/* Sequencer Registers */
#define MIS     SEQ+SEQ_C	/* General Registers */
#define ISR1    MIS+MIS_C	/* SVGA Extended Registers */
#define GRAI    ISR1+ISR1_C	/* SVGA Extended Registers */
#define CRTI    GRAI+GRAI_C
#define SEQI    CRTI+CRTI_C
#define FCR     SEQI+SEQI_C
#define ISR0    FCR+FCR_C
#define PELIR   ISR0+ISR0_C
#define PELIW   PELIR+PELIR_C
#define PELM    PELIW+PELIW_C
#define GR1P    PELM+PELM_C
#define GR2P    GR1P+GR1P_C

#define SEG_SELECT 0x3CD
#define MAX_REGS 100
#define TEXT       0

extern __inline__ void allow_switch(void);
extern void dump_video_linux(void);
extern void set_vc_screen_page(int);
extern void get_video_ram(int);
extern void clear_screen(int, int);
extern void set_linux_video(void);
extern void put_video_ram(void);

extern int get_perm(void);
extern int release_perm(void);
extern int set_regs(unsigned char regs[]);
extern void open_vga_mem(void);
extern void close_vga_mem(void);

extern unsigned char video_initialized;
extern void vga_initialize(void);

#define MAX_S_REGS	71
#define MAX_X_REGS	30
#define MAX_X_REGS16	10

/* Struct to hold necessary elements during a save/restore */
struct video_save_struct {
  unsigned char regs[MAX_S_REGS];	/* These are the standard VGA-regs */
  unsigned char xregs[MAX_X_REGS];	/* These are EXT regs */
  unsigned short xregs16[MAX_X_REGS16]; /* These are 16-bit EXT regs */
  unsigned char *mem;
  unsigned char pal[3 * 256];
  unsigned char save_mem_size[4];
  unsigned char banks;
  unsigned char video_mode;
  unsigned char *video_name;	/* Debugging only */
  unsigned char release_video;
  unsigned char *textmem;	/* for saving page 0 memory */
};
extern struct video_save_struct linux_regs, dosemu_regs;
extern void save_vga_state(struct video_save_struct *save_regs);
extern void restore_vga_state(struct video_save_struct *save_regs);
extern void load_vga_font(unsigned char);

extern void vga_blink(unsigned char blink);

/*
extern int save_vga_regs();
extern int restore_vga_regs();
*/

struct dinfo {
  int xdim;
  int ydim;
  int colors;
  int xbytes;
  unsigned char mode;
  int buffer;
  unsigned char bpc;
  int seg;
  int off;
};
extern struct dinfo cur_info;

extern int vga_setmode(int mode);
extern int vga_hasmode(int mode);

extern int vga_clear(void);
extern int vga_newscreen(void);

extern int save_vga_setmode(void);
extern int restore_vga_setmode(void);

extern int vga_getxdim(void);
extern int vga_getydim(void);
extern int vga_getcolors(void);

extern int vga_setpalette(int index, int red, int green, int blue);
extern int vga_getpalette(int index, int *red, int *green, int *blue);
extern int vga_setpalvec(int start, int num, uchar * pal);
extern int vga_getpalvec(int start, int num, uchar * pal);

extern int vga_screenoff(void);
extern int vga_screenon(void);

extern int vga_setcolor(int color);
extern int vga_drawpixel(int x, int y);
extern int vga_drawline(int x1, int y1, int x2, int y2);
extern int vga_drawscanline(int line, unsigned char *colors);
extern int vga_drawscansegment(unsigned char *colors, int x, int y, int length);

extern int vga_dumpregs(void);

/* Various defines for all common video adapters */

#define CARD_VGA	1
#define CARD_EGA	2
#define CARD_CGA	3
#define CARD_MDA	4

/* Defines for Monochrome Display Adapter */

#define MDA_INIT_SCREEN_MODE   7 /* 80x25 MDA monochrome */
#define MDA_CONF_SCREEN_MODE   (3<<4)	/* for int 11h info     */
#define MDA_VIDEO_COMBO        1
#define MDA_VIDEO_SUBSYS       1 /* 1=mono               */
/* #define BASE_CRTC               0x3b4  currently not used */

/* Defines for Color Graphics Adapter */

#define CGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define CGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA, 0=EGA/VGA */
#define CGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok??) */
#define CGA_VIDEO_SUBSYS       0 /* 0=color */
/* #define BASE_CRTC               0x3d4  currently not used */

/* Defines for Enhanched Graphics Adapter, same as CGA */

#define EGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define EGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA, 0=EGA/VGA */
#define EGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok??) */
#define EGA_VIDEO_SUBSYS       0 /* 0=color */
/* #define BASE_CRTC               0x3d4  currently not used */

/* Defines for Video Graphic Array, same as CGA */

#define VGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define VGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA, 0=EGA/VGA */
#define VGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok??) */
#define VGA_VIDEO_SUBSYS       0 /* 0=color */
/* #define BASE_CRTC               0x3d4  currently not used */

/* Values are set from emu.c depending on video-card defined in config */

extern int virt_text_base;
extern int phys_text_base;
extern int video_combo;
extern int video_subsys;

#define PLAINVGA	0
#define TRIDENT		1
#define ET4000		2
#define DIAMOND		3
#define S3		4

extern u_char video_port_in(int port);
extern void video_port_out(u_char value, int port);

extern void install_int_10_handler(void);

extern int CRT_I, CRT_D, IS1_R, FCR_W, color_text;
extern u_char att_d_index;
extern u_char permissions;
extern struct screen_stat scr_state;
extern int cursor_row;
extern int cursor_col;

#endif 
/* End of include/video.h */
