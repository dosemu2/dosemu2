#include <stdio.h>		/* definition of NULL*/
#include "XModeRemap.h"

#if 0
static void Opt1x8to1x8p(XModeInfoType *);
static void Opt1x8to1x8s(XModeInfoType *);
static void Opt1x8to2x8p(XModeInfoType *);
static void Opt1x8to2x8s(XModeInfoType *);
#endif
static void Opt1x8to1x16(XModeInfoType *);
static void Opt1x8to2x16(XModeInfoType *);
#if 0
static void Opt1x8to1x32(XModeInfoType *);
static void Opt1x8to2x32(XModeInfoType *);
#endif

#define REMAP_FUNCTIONS 2

/* put all functions into this array */
static XModeRemapRecord RemapOptList[REMAP_FUNCTIONS] = {
#if 0
REMAP_REC(1, 1, VGA_PSEUDO_8, X_PSEUDO_8P, Opt1x8to1x8p),
REMAP_REC(1, 1, VGA_PSEUDO_8, X_PSEUDO_8S, Opt1x8to1x8s),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_PSEUDO_8P, Opt1x8to2x8p),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_PSEUDO_8S, Opt1x8to2x8s),
#endif
REMAP_REC(1, 1, VGA_PSEUDO_8, X_TRUE_15 | X_TRUE_16, Opt1x8to1x16),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_TRUE_15 | X_TRUE_16, Opt1x8to2x16),
#if 0
REMAP_REC(1, 1, VGA_PSEUDO_8, X_TRUE_32, Opt1x8to1x32),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_TRUE_32, Opt1x8to2x32)
#endif
};


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * returns chained list of modes
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
XModeRemapRecord *XModeRemap_i386()
{
  int i;

  /* chain them together */
  for(i = 0; i < REMAP_FUNCTIONS - 1; i++) {
    RemapOptList[i].Next = RemapOptList + i + 1;
  }

  return RemapOptList;
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 15/16 bit true color
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
static void Opt1x8to1x16(XModeInfoType *xmi)
{
  int j, l, sl, vsl;
  unsigned char *src;
  unsigned short *dst2;

  src = xmi->VGAScreen + xmi->VGAOffset;
  dst2 = (unsigned short *) xmi->XScreen + xmi->XY0 * xmi->XScanLen + xmi->XX0;
  l = (xmi->VGAX1 - xmi->VGAX0);
  sl = xmi->XScanLen;
  vsl = xmi->VGAScanLen;
  j = xmi->XY1 - xmi->XY0;
  
  if ( (sl == vsl)  && ((l == sl) || (j == 1)) ) {
    /* we can process one whole chunk */
    l *= j;
    __asm__("
	cld
      1:
	lodsb
	movzbl	%%al,%%eax
	movw	(%0,%%eax,4),%%ax
	stosw
	loop	1b
    ": : "r" ((int)xmi->Color8Lut), "c" ((int)l),
         "D" ((int)dst2), "S" ((int)src)
     : "ax", "cx", "di", "si"
    );
    return;
  }

  for(; j > 0;  j--) {
#if 0
    int i;
    for(i = 0; i < l; i++) {
      dst2[i] = xmi->Color8Lut[src[i]];
    }
#else
    __asm__("
	cld
      1:
	lodsb
	movzbl	%%al,%%eax
	movw	(%0,%%eax,4),%%ax
	stosw
	loop	1b
    ": : "r" ((int)xmi->Color8Lut), "c" ((int)l),
         "D" ((int)dst2), "S" ((int)src)
     : "ax", "cx", "di", "si"
    );
#endif
    dst2 += sl;
    src += vsl;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 15/16 bit true color, scaled 2x2
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Opt1x8to2x16(XModeInfoType *xmi)
{
  int j, l, sl, vsl;
  unsigned char *src;
  unsigned *dst4;

  src = xmi->VGAScreen + xmi->VGAOffset;
  sl = xmi->XScanLen;
  vsl = xmi->VGAScanLen;
  dst4 = (unsigned *) xmi->XScreen + xmi->XY0 * xmi->XScanLen * 2 + xmi->XX0;

  l = (xmi->VGAX1 - xmi->VGAX0);
  
  for(j= xmi->XY1 - xmi->XY0; j >0; j--) {
#if 0
    int i;
    for(i = 0; i < l; i++) {
      dst4[i] =
      dst4[i + sl] = xmi->Color8Lut[src[i]];
    }
#else
    __asm__("
	cld
      1:
	lodsb
	movzbl	%%al,%%eax
	movl	(%0,%%eax,4),%%eax
	movl	%%eax,(%%edi,%%edx,1)
	stosl
	loop	1b
    ": : "r" ((int)xmi->Color8Lut), "c" ((int)l), "d" ((int)sl<<2),
         "D" ((int)dst4), "S" ((int)src)
     : "ax", "cx", "di", "si"
    );
#endif
    dst4 += sl + sl;
    src += vsl;
  }
}

