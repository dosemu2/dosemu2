/*
 * Declarations needed to map VGA video modes to XImages
 * included by X.c, XModeRemap*.c
 * -- sw
 */

/*
 * define to have the the names of remap functions
 * as (char *) available
 * -- sw
 */
#define REMAP_FUNC_SHOW

/* use bitmasks so a function can handle more than one mode */
#define VGA_PSEUDO_1	(1 << 0)
#define VGA_PSEUDO_2	(1 << 1)
#define VGA_PSEUDO_4	(1 << 2)
#define VGA_PSEUDO_8	(1 << 3)
#define VGA_TRUE_15	(1 << 4)
#define VGA_TRUE_16	(1 << 5)
#define VGA_TRUE_24	(1 << 6)
#define VGA_TRUE_32	(1 << 7)
#define VGA_MODE_X	(1 << 8)

#define X_PSEUDO_8P	(1 << 0)
#define X_PSEUDO_8S	(1 << 1)
#define X_TRUE_1	(1 << 2)
#define X_TRUE_15	(1 << 3)
#define X_TRUE_16	(1 << 4)
#define X_TRUE_32	(1 << 5)
#define X_UNSUP		(1 << 31)

#ifdef REMAP_FUNC_SHOW
  #define REMAP_REC(SX, SY, VGA, X, F) {SX, SY, VGA, X, F, #F, NULL}
#else
  #define REMAP_REC(SX, SY, VGA, X, F) {SX, SY, VGA, X, F, NULL}
#endif

typedef struct XModeInfoStruct {
  /* do not change the declaration order -- sw */
  unsigned ScaleX, ScaleY;
  unsigned VGAWidth, VGAHeight, VGAScanLen;
  unsigned XWidth, XHeight, XScanLen;
  int VGAX0, VGAY0, VGAX1, VGAY1;
  int XX0, XY0, XX1, XY1;
  int VGAOffset;
  unsigned char *VGAScreen, *XScreen;
  unsigned char *lut, *lut2;
  unsigned *Color8Lut;
  void (*RemapFunc)(struct XModeInfoStruct *);
#ifdef REMAP_FUNC_SHOW
  char *RemapFuncName;
#endif
} XModeInfoType;


typedef struct XModeRemapRecordStruct {
  unsigned ScaleX, ScaleY;
  unsigned VGAMode;
  unsigned XMode;
  void (*RemapFunc)(struct XModeInfoStruct *);
#ifdef REMAP_FUNC_SHOW
  char *RemapFuncName;
#endif
  struct XModeRemapRecordStruct *Next;
} XModeRemapRecord;


/*
 *
 * these return a chained list of XModeRemapRecords
 * last link points to NULL
 *
 */
XModeRemapRecord *XModeRemap_i386(void);
XModeRemapRecord *XModeRemapGeneric(void);

