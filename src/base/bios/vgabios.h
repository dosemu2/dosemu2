#ifndef vgabios_h_included
#define vgabios_h_included

void vgaemu_get_cursor_pos(Bit8u page,Bit16u *shape,Bit16u *pos);
void vgaemu_scroll(int x0, int y0, int x1, int y1, int n, unsigned char attr);
void vgaemu_put_char(unsigned char c, unsigned char page, unsigned char attr);
void vgaemu_repeat_char(unsigned char c, unsigned char page,
    unsigned char attr, int count);
void vgaemu_repeat_char_attr(unsigned char c, unsigned char page,
    unsigned char attr, int count);
void vgaemu_put_pixel(int x, int y, unsigned char page, unsigned char attr);
unsigned char vgaemu_get_pixel(int x, int y, unsigned char page);

/* Types */
#if 0
typedef unsigned char  Bit8u;
typedef unsigned short Bit16u;
typedef unsigned long  Bit32u;
typedef unsigned short Boolean;
#endif

/* Defines */

#define SET_AL(val8) AX = ((AX & 0xff00) | (val8))
#define SET_BL(val8) BX = ((BX & 0xff00) | (val8))
#define SET_CL(val8) CX = ((CX & 0xff00) | (val8))
#define SET_DL(val8) DX = ((DX & 0xff00) | (val8))
#define SET_AH(val8) AX = ((AX & 0x00ff) | ((val8) << 8))
#define SET_BH(val8) BX = ((BX & 0x00ff) | ((val8) << 8))
#define SET_CH(val8) CX = ((CX & 0x00ff) | ((val8) << 8))
#define SET_DH(val8) DX = ((DX & 0x00ff) | ((val8) << 8))

#define GET_AL() ( AX & 0x00ff )
#define GET_BL() ( BX & 0x00ff )
#define GET_CL() ( CX & 0x00ff )
#define GET_DL() ( DX & 0x00ff )
#define GET_AH() ( AX >> 8 )
#define GET_BH() ( BX >> 8 )
#define GET_CH() ( CX >> 8 )
#define GET_DH() ( DX >> 8 )

#define SET_CF()     FLAGS |= 0x0001
#define CLEAR_CF()   FLAGS &= 0xfffe
#define GET_CF()     (FLAGS & 0x0001)

#define SET_ZF()     FLAGS |= 0x0040
#define CLEAR_ZF()   FLAGS &= 0xffbf
#define GET_ZF()     (FLAGS & 0x0040)

#define SCROLL_DOWN 0
#define SCROLL_UP   1
#define NO_ATTR     2
#define WITH_ATTR   3

#endif
