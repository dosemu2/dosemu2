/*
 * Here are the remap functions written in plain C,
 * specially optimized versions should go into their own files.
 *
 * They are provided as a fallback, in case there are no optimized
 * versions available for a particular configuration.
 *
 * -- sw
 */

#include <stdio.h>		/* definition of NULL*/

#include "XModeRemap.h"

void Gen1x8to1x8p(XModeInfoType *);
void Gen1x8to1x8s(XModeInfoType *);
void Gen1x8to2x8p(XModeInfoType *);
void Gen1x8to2x8s(XModeInfoType *);
void Gen1x8to1x16(XModeInfoType *);
void Gen1x8to2x16(XModeInfoType *);
void Gen1x8to1x32(XModeInfoType *);
void Gen1x8to2x32(XModeInfoType *);

#define REMAP_FUNCTIONS 8

/* put all functions into this array */
static XModeRemapRecord RemapGenericList[REMAP_FUNCTIONS] = {
REMAP_REC(1, 1, VGA_PSEUDO_8, X_PSEUDO_8P, Gen1x8to1x8p),
REMAP_REC(1, 1, VGA_PSEUDO_8, X_PSEUDO_8S, Gen1x8to1x8s),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_PSEUDO_8P, Gen1x8to2x8p),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_PSEUDO_8S, Gen1x8to2x8s),
REMAP_REC(1, 1, VGA_PSEUDO_8, X_TRUE_15 | X_TRUE_16, Gen1x8to1x16),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_TRUE_15 | X_TRUE_16, Gen1x8to2x16),
REMAP_REC(1, 1, VGA_PSEUDO_8, X_TRUE_32, Gen1x8to1x32),
REMAP_REC(2, 2, VGA_PSEUDO_8, X_TRUE_32, Gen1x8to2x32)
};

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * returns chained list of modes
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
XModeRemapRecord *XModeRemapGeneric()
{
  int i;

  /* chain them together */
  for(i = 0; i < REMAP_FUNCTIONS - 1; i++) {
    RemapGenericList[i].Next = RemapGenericList + i + 1;
  }

  return RemapGenericList;
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 8 bit pseudo color, private color map
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to1x8p(XModeInfoType *xmi)
{
  int i, j, l;
  unsigned char *src, *dst;
  unsigned *src4, *dst4;

  src = xmi->VGAScreen + xmi->VGAOffset;
  dst = xmi->XScreen + xmi->XY0 * xmi->XScanLen + xmi->XX0;
  l = (xmi->VGAX1 - xmi->VGAX0) >> 2;

  src4 = (unsigned *) src; dst4 = (unsigned *) dst;

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst4[i] = src4[i];
    }
    dst4 += xmi->XScanLen >> 2;
    src4 += xmi->VGAScanLen >> 2;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 8 bit pseudo color, shared color map
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to1x8s(XModeInfoType *xmi)
{
  int i, j, l;
  unsigned char *src, *dst;

  src = xmi->VGAScreen + xmi->VGAOffset;
  dst = xmi->XScreen + xmi->XY0 * xmi->XScanLen + xmi->XX0;
  l = xmi->VGAX1 - xmi->VGAX0;

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    if(j & 1) {
      for(i = 0; i < l; i += 2) {
        dst[i] = xmi->lut[src[i]];
        dst[i + 1] = xmi->lut2[src[i + 1]];
      }
    }
    else {
      for(i = 0; i < l; i += 2) {
        dst[i] = xmi->lut2[src[i]];
        dst[i + 1] = xmi->lut[src[i + 1]];
      }
    }
    dst += xmi->XScanLen;
    src += xmi->VGAScanLen;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 8 bit pseudo color, scaled 2x2, private color map
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to2x8p(XModeInfoType *xmi)
{
  int i, j, l, sl;
  unsigned char *src, *dst;

  src = xmi->VGAScreen + xmi->VGAOffset;
  sl = xmi->XScanLen << 1;
  dst = xmi->XScreen + (xmi->XY0 * sl + xmi->XX0) * 2;
  l = (xmi->VGAX1 - xmi->VGAX0);

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst[2 * i] = 
      dst[2 * i + 1] = 
      dst[2 * i + sl] = 
      dst[2 * i + 1 + sl] = src[i];
    }
    dst += sl + sl;
    src += xmi->VGAScanLen;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 8 bit pseudo color, scaled 2x2, shared color map
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to2x8s(XModeInfoType *xmi)
{
  int i, j, l, sl;
  unsigned char *src, *dst;

  src = xmi->VGAScreen + xmi->VGAOffset;
  sl = xmi->XScanLen << 1;
  dst = xmi->XScreen + (xmi->XY0 * sl + xmi->XX0) * 2;
  l = (xmi->VGAX1 - xmi->VGAX0);

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst[2 * i] = 
      dst[2 * i + 1 + sl] = xmi->lut[src[i]];
      dst[2 * i + 1] = 
      dst[2 * i + sl] = xmi->lut2[src[i]];
    }
    dst += sl + sl;
    src += xmi->VGAScanLen;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 15/16 bit true color
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to1x16(XModeInfoType *xmi)
{
  int i, j, l;
  unsigned char *src;
  unsigned short *dst2;

  src = xmi->VGAScreen + xmi->VGAOffset;
  dst2 = (unsigned short *) xmi->XScreen + xmi->XY0 * xmi->XScanLen + xmi->XX0;
  l = (xmi->VGAX1 - xmi->VGAX0);

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst2[i] = xmi->Color8Lut[src[i]];
    }
    dst2 += xmi->XScanLen;
    src += xmi->VGAScanLen;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 15/16 bit true color, scaled 2x2
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to2x16(XModeInfoType *xmi)
{
  int i, j, l, sl;
  unsigned char *src;
  unsigned *dst4;

  src = xmi->VGAScreen + xmi->VGAOffset;
  sl = xmi->XScanLen;
  dst4 = (unsigned *) xmi->XScreen + xmi->XY0 * xmi->XScanLen * 2 + xmi->XX0;

  l = (xmi->VGAX1 - xmi->VGAX0);

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst4[i] =
      dst4[i + sl] = xmi->Color8Lut[src[i]];
    }
    dst4 += sl + sl;
    src += xmi->VGAScanLen;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 32 bit true color
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to1x32(XModeInfoType *xmi)
{
  int i, j, l;
  unsigned char *src;
  unsigned *dst4;

  src = xmi->VGAScreen + xmi->VGAOffset;
  dst4 = (unsigned *) xmi->XScreen + xmi->XY0 * xmi->XScanLen + xmi->XX0;
  l = (xmi->VGAX1 - xmi->VGAX0);

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst4[i] = xmi->Color8Lut[src[i]];
    }
    dst4 += xmi->XScanLen;
    src += xmi->VGAScanLen;
  }
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * 8 bit pseudo color --> 32 bit true color, scaled 2x2
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void Gen1x8to2x32(XModeInfoType *xmi)
{
  int i, j, l, sl;
  unsigned char *src;
  unsigned *dst4;

  src = xmi->VGAScreen + xmi->VGAOffset;
  sl = xmi->XScanLen << 1;
  dst4 = (unsigned *) xmi->XScreen + (xmi->XY0 * sl + xmi->XX0) * 2;

  l = (xmi->VGAX1 - xmi->VGAX0);

  for(j = xmi->XY0; j < xmi->XY1; j++) {
    for(i = 0; i < l; i++) {
      dst4[2 * i] =
      dst4[2 * i + 1] =
      dst4[2 * i + sl] =
      dst4[2 * i + sl + 1] = xmi->Color8Lut[src[i]];
    }
    dst4 += sl + sl;
    src += xmi->VGAScanLen;
  }
}


