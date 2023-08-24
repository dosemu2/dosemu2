#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "cpu.h"
#include "memory.h"

/* Define as type to be equivalent bitsize as float */
#define float FxU32

#include <sdk2_glide.h>
#include "glide2x.h"

#define DOSEMU

extern int build_posix_path(char *dest, const char *src, int allowwildcards);
extern unsigned long SEL_ADR(unsigned short sel, unsigned long reg);

#define SDK_GRGLIDEINIT                  grGlideInit
#define SDK_GRSSTQUERYHARDWARE           grSstQueryHardware
#define SDK_GRSSTSELECT                  grSstSelect
#define SDK_GRSSTWINOPEN                 grSstWinOpen
#define SDK_GRDEPTHBUFFERMODE            grDepthBufferMode
#define SDK_GRDEPTHBUFFERFUNCTION        grDepthBufferFunction
#define SDK_GRDEPTHMASK                  grDepthMask
#define SDK_GRTEXCOMBINEFUNCTION         grTexCombineFunction
#define SDK_GRCONSTANTCOLORVALUE         grConstantColorValue
#define SDK_GUALPHASOURCE                guAlphaSource
#define SDK_GRCHROMAKEYMODE              grChromakeyMode
#define SDK_GRCHROMAKEYVALUE             grChromakeyValue
#define SDK_GRGAMMACORRECTIONVALUE       grGammaCorrectionValue
#define SDK_GRTEXDOWNLOADTABLE           grTexDownloadTable
#define SDK_GUTEXMEMRESET                guTexMemReset
#define SDK_GU3DFGETINFO                 gu3dfGetInfo
#define SDK_GU3DFLOAD                    gu3dfLoad
#define SDK_GUTEXALLOCATEMEMORY          guTexAllocateMemory
#define SDK_GUTEXDOWNLOADMIPMAP          guTexDownloadMipMap
#define SDK_GRBUFFERCLEAR                grBufferClear
#define SDK_GUCOLORCOMBINEFUNCTION       guColorCombineFunction
#define SDK_GUTEXSOURCE                  guTexSource
#define SDK_GRDRAWPOLYGONVERTEXLIST      grDrawPolygonVertexList
#define SDK_GRBUFFERSWAP                 grBufferSwap
#define SDK_GRTEXFILTERMODE              grTexFilterMode
#define SDK_GRSSTWINCLOSE                grSstWinClose
#define SDK_GRDEPTHBIASLEVEL             grDepthBiasLevel
#define SDK_GRCOLORCOMBINE               grColorCombine
#define SDK_GRALPHABLENDFUNCTION         grAlphaBlendFunction
#define SDK_GRDRAWLINE                   grDrawLine
#define SDK_GRCONSTANTCOLORVALUE4        grConstantColorValue4
#define SDK_GRGLIDESHUTDOWN              grGlideShutdown
#define SDK_GRAADRAWLINE                 grAADrawLine
#define SDK_GRRENDERBUFFER               grRenderBuffer
#define SDK_GRTEXDOWNLOADMIPMAP          grTexDownloadMipMap
#define SDK_GRTEXSOURCE                  grTexSource
#define SDK_GRDRAWTRIANGLE               grDrawTriangle
#define SDK_GRDISABLEALLEFFECTS          grDisableAllEffects
#define SDK_GRTEXMIPMAPMODE              grTexMipMapMode
#define SDK_GRTEXLODBIASVALUE            grTexLodBiasValue
#define SDK_GRTEXCLAMPMODE               grTexClampMode
#define SDK_GRALPHACOMBINE               grAlphaCombine
#define SDK_GRFOGMODE                    grFogMode
#define SDK_GRALPHATESTFUNCTION          grAlphaTestFunction
#define SDK_GRCLIPWINDOW                 grClipWindow
#define SDK_GRCULLMODE                   grCullMode
#define SDK_GRFOGCOLORVALUE              grFogColorValue
#define SDK_GRFOGTABLE                   grFogTable
#define SDK_GRCOLORMASK                  grColorMask
#define SDK_GRLFBLOCK                    grLfbLock
#define SDK_GRTEXDETAILCONTROL           grTexDetailControl
#define SDK_GRHINTS                      grHints
#define SDK_GUDRAWTRIANGLEWITHCLIP       guDrawTriangleWithClip
#define SDK_GUFOGGENERATEEXP             guFogGenerateExp
#define SDK_GRTEXCOMBINE                 grTexCombine
#define SDK_GRBUFFERNUMPENDING           grBufferNumPending
#define SDK_GRGLIDEGETSTATE              grGlideGetState
#define SDK_GRGLIDEGETVERSION            grGlideGetVersion
#define SDK_GRGLIDESHAMELESSPLUG         grGlideShamelessPlug
#define SDK_GRLFBUNLOCK                  grLfbUnlock
#define SDK_GRSSTSTATUS                  grSstStatus
#define SDK_GRTEXCALCMEMREQUIRED         grTexCalcMemRequired
#define SDK_GUTEXCOMBINEFUNCTION         guTexCombineFunction
#define SDK_GRTEXDOWNLOADMIPMAPLEVEL     grTexDownloadMipMapLevel
#define SDK_GRTEXMAXADDRESS              grTexMaxAddress
#define SDK_GRTEXMINADDRESS              grTexMinAddress
#define SDK_GUTEXMEMQUERYAVAIL           guTexMemQueryAvail
#define SDK_GRTEXTEXTUREMEMREQUIRED      grTexTextureMemRequired
#define SDK_GRSPLASH                     grSplash

#define THUNK(func) THUNK_##func

#define ENDDECLARE }}

#define DECLARE_THUNK0(func,ret) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (void) = &SDK_##func; \
{ printf (__func__);

#define DECLARE_THUNK1(func,ret,t1,v1) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
    } { printf (__func__);

#define DECLARE_THUNK2(func,ret,t1,v1,t2,v2) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
    } { printf (__func__);

#define DECLARE_THUNK3(func,ret,t1,v1,t2,v2,t3,v3) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
    } { printf (__func__);

#define DECLARE_THUNK4(func,ret,t1,v1,t2,v2,t3,v3,t4,v4) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3, t4) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; t4 v4; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
        v4 = (t4) DWORD(THUNK_stack[3]); \
    } { printf (__func__);

#define DECLARE_THUNK5(func,ret,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3, t4, t5) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; t4 v4; t5 v5; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
        v4 = (t4) DWORD(THUNK_stack[3]); \
        v5 = (t5) DWORD(THUNK_stack[4]); \
    } { printf (__func__);

#define DECLARE_THUNK6(func,ret,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3, t4, t5, t6) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; t4 v4; t5 v5; t6 v6; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
        v4 = (t4) DWORD(THUNK_stack[3]); \
        v5 = (t5) DWORD(THUNK_stack[4]); \
        v6 = (t6) DWORD(THUNK_stack[5]); \
    } { printf (__func__);

#define DECLARE_THUNK7(func,ret,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6,t7,v7) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3, t4, t5, t6, t7) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; t4 v4; t5 v5; t6 v6; t7 v7; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
        v4 = (t4) DWORD(THUNK_stack[3]); \
        v5 = (t5) DWORD(THUNK_stack[4]); \
        v6 = (t6) DWORD(THUNK_stack[5]); \
        v7 = (t7) DWORD(THUNK_stack[6]); \
    } { printf (__func__);

#define DECLARE_THUNK8(func,ret,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6,t7,v7,t8,v8) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3, t4, t5, t6, t7, t8) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; t4 v4; t5 v5; t6 v6; t7 v7; t8 v8; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
        v4 = (t4) DWORD(THUNK_stack[3]); \
        v5 = (t5) DWORD(THUNK_stack[4]); \
        v6 = (t6) DWORD(THUNK_stack[5]); \
        v7 = (t7) DWORD(THUNK_stack[6]); \
        v8 = (t8) DWORD(THUNK_stack[7]); \
    } { printf (__func__);

#define DECLARE_THUNK15(func,ret,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6,t7,v7,t8,v8,t9,v9,t10,v10,t11,v11,t12,v12,t13,v13,t14,v14,t15,v15) \
static void THUNK(func)(cpuctx_t *scp) { \
    static ret FX_CALL (*THUNK_sdk) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15) = &SDK_##func; \
    unsigned long THUNK_offset = (unsigned long)LINEAR2UNIX(SEL_ADR(_ss, _esp)) - _esp; \
    t1 v1; t2 v2; t3 v3; t4 v4; t5 v5; t6 v6; t7 v7; t8 v8; t9 v9; \
    t10 v10; t11 v11; t12 v12; t13 v13; t14 v14; t15 v15; \
    {   unsigned long *THUNK_stack = (unsigned long *)_esp + 2; \
        __faddr(THUNK_stack); \
        v1 = (t1) DWORD(THUNK_stack[0]); \
        v2 = (t2) DWORD(THUNK_stack[1]); \
        v3 = (t3) DWORD(THUNK_stack[2]); \
        v4 = (t4) DWORD(THUNK_stack[3]); \
        v5 = (t5) DWORD(THUNK_stack[4]); \
        v6 = (t6) DWORD(THUNK_stack[5]); \
        v7 = (t7) DWORD(THUNK_stack[6]); \
        v8 = (t8) DWORD(THUNK_stack[7]); \
        v9 = (t9) DWORD(THUNK_stack[8]); \
        v10 = (t10) DWORD(THUNK_stack[9]); \
        v11 = (t11) DWORD(THUNK_stack[10]); \
        v12 = (t12) DWORD(THUNK_stack[11]); \
        v13 = (t13) DWORD(THUNK_stack[12]); \
        v14 = (t14) DWORD(THUNK_stack[13]); \
        v15 = (t15) DWORD(THUNK_stack[14]); \
    } { printf (__func__);

#define RETURNI(v) _eax = v;

#define __faddr(addr) asm volatile ("mov %1, %%eax; add %%eax, %0": "=g"(addr): "m"(THUNK_offset), "0"(addr): "%eax")

#define __fcall(...) \
    (*THUNK_sdk) (__VA_ARGS__)

#include "thunks.i"
#include "../../../src/dosext/dpmi/vxd.h"

static struct vxd_client vxd;
static void (*thunks[CMD_MAX]) (cpuctx_t *scp);


static void openglide_plugin_entry (cpuctx_t *scp)
{
    unsigned char offset = (unsigned char) _eax;

    if ((_eax < CMD_MAX) && thunks[offset])
    {
        //fprintf(stderr, "PLUGIN: openglide_plugin_entry called %u\n", offset);
        (*thunks[offset])(scp);
	printf ("-done\n");
        return;
    }

    if (_eax >= CMD_MAX)
        fprintf(stderr, "PLUGIN: openglide_plugin_entry cmd out of range %lu (max %u)\n", _eax, CMD_MAX);
    else
        fprintf(stderr, "PLUGIN: openglide_plugin_entry cmd %u undefined\n", offset);
}

void openglide_plugin_init(void)
{
    memcpy (vxd.name, "OPENGLD1", sizeof (vxd.name));
    vxd.bx    = 0;
    vxd.entry = &openglide_plugin_entry;

    memset (thunks, 0, sizeof (thunks));
    /* Configure thunks (Note this can be removed when
     * thunks can be directly registered and called */
    thunks[CMD_GRGLIDEINIT]               = &THUNK(GRGLIDEINIT);
    thunks[CMD_GRSSTQUERYHARDWARE]        = &THUNK(GRSSTQUERYHARDWARE);
    thunks[CMD_GRSSTSELECT]               = &THUNK(GRSSTSELECT);
    thunks[CMD_GRSSTWINOPEN]              = &THUNK(GRSSTWINOPEN);
    thunks[CMD_GRDEPTHBUFFERMODE]         = &THUNK(GRDEPTHBUFFERMODE);
    thunks[CMD_GRDEPTHBUFFERFUNCTION]     = &THUNK(GRDEPTHBUFFERFUNCTION);
    thunks[CMD_GRDEPTHMASK]               = &THUNK(GRDEPTHMASK);
    thunks[CMD_GRTEXCOMBINEFUNCTION]      = &THUNK(GRTEXCOMBINEFUNCTION);
    thunks[CMD_GRCONSTANTCOLORVALUE]      = &THUNK(GRCONSTANTCOLORVALUE);
    thunks[CMD_GUALPHASOURCE]             = &THUNK(GUALPHASOURCE);
    thunks[CMD_GRCHROMAKEYMODE]           = &THUNK(GRCHROMAKEYMODE);
    thunks[CMD_GRCHROMAKEYVALUE]          = &THUNK(GRCHROMAKEYVALUE);
    thunks[CMD_GRGAMMACORRECTIONVALUE]    = &THUNK(GRGAMMACORRECTIONVALUE);
    thunks[CMD_GRTEXDOWNLOADTABLE]        = &THUNK(GRTEXDOWNLOADTABLE);
    thunks[CMD_GUTEXMEMRESET]             = &THUNK(GUTEXMEMRESET);
    thunks[CMD_GU3DFGETINFO]              = &THUNK(GU3DFGETINFO);
    thunks[CMD_GU3DFLOAD]                 = &THUNK(GU3DFLOAD);
    thunks[CMD_GUTEXALLOCATEMEMORY]       = &THUNK(GUTEXALLOCATEMEMORY);
    thunks[CMD_GUTEXDOWNLOADMIPMAP]       = &THUNK(GUTEXDOWNLOADMIPMAP);
    thunks[CMD_GRBUFFERCLEAR]             = &THUNK(GRBUFFERCLEAR);
    thunks[CMD_GUCOLORCOMBINEFUNCTION]    = &THUNK(GUCOLORCOMBINEFUNCTION);
    thunks[CMD_GUTEXSOURCE]               = &THUNK(GUTEXSOURCE);
    thunks[CMD_GRDRAWPOLYGONVERTEXLIST]   = &THUNK(GRDRAWPOLYGONVERTEXLIST);
    thunks[CMD_GRBUFFERSWAP]              = &THUNK(GRBUFFERSWAP);
    thunks[CMD_GRTEXFILTERMODE]           = &THUNK(GRTEXFILTERMODE);
    thunks[CMD_GRSSTWINCLOSE]             = &THUNK(GRSSTWINCLOSE);
    thunks[CMD_GRDEPTHBIASLEVEL]          = &THUNK(GRDEPTHBIASLEVEL);
    thunks[CMD_GRCOLORCOMBINE]            = &THUNK(GRCOLORCOMBINE);
    thunks[CMD_GRALPHABLENDFUNCTION]      = &THUNK(GRALPHABLENDFUNCTION);
    thunks[CMD_GRDRAWLINE]                = &THUNK(GRDRAWLINE);
    thunks[CMD_GRCONSTANTCOLORVALUE4]     = &THUNK(GRCONSTANTCOLORVALUE4);
    thunks[CMD_GRGLIDESHUTDOWN]           = &THUNK(GRGLIDESHUTDOWN);
    thunks[CMD_GRAADRAWLINE]              = &THUNK(GRAADRAWLINE);
    thunks[CMD_GRRENDERBUFFER]            = &THUNK(GRRENDERBUFFER);
    thunks[CMD_GRTEXDOWNLOADMIPMAP]       = &THUNK(GRTEXDOWNLOADMIPMAP);
    thunks[CMD_GRTEXSOURCE]               = &THUNK(GRTEXSOURCE);
    thunks[CMD_GRDRAWTRIANGLE]            = &THUNK(GRDRAWTRIANGLE);
    thunks[CMD_GRDISABLEALLEFFECTS]       = &THUNK(GRDISABLEALLEFFECTS);
    thunks[CMD_GRTEXMIPMAPMODE]           = &THUNK(GRTEXMIPMAPMODE);
    thunks[CMD_GRTEXLODBIASVALUE]         = &THUNK(GRTEXLODBIASVALUE);
    thunks[CMD_GRTEXCLAMPMODE]            = &THUNK(GRTEXCLAMPMODE);
    thunks[CMD_GRALPHACOMBINE]            = &THUNK(GRALPHACOMBINE);
    thunks[CMD_GRFOGMODE]                 = &THUNK(GRFOGMODE);
    thunks[CMD_GRALPHATESTFUNCTION]       = &THUNK(GRALPHATESTFUNCTION);
    thunks[CMD_GRCLIPWINDOW]              = &THUNK(GRCLIPWINDOW);
    thunks[CMD_GRCULLMODE]                = &THUNK(GRCULLMODE);
    thunks[CMD_GRFOGCOLORVALUE]           = &THUNK(GRFOGCOLORVALUE);
    thunks[CMD_GRFOGTABLE]                = &THUNK(GRFOGTABLE);
    thunks[CMD_GRCOLORMASK]               = &THUNK(GRCOLORMASK);
    thunks[CMD_GRLFBLOCK]                 = &THUNK(GRLFBLOCK);
    thunks[CMD_GRTEXDETAILCONTROL]        = &THUNK(GRTEXDETAILCONTROL);
    thunks[CMD_GRHINTS]                   = &THUNK(GRHINTS);
    thunks[CMD_GUDRAWTRIANGLEWITHCLIP]    = &THUNK(GUDRAWTRIANGLEWITHCLIP);
    thunks[CMD_GUFOGGENERATEEXP]          = &THUNK(GUFOGGENERATEEXP);
    thunks[CMD_GRTEXCOMBINE]              = &THUNK(GRTEXCOMBINE);
    thunks[CMD_GRBUFFERNUMPENDING]        = &THUNK(GRBUFFERNUMPENDING);
    thunks[CMD_GRGLIDEGETSTATE]           = &THUNK(GRGLIDEGETSTATE);
    thunks[CMD_GRGLIDEGETVERSION]         = &THUNK(GRGLIDEGETVERSION);
    thunks[CMD_GRGLIDESHAMELESSPLUG]      = &THUNK(GRGLIDESHAMELESSPLUG);
    thunks[CMD_GRLFBUNLOCK]               = &THUNK(GRLFBUNLOCK);
    thunks[CMD_GRSSTSTATUS]               = &THUNK(GRSSTSTATUS);
    thunks[CMD_GRTEXCALCMEMREQUIRED]      = &THUNK(GRTEXCALCMEMREQUIRED);
    thunks[CMD_GUTEXCOMBINEFUNCTION]      = &THUNK(GUTEXCOMBINEFUNCTION);
    thunks[CMD_GRTEXDOWNLOADMIPMAPLEVEL]  = &THUNK(GRTEXDOWNLOADMIPMAPLEVEL);
    thunks[CMD_GRTEXMAXADDRESS]           = &THUNK(GRTEXMAXADDRESS);
    thunks[CMD_GRTEXMINADDRESS]           = &THUNK(GRTEXMINADDRESS);
    thunks[CMD_GUTEXMEMQUERYAVAIL]        = &THUNK(GUTEXMEMQUERYAVAIL);
    thunks[CMD_GRTEXTEXTUREMEMREQUIRED]   = &THUNK(GRTEXTEXTUREMEMREQUIRED);
    thunks[CMD_GRSPLASH]                  = &THUNK(GRSPLASH);

    register_vxd_client (&vxd);

#if 0
    fprintf(stderr, "PLUGIN: openglide_plugin_init called\n");
#endif
}

void openglide_plugin_close(void)
{
#if 0
    fprintf(stderr, "PLUGIN: openglide_plugin_close called\n");
#endif
}
