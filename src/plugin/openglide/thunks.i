DECLARE_THUNK2(GU3DFGETINFO, FxBool, const char*, filename, Gu3dfInfo*, file_info)
    FxBool ret;
    __faddr (filename);
    __faddr (file_info);

#ifdef DOSEMU
    {
        char path[PATH_MAX];
        if (build_posix_path (path, filename, 0) < 0)
            ret = FXFALSE;
        else
            ret = __fcall (path, file_info);
    }
#else
    ret = __fcall (filename, file_info);
#endif

    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK2(GU3DFLOAD, FxBool, const char*, filename, Gu3dfInfo*, file_info)
    FxBool ret;
    __faddr (filename);
    __faddr (file_info);

#ifdef DOSEMU
    {
        char path[PATH_MAX];
        if (build_posix_path (path, filename, 0) < 0)
            ret = FXFALSE;
        else
            ret = __fcall (path, file_info);
    }
#else
    ret = __fcall (filename, file_info);
#endif

    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK2(GRAADRAWLINE, void, const GrVertex*, v1, const GrVertex*, v2)
    __faddr (v1);
    __faddr (v2);
    __fcall (v1, v2);
ENDDECLARE

DECLARE_THUNK1(GUALPHASOURCE, void, GrAlphaSource_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK4(GRALPHABLENDFUNCTION, void, GrAlphaBlendFnc_t, a, GrAlphaBlendFnc_t, b,
                                     GrAlphaBlendFnc_t, c, GrAlphaBlendFnc_t, d)
    __fcall (a, b, c, d);
ENDDECLARE

DECLARE_THUNK5(GRALPHACOMBINE, void, GrCombineFunction_t, a, GrCombineFactor_t, b,
                               GrCombineLocal_t, c, GrCombineOther_t, d, FxBool, e)
    __fcall (a, b, c, d, e);
ENDDECLARE

DECLARE_THUNK1(GRALPHATESTFUNCTION, void, GrCmpFnc_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK3(GRBUFFERCLEAR, void, GrColor_t, a, GrAlpha_t, b, FxU16, c)
    __fcall (a, b, c);
ENDDECLARE

DECLARE_THUNK0(GRBUFFERNUMPENDING, int)
    int ret;
    ret = __fcall ();
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK1(GRBUFFERSWAP, void, int, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRCHROMAKEYMODE, void, GrChromakeyMode_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRCHROMAKEYVALUE, void, GrColor_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK4(GRCLIPWINDOW, void, FxU32, a, FxU32, b, FxU32, c, FxU32, d)
    __fcall (a, b, c, d);
ENDDECLARE

DECLARE_THUNK5(GRCOLORCOMBINE, void, GrCombineFunction_t, a, GrCombineFactor_t, b,
                               GrCombineLocal_t, c, GrCombineOther_t, d, FxBool, e)
    __fcall (a, b, c, d, e);
ENDDECLARE

DECLARE_THUNK1(GUCOLORCOMBINEFUNCTION, void, GrColorCombineFnc_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK2(GRCOLORMASK, void, FxBool, a, FxBool, b)
    __fcall (a, b);
ENDDECLARE

DECLARE_THUNK1(GRCONSTANTCOLORVALUE, void, GrColor_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK4(GRCONSTANTCOLORVALUE4, void, float, a, float, b, float, c, float, d)
    __fcall (a, b, c, d);
ENDDECLARE

DECLARE_THUNK1(GRCULLMODE, void, GrCullMode_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRDEPTHBIASLEVEL, void, FxI16, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRDEPTHBUFFERFUNCTION, void, GrCmpFnc_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRDEPTHBUFFERMODE, void, GrDepthBufferMode_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRDEPTHMASK, void, FxBool, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK0(GRDISABLEALLEFFECTS, void)
    __fcall ();
ENDDECLARE

DECLARE_THUNK2(GRDRAWLINE, void, const GrVertex*, v1, const GrVertex*, v2)
    __faddr (v1);
    __faddr (v2);
    __fcall (v1, v2);
ENDDECLARE

DECLARE_THUNK2(GRDRAWPOLYGONVERTEXLIST, void, int, nverts, const GrVertex*, vlist)
    __faddr (vlist);
    __fcall (nverts, vlist);
ENDDECLARE

DECLARE_THUNK3(GRDRAWTRIANGLE, void, const GrVertex*, v1, const GrVertex*, v2,
                               const GrVertex*, v3)
    __faddr (v1);
    __faddr (v2);
    __faddr (v3);
    __fcall (v1, v2, v3);
ENDDECLARE

DECLARE_THUNK3(GUDRAWTRIANGLEWITHCLIP, void, const GrVertex*, v1, const GrVertex*, v2,
                                       const GrVertex*, v3)
    __faddr (v1);
    __faddr (v2);
    __faddr (v3);
    __fcall (v1, v2, v3);
ENDDECLARE

DECLARE_THUNK1(GRFOGCOLORVALUE, void, GrColor_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK2(GUFOGGENERATEEXP, void, GrFog_t*, table, float, density)
    __faddr (table);
    __fcall (table, density);
ENDDECLARE

DECLARE_THUNK1(GRFOGMODE, void, GrFogMode_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRFOGTABLE, void, const GrFog_t*, ft)
    __faddr (ft);
    __fcall (ft);
ENDDECLARE

DECLARE_THUNK1(GRGAMMACORRECTIONVALUE, void, float, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRGLIDEGETSTATE, void, GrState*, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRGLIDEGETVERSION, void, char*, version)
    __faddr (version);
    __fcall (version);
ENDDECLARE

DECLARE_THUNK0(GRGLIDEINIT, void)
    __fcall ();
ENDDECLARE

DECLARE_THUNK1(GRGLIDESHAMELESSPLUG, void, int, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK0(GRGLIDESHUTDOWN, void)
    __fcall ();
ENDDECLARE

DECLARE_THUNK2(GRHINTS, void, GrHint_t, a, FxU32, b)
    __fcall (a, b);
ENDDECLARE

DECLARE_THUNK6(GRLFBLOCK, FxBool,
                          GrLock_t,           type,
                          GrBuffer_t,         buffer,
                          GrLfbWriteMode_t,   writeMode,
                          GrOriginLocation_t, origin,
                          FxBool,             pixelPipeline,
                          GrLfbInfo_t*,       info)
    FxBool ret;
    __faddr (info);
    ret = __fcall (type, buffer, writeMode, origin, pixelPipeline, info);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK2(GRLFBUNLOCK, FxBool, GrLock_t, type, GrBuffer_t, buffer)
    FxBool ret;
    ret = __fcall (type, buffer);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK1(GRRENDERBUFFER, void, GrBuffer_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK1(GRSSTQUERYHARDWARE, FxBool, GrHwConfiguration*, hwConfig)
    FxBool ret;
    __faddr (hwConfig);
    ret = __fcall (hwConfig);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK1(GRSSTSELECT, void, int, which_sst)
    __fcall (which_sst);
ENDDECLARE

DECLARE_THUNK7(GRSSTWINOPEN, FxBool, FxU32, a, GrScreenResolution_t, b,
                             GrScreenRefresh_t, c, GrColorFormat_t, d,
                             GrOriginLocation_t, e, int, f, int, g)
    FxBool ret;
    ret = __fcall (a, b, c, d, e, f, g);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK0(GRSSTWINCLOSE, void)
    __fcall ();
ENDDECLARE

DECLARE_THUNK0(GRSSTSTATUS, FxU32)
    FxU32 ret;
    ret = __fcall ();
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK15(GUTEXALLOCATEMEMORY, GrMipMapId_t,
                                     GrChipID_t, tmu, FxU8, odd_even_mask,
                                     int, width, int, height, GrTextureFormat_t, fmt,
                                     GrMipMapMode_t, mm_mode, GrLOD_t, smallest_lod,
                                     GrLOD_t, largest_lod, GrAspectRatio_t, aspect,
                                     GrTextureClampMode_t, s_clamp_mode,
                                     GrTextureClampMode_t, t_clamp_mode,
                                     GrTextureFilterMode_t, minfilter_mode,
                                     GrTextureFilterMode_t, magfilter_mode,
                                     float, lod_bias, FxBool, trilinear)
    GrMipMapId_t ret;
    ret = __fcall (tmu, odd_even_mask, width, height, fmt,
                   mm_mode, smallest_lod, largest_lod, aspect,
                   s_clamp_mode, t_clamp_mode, minfilter_mode,
                   magfilter_mode, lod_bias, trilinear);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK4(GRTEXCALCMEMREQUIRED, FxU32, GrLOD_t, lodmin, GrLOD_t, lodmax, GrAspectRatio_t, aspect, GrTextureFormat_t, fmt)
    FxU32 ret;
    ret = __fcall (lodmin, lodmax, aspect, fmt);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK3(GRTEXCLAMPMODE, void, GrChipID_t, a, GrTextureClampMode_t, b,
                               GrTextureClampMode_t, c)
    __fcall (a, b, c);
ENDDECLARE

DECLARE_THUNK7(GRTEXCOMBINE, void, GrChipID_t, a, GrCombineFunction_t, b,
                             GrCombineFactor_t, c, GrCombineFunction_t, d,
                             GrCombineFactor_t, e, FxBool, f, FxBool, g)
    __fcall (a, b, c, d, e, f, g);
ENDDECLARE

DECLARE_THUNK2(GRTEXCOMBINEFUNCTION, void, GrChipID_t, a, GrTextureCombineFnc_t, b)
    __fcall (a, b);
ENDDECLARE

DECLARE_THUNK2(GUTEXCOMBINEFUNCTION, void, GrChipID_t, a, GrTextureCombineFnc_t, b)
    __fcall (a, b);
ENDDECLARE

DECLARE_THUNK4(GRTEXDETAILCONTROL, void, GrChipID_t, a, int, b, FxU8, c, float, d)
    __fcall (a, b, c, d);
ENDDECLARE

DECLARE_THUNK4(GRTEXDOWNLOADMIPMAP, void, GrChipID_t, tmu, FxU32, startAddress,
                                    FxU32, evenOdd, GrTexInfo*, info)
    __faddr (info);
    __fcall (tmu, startAddress, evenOdd, info);
ENDDECLARE

DECLARE_THUNK8(GRTEXDOWNLOADMIPMAPLEVEL, void,
                                         GrChipID_t,        tmu,
                                         FxU32,             startAddress,
                                         GrLOD_t,           thisLod,
                                         GrLOD_t,           largeLod,
                                         GrAspectRatio_t,   aspectRatio,
                                         GrTextureFormat_t, format,
                                         FxU32,             evenOdd,
                                         void*,             data)
    __fcall (tmu, startAddress, thisLod, largeLod, aspectRatio, format, evenOdd, data);
ENDDECLARE

DECLARE_THUNK3(GUTEXDOWNLOADMIPMAP, void, GrMipMapId_t, mmid, const void*, src,
                                    const GuNccTable*, table)
    __faddr (src);
    __faddr (table);
    __fcall (mmid, src, table);
ENDDECLARE

DECLARE_THUNK3(GRTEXDOWNLOADTABLE, void, GrChipID_t, tmu, GrTexTable_t, type,
                                   void*, data)
    __faddr (data);
    __fcall (tmu, type, data);
ENDDECLARE

DECLARE_THUNK3(GRTEXFILTERMODE, void, GrChipID_t, a, GrTextureFilterMode_t, b,
                                GrTextureFilterMode_t, c)
    __fcall (a, b, c);
ENDDECLARE

DECLARE_THUNK2(GRTEXLODBIASVALUE, void, GrChipID_t, a, float, b)
    __fcall (a, b);
ENDDECLARE

DECLARE_THUNK1(GRTEXMAXADDRESS, FxU32, GrChipID_t, a)
    FxU32 ret;
    ret = __fcall (a);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK1(GRTEXMINADDRESS, FxU32, GrChipID_t, a)
    FxU32 ret;
    ret = __fcall (a);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK1(GUTEXMEMQUERYAVAIL, FxU32, GrChipID_t, a)
    FxU32 ret;
    ret = __fcall (a);
    RETURNI (ret);
ENDDECLARE

DECLARE_THUNK0(GUTEXMEMRESET, void)
    __fcall ();
ENDDECLARE

DECLARE_THUNK3(GRTEXMIPMAPMODE, void, GrChipID_t, tmu, GrMipMapMode_t, mode,
                                FxBool, lodBlend)
    __fcall (tmu, mode, lodBlend);
ENDDECLARE

DECLARE_THUNK4(GRTEXSOURCE, void, GrChipID_t, tmu, FxU32, startAddress,
                            FxU32, evenOdd, GrTexInfo*, info)
    __faddr (info);
    __fcall (tmu, startAddress, evenOdd, info);
ENDDECLARE

DECLARE_THUNK1(GUTEXSOURCE, void, GrMipMapId_t, a)
    __fcall (a);
ENDDECLARE

DECLARE_THUNK2(GRTEXTEXTUREMEMREQUIRED, FxU32, FxU32, evenOdd, GrTexInfo*, info)
    FxU32 ret;
    __faddr (info);
    ret = __fcall (evenOdd, info);
    RETURNI (ret);
ENDDECLARE
