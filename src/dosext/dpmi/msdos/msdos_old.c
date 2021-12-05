int msdos_pre_extender_old(sigcontext_t *scp, int intr,
			       struct RealModeCallStructure *rmreg,
			       int *r_mask, u_char *stk, int stk_len,
			       int *r_stk_used)
{
    int rm_mask = *r_mask, alt_ent = 0, act = 0, stk_used = 0;

    D_printf("MSDOS: pre_extender: int 0x%x, ax=0x%x\n", intr,
	     _LWORD(eax));
    if (MSDOS_CLIENT.user_dta_sel && intr == 0x21) {
	switch (_HI(ax)) {	/* functions use DTA */
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x4e:
	case 0x4f:		/* find first/next */
	    MEMCPY_2DOS(DTA_under_1MB, DTA_over_1MB, 0x80);
	    act = 1;
	    break;
	}
    }

    if (need_xbuf(intr, _LWORD(eax), _LWORD(ecx))) {
	int err = prepare_ems_frame();
	act = 1;
    }

    /* only consider DOS and some BIOS services */
    switch (intr) {
    case 0x41:			/* win debug */
	/* INT 41 - SYSTEM DATA - HARD DISK 0 PARAMETER TABLE ADDRESS [NOT A VECTOR!] */
	/* Since in real mode its not a vector, we need to return MSDOS_DONE. */
	return MSDOS_DONE;
    case 0x10:
	switch (_LWORD(eax)) {
	case 0x1130:
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;

    case 0x15:			/* misc */
	switch (_HI(ax)) {
	case 0xc0:
	    break;
	case 0xc2:
	    D_printf("MSDOS: PS2MOUSE function 0x%x\n", _LO(ax));
	    switch (_LO(ax)) {
	    case 0x07:		/* set handler addr */
		if (_es && D_16_32(_ebx)) {
		    far_t rma = MSDOS_CLIENT.rmcbs[RMCB_PS2MS];
		    D_printf
			("MSDOS: PS2MOUSE: set handler addr 0x%x:0x%x\n",
			 _es, D_16_32(_ebx));
		    MSDOS_CLIENT.PS2mouseCallBack.selector = _es;
		    MSDOS_CLIENT.PS2mouseCallBack.offset = D_16_32(_ebx);
		    SET_RMREG(es, rma.segment);
		    SET_RMLWORD(bx, rma.offset);
		} else {
		    D_printf("MSDOS: PS2MOUSE: reset handler addr\n");
		    SET_RMREG(es, 0);
		    SET_RMLWORD(bx, 0);
		}
		break;
	    default:
		break;
	    }
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
    case 0x20:			/* DOS terminate */
	old_dos_terminate(scp, intr, rmreg, rm_mask);
	RMPRESERVE2(cs, ip);
	break;
    case 0x21:
	switch (_HI(ax)) {
	case 0x2f:		/* GET DTA */
	case 0x32:		/* get DPB */
	case 0x34:		/* Get Address of InDOS Flag */
	case 0x51:		/* get PSP */
	case 0x52:		/* Get List of List */
	case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	case 0x62:		/* GET CURRENT PSP ADDRESS */
	case 0x63:		/* Get Lead Byte Table Address */
	    /* functions interesting for post_extender */
	    break;
	    /* first see if we don\'t need to go to real mode */
	case 0x25:{		/* set vector */
		DPMI_INTDESC desc;
		desc.selector = _ds;
		desc.offset32 = D_16_32(_edx);
		dpmi_set_interrupt_vector(_LO(ax), desc);
		D_printf("MSDOS: int 21,ax=0x%04x, ds=0x%04x. dx=0x%04x\n",
			 _LWORD(eax), _ds, _LWORD(edx));
	    }
	    return MSDOS_DONE;
	case 0x35:{		/* Get Interrupt Vector */
		DPMI_INTDESC desc = dpmi_get_interrupt_vector(_LO(ax));
		_es = desc.selector;
		_ebx = desc.offset32;
		D_printf("MSDOS: int 21,ax=0x%04x, es=0x%04x. bx=0x%04x\n",
			 _LWORD(eax), _es, _LWORD(ebx));
	    }
	    return MSDOS_DONE;
	case 0x48:		/* allocate memory */
	    {
		unsigned long size = _LWORD(ebx) << 4;
		dosaddr_t addr = msdos_malloc(size);
		if (!addr) {
		    unsigned int meminfo[12];
		    GetFreeMemoryInformation(meminfo);
		    _eflags |= CF;
		    _LWORD(ebx) = meminfo[0] >> 4;
		    _LWORD(eax) = 0x08;
		} else {
		    unsigned short sel = AllocateDescriptors(1);
		    SetSegmentBaseAddress(sel, addr);
		    SetSegmentLimit(sel, size - 1);
		    _LWORD(eax) = sel;
		    _eflags &= ~CF;
		}
		return MSDOS_DONE;
	    }
	case 0x49:		/* free memory */
	    {
		if (msdos_free(GetSegmentBase(_es)) == -1) {
		    _eflags |= CF;
		} else {
		    _eflags &= ~CF;
		    FreeDescriptor(_es);
		    FreeSegRegs(scp, _es);
		}
		return MSDOS_DONE;
	    }
	case 0x4a:		/* reallocate memory */
	    {
		unsigned new_size = _LWORD(ebx) << 4;
		unsigned addr =
		    msdos_realloc(GetSegmentBase(_es), new_size);
		if (!addr) {
		    unsigned int meminfo[12];
		    GetFreeMemoryInformation(meminfo);
		    _eflags |= CF;
		    _LWORD(ebx) = meminfo[0] >> 4;
		    _LWORD(eax) = 0x08;
		} else {
		    SetSegmentBaseAddress(_es, addr);
		    SetSegmentLimit(_es, new_size - 1);
		    _eflags &= ~CF;
		}
		return MSDOS_DONE;
	    }
	case 0x00:		/* DOS terminate */
	    old_dos_terminate(scp, intr, rmreg, rm_mask);
	    RMPRESERVE2(cs, ip);
	    SET_RMLWORD(ax, 0x4c00);
	    break;
	case 0x09:		/* Print String */
	    {
		int i;
		char *s, *d;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		d = msdos_seg2lin(trans_buffer_seg());
		s = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		for (i = 0; i < 0xffff; i++, d++, s++) {
		    *d = *s;
		    if (*s == '$')
			break;
		}
	    }
	    break;
	case 0x1a:		/* set DTA */
	    {
		unsigned long off = D_16_32(_edx);
		if (!in_dos_space(_ds, off)) {
		    MSDOS_CLIENT.user_dta_sel = _ds;
		    MSDOS_CLIENT.user_dta_off = off;
		    SET_RMREG(ds, MSDOS_CLIENT.lowmem_seg + DTA_Para_ADD);
		    SET_RMLWORD(dx, 0);
		    MEMCPY_2DOS(DTA_under_1MB, DTA_over_1MB, 0x80);
		} else {
		    SET_RMREG(ds, GetSegmentBase(_ds) >> 4);
		    MSDOS_CLIENT.user_dta_sel = 0;
		}
	    }
	    break;

	    /* FCB functions */
	case 0x0f:
	case 0x10:		/* These are not supported by */
	case 0x14:
	case 0x15:		/* dosx.exe, according to Ralf Brown */
	case 0x21 ... 0x24:
	case 0x27:
	case 0x28:
	    error("MS-DOS: Unsupported function 0x%x\n", _HI(ax));
	    _HI(ax) = 0xff;
	    return MSDOS_DONE;
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x13:		/* Delete using FCB */
	case 0x16:		/* Create usring FCB */
	case 0x17:		/* rename using FCB */
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32), 0x50);
	    break;
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			    0x50);
	    }
	    break;
	case 0x47:		/* GET CWD */
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(si, 0);
	    break;
	case 0x4b:		/* EXEC */
	    {
		/* we must copy all data from higher 1MB to lower 1MB */
		unsigned short segment = EXEC_SEG, par_seg;
		char *p;
		unsigned short sel, off;
		far_t rma = get_exec_helper();

		/* must copy command line */
		SET_RMREG(ds, segment);
		SET_RMLWORD(dx, 0);
		p = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		snprintf((char *) msdos_seg2lin(segment), MAX_DOS_PATH,
			 "%s", p);
		segment += (MAX_DOS_PATH + 0x0f) >> 4;
		D_printf("BCC: call dos exec: %s\n", p);

		/* must copy parameter block */
		SET_RMREG(es, segment);
		SET_RMLWORD(bx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),
			    SEL_ADR_X(_es, _ebx, MSDOS_CLIENT.is_32),
			    0x20);
		par_seg = segment;
		segment += 2;
#if 0
		/* now the envrionment segment */
		sel = READ_WORD(SEGOFF2LINEAR(par_seg, 0));
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0), segment);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),	/* 4K envr. */
			    GetSegmentBase(sel), 0x1000);
		segment += 0x100;
#else
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0), 0);
#endif
		/* now the tail of the command line */
		off = READ_WORD(SEGOFF2LINEAR(par_seg, 2));
		sel = READ_WORD(SEGOFF2LINEAR(par_seg, 4));
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 4), segment);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 2), 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),
			    SEL_ADR_X(sel, off, MSDOS_CLIENT.is_32),
			    0x80);
		segment += 8;

		/* set the FCB pointers to something reasonable */
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 6), 0);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 8), segment);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0xA), 0);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0xC), segment);
		/* zero out fcbs */
		MEMSET_DOS(SEGOFF2LINEAR(segment, 0), 0, 0x30);
		segment += 3;

		/* then the enviroment seg */
		if (get_env_sel())
		    write_env_sel(GetSegmentBase(get_env_sel()) >> 4);

		if (segment != EXEC_SEG + EXEC_Para_SIZE)
		    error("DPMI: exec: seg=%#x (%#x), size=%#x\n",
			  segment, segment - EXEC_SEG, EXEC_Para_SIZE);
		if (ems_frame_mapped)
		    error
			("DPMI: exec: EMS frame should not be mapped here\n");
		/* the new client may do the raw mode switches (eg.
		 * tlink.exe when started by bcc.exe) and we need to
		 * preserve at least current client's eip. In fact, DOS exec
		 * preserves most registers, so, if the child is not polite,
		 * we also need to preserve all segregs and stack regs too
		 * (rest will be translated from realmode).
		 * The problem is that DOS stores the registers on the stack
		 * so we can't replace the already saved regs with PM ones
		 * and extract them in post_extender() as we usually do.
		 * Additionally PM eax will be trashed, so we need to
		 * preserve also that for post_extender() to work at all.
		 * This is the task for the realmode helper.
		 * The alternative implementation was to do save_pm_regs()
		 * here and use the AX stack for post_extender(), which
		 * is both unportable and ugly. */
		rm_do_int_to(_eflags, rma.segment, rma.offset,
			rmreg, &rm_mask, stk, stk_len, &stk_used);
		alt_ent = 1;
	    }
	    break;

	case 0x50:		/* set PSP */
	    if (!in_dos_space(_LWORD(ebx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(ebx);
		SET_RMLWORD(bx, CURRENT_PSP);
		MEMCPY_DOS2DOS(SEGOFF2LINEAR(CURRENT_PSP, 0),
			       GetSegmentBase(_LWORD(ebx)), 0x100);
		D_printf("MSDOS: PSP moved from %x to %x\n",
			 GetSegmentBase(_LWORD(ebx)),
			 SEGOFF2LINEAR(CURRENT_PSP, 0));
	    } else {
		MSDOS_CLIENT.current_psp = GetSegmentBase(_LWORD(ebx)) >> 4;
		SET_RMLWORD(bx, MSDOS_CLIENT.current_psp);
		MSDOS_CLIENT.user_psp_sel = 0;
	    }
	    break;

	case 0x26:		/* create PSP */
	    SET_RMLWORD(dx, trans_buffer_seg());
	    break;

	case 0x55:		/* create & set PSP */
	    if (!in_dos_space(_LWORD(edx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(edx);
		SET_RMLWORD(dx, CURRENT_PSP);
	    } else {
		MSDOS_CLIENT.current_psp = GetSegmentBase(_LWORD(edx)) >> 4;
		SET_RMLWORD(dx, MSDOS_CLIENT.current_psp);
		MSDOS_CLIENT.user_psp_sel = 0;
	    }
	    break;

	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x5b:		/* Create */
	    {
		char *src, *dst;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		dst = msdos_seg2lin(trans_buffer_seg());
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x5d:		/* share & misc  */
	    if (_LO(ax) <= 0x05 || _LO(ax) == 0x0a) {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(dx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			    0x16);
	    }
	    break;
	case 0x38:
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    break;
	case 0x3f:		/* dos read */
	    msdos_lr_helper(scp, MSDOS_CLIENT.rmreg_buf, trans_buffer_seg(),
		    restore_ems_frame);
	    return MSDOS_DONE;
	case 0x40:		/* dos write */
	    msdos_lw_helper(scp, MSDOS_CLIENT.rmreg_buf, trans_buffer_seg(),
		    restore_ems_frame);
	    return MSDOS_DONE;
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x30);
		seg += 3;

		SET_RMREG(es, seg);
		SET_RMLWORD(bp, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_es, _ebp, MSDOS_CLIENT.is_32),
			    0x60);
	    }
	    break;
	case 0x56:		/* rename file */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(dx, 0);
		snprintf(msdos_seg2lin(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_X(_ds, _edx,
					       MSDOS_CLIENT.is_32));
		seg += 0x20;

		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		snprintf(msdos_seg2lin(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_X(_es, _edi,
					       MSDOS_CLIENT.is_32));
	    }
	    break;
	case 0x5f: {		/* redirection */
	    unsigned short seg;
	    switch (_LO(ax)) {
	    case 0:
	    case 1:
		break;
	    case 2:
	    case 5:
	    case 6:
		seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		seg++;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		break;
	    case 3:
		seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    16);
		seg++;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			    128);
		break;
	    case 4:
		seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    128);
		break;
	    }
	    break;
	  }
	case 0x60:		/* Get Fully Qualified File Name */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		break;
	    }
	case 0x6c:		/*  Extended Open/Create */
	    {
		char *src, *dst;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(si, 0);
		src = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
		dst = msdos_seg2lin(trans_buffer_seg());
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x65:		/* internationalization */
	    switch (_LO(ax)) {
	    case 0:
		SET_RMREG(es, trans_buffer_seg());
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			    SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 1 ... 7:
		SET_RMREG(es, trans_buffer_seg());
		SET_RMLWORD(di, 0);
		break;
	    case 0x21:
	    case 0xa1:
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			    SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		strcpy(msdos_seg2lin(trans_buffer_seg()),
		       SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32));
		break;
	    }
	    break;
	case 0x71:		/* LFN functions */
	    {
		char *src, *dst;
		switch (_LO(ax)) {
		case 0x3B:	/* change dir */
		case 0x41:	/* delete file */
		case 0x43:	/* get file attributes */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4E:	/* find first file */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4F:	/* find next file */
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, 0);
		    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
				SEL_ADR_X(_es, _edi,
					     MSDOS_CLIENT.is_32), 0x13e);
		    break;
		case 0x47:	/* get cur dir */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(si, 0);
		    break;
		case 0x60:	/* canonicalize filename */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(si, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x6c:	/* extended open/create */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(si, 0);
		    src = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA0:	/* get volume info */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA1:	/* close find */
		    break;
		case 0x42:	/* long seek */
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(SCRATCH_SEG);
		    memcpy(dst, src, 8);
		    SET_RMREG(ds, SCRATCH_SEG);
		    SET_RMLWORD(dx, 0);
		    break;
		case 0xA6:	/* get file info by handle */
		    SET_RMREG(ds, SCRATCH_SEG);
		    SET_RMLWORD(dx, 0);
		    break;
		default:	/* all other subfuntions currently not supported */
		    _eflags |= CF;
		    _eax = _eax & 0xFFFFFF00;
		    return MSDOS_DONE;
		}
	    }
	    break;
	case 0x73: {		/* fat32 functions */
		char *src, *dst;
		switch (_LO(ax)) {
		case 2:		/* GET EXTENDED DPB */
		case 4:		/* Set DPB TO USE FOR FORMATTING */
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, 0);
		    break;
		case 3:		/* GET EXTENDED FREE SPACE ON DRIVE */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 5:		/* EXTENDED ABSOLUTE DISK READ/WRITE */
		    if (_LWORD(ecx) == 0xffff) {
			int err = 0;
			uint8_t *pkt = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
//			err = do_abs_rw(scp, rmreg, r_mask, pkt, _LWORD(esi) & 1);
			if (err)
			    return MSDOS_DONE;
		    }
		    break;
		}
	    }
	    break;

	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;

    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk Write */
	if (_LWORD(ecx) == 0xffff) {
	    int err = 0;
	    uint8_t *src = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
//	    err = do_abs_rw(scp, rmreg, r_mask, src, intr == 0x26);
	    if (err)
		return MSDOS_DONE;
	}
	/* not to return MSDOS_NONE here because RM flags are returned
	 * on stack. */
	break;

    case 0x2f:
	switch (_LWORD(eax)) {
	case 0x1688:
	    _eax = 0;
	    _ebx = MSDOS_CLIENT.ldt_alias;
	    return MSDOS_DONE;
	case 0x168a:
	    get_ext_API(scp);
	    return MSDOS_DONE;
	/* need to be careful with 0x2f as it is currently revectored.
	 * As such, we need to return MSDOS_NONE for what we don't handle,
	 * but break for what the post_extender is needed.
	 * Maybe eventually it will be possible to make int2f non-revect. */
	case 0x4310:	// for post_extender()
	    break;
	case 0xae00:
	case 0xae01: {
	    uint8_t *src1 = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
	    uint8_t *src2 = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
	    int dsbx_len = src1[1] + 3;
	    SET_RMREG(ds, SCRATCH_SEG);
	    SET_RMLWORD(bx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(SCRATCH_SEG, 0), src1, dsbx_len);
	    SET_RMLWORD(si, dsbx_len);
	    MEMCPY_2DOS(SEGOFF2LINEAR(SCRATCH_SEG, dsbx_len), src2, 12);
	    break;
	}
	default:	// for do_int()
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
    case 0x33:			/* mouse */
	switch (_LWORD(eax)) {
	case 0x19:		/* Get User Alternate Interrupt Address */
	    /* for post_extender */
	    break;
	case 0x09:		/* Set Mouse Graphics Cursor */
	    SET_RMREG(es, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			SEL_ADR_X(_es, _edx, MSDOS_CLIENT.is_32), 16);
	    break;
	case 0x0c:		/* set call back */
	case 0x14:{		/* swap call back */
		struct pmaddr_s old_callback = MSDOS_CLIENT.mouseCallBack;
		MSDOS_CLIENT.mouseCallBack.selector = _es;
		MSDOS_CLIENT.mouseCallBack.offset = D_16_32(_edx);
		if (_es) {
		    far_t rma = MSDOS_CLIENT.rmcbs[RMCB_MS];
		    D_printf("MSDOS: set mouse callback\n");
		    SET_RMREG(es, rma.segment);
		    SET_RMLWORD(dx, rma.offset);
		} else {
		    D_printf("MSDOS: reset mouse callback\n");
		    SET_RMREG(es, 0);
		    SET_RMLWORD(dx, 0);
		}
		if (_LWORD(eax) == 0x14) {
		    _es = old_callback.selector;
		    if (MSDOS_CLIENT.is_32)
			_edx = old_callback.offset;
		    else
			_LWORD(edx) = old_callback.offset;
		}
	    }
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;

#ifdef SUPPORT_DOSEMU_HELPERS
    case DOS_HELPER_INT:	/* dosemu helpers */
	switch (_LO(ax)) {
	case DOS_HELPER_PRINT_STRING:	/* print string to dosemu log */
	    {
		char *s, *d;
		SET_RMREG(es, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		d = msdos_seg2lin(trans_buffer_seg());
		s = SEL_ADR_X(_es, _edx, MSDOS_CLIENT.is_32);
		snprintf(d, 1024, "%s", s);
	    }
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
#endif

    default:
	if (!act)
	    return MSDOS_NONE;
	break;
    }

    if (need_copy_dseg(intr, _LWORD(eax), _LWORD(ecx))) {
	unsigned int src, dst;
	int len;
	SET_RMREG(ds, trans_buffer_seg());
	src = GetSegmentBase(_ds);
	dst = SEGOFF2LINEAR(trans_buffer_seg(), 0);
	len = min((int) (GetSegmentLimit(_ds) + 1), 0x10000);
	D_printf
	    ("MSDOS: whole segment of DS at %x copy to DOS at %x for %#x\n",
	     src, dst, len);
	MEMCPY_DOS2DOS(dst, src, len);
    }

    if (need_copy_eseg(intr, _LWORD(eax))) {
	unsigned int src, dst;
	int len;
	SET_RMREG(es, trans_buffer_seg());
	src = GetSegmentBase(_es);
	dst = SEGOFF2LINEAR(trans_buffer_seg(), 0);
	len = min((int) (GetSegmentLimit(_es) + 1), 0x10000);
	D_printf
	    ("MSDOS: whole segment of ES at %x copy to DOS at %x for %#x\n",
	     src, dst, len);
	MEMCPY_DOS2DOS(dst, src, len);
    }

    if (!alt_ent)
	rm_int(intr, _eflags, rmreg, &rm_mask,
		stk, stk_len, &stk_used);
    *r_mask = rm_mask;
    *r_stk_used = stk_used;
    return MSDOS_RM;
}

void msdos_post_extender_old(sigcontext_t *scp, int intr,
				const struct RealModeCallStructure *rmreg)
{
    u_short ax = _LWORD(eax);
    int update_mask = ~0;
#define PRESERVE1(rg) (update_mask &= ~(1 << rg##_INDEX))
#define PRESERVE2(rg1, rg2) (update_mask &= ~((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_REG(rg, val) (PRESERVE1(rg), _##rg = (val))
#define TRANSLATE_S(sr) SET_REG(sr, ConvertSegmentToDescriptor(RMREG(sr)))
    D_printf("MSDOS: post_extender: int 0x%x ax=0x%04x\n", intr, ax);

    if (MSDOS_CLIENT.user_dta_sel && intr == 0x21) {
	switch (HI_BYTE(ax)) {	/* functions use DTA */
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x4e:
	case 0x4f:		/* find first/next */
	    MEMCPY_2UNIX(DTA_over_1MB, DTA_under_1MB, 0x80);
	    break;
	}
    }

    if (need_copy_dseg(intr, ax, _LWORD(ecx))) {
	unsigned short my_ds;
	unsigned int src, dst;
	int len;
	my_ds = trans_buffer_seg();
	src = SEGOFF2LINEAR(my_ds, 0);
	dst = GetSegmentBase(_ds);
	len = min((int) (GetSegmentLimit(_ds) + 1), 0x10000);
	D_printf("MSDOS: DS seg at %x copy back at %x for %#x\n",
		 src, dst, len);
	memcpy_dos2dos(dst, src, len);
    }

    if (need_copy_eseg(intr, ax)) {
	unsigned short my_es;
	unsigned int src, dst;
	int len;
	my_es = trans_buffer_seg();
	src = SEGOFF2LINEAR(my_es, 0);
	dst = GetSegmentBase(_es);
	len = min((int) (GetSegmentLimit(_es) + 1), 0x10000);
	D_printf("MSDOS: ES seg at %x copy back at %x for %#x\n",
		 src, dst, len);
	memcpy_dos2dos(dst, src, len);
    }

    switch (intr) {
    case 0x10:			/* video */
	if (ax == 0x1130) {
	    /* get current character generator infor */
	    TRANSLATE_S(es);
	}
	break;
    case 0x15:
	/* we need to save regs at int15 because AH has the return value */
	if (HI_BYTE(ax) == 0xc0) {	/* Get Configuration */
	    if (RMREG(flags) & CF)
		break;
	    TRANSLATE_S(es);
	}
	break;
    case 0x2f:
	switch (ax) {
	case 0x4310: {
	    struct pmaddr_s pma = {};
	    MSDOS_CLIENT.XMS_call = MK_FARt(RMREG(es), RMLWORD(bx));
//	    pma = get_pmrm_handler(XMS_CALL, xms_call, get_xms_call,
//		    xms_ret, MSDOS_CLIENT.rmreg_buf, scratch_seg, NULL);
	    SET_REG(es, pma.selector);
	    SET_REG(ebx, pma.offset);
	    break;
	}
	case 0xae00:
	    PRESERVE2(ebx, esi);
	    break;
	case 0xae01: {
	    int dsbx_len = READ_BYTE(SEGOFF2LINEAR(RMREG(ds),
		    RMLWORD(bx)) + 1) + 3;

	    PRESERVE2(ebx, esi);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(bx)), dsbx_len);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(si)), 12);
	    break;
	}
	}
	break;

    case 0x21:
	switch (HI_BYTE(ax)) {
	case 0x00:		/* psp kill */
	    PRESERVE1(eax);
	    break;
	case 0x09:		/* print String */
	case 0x1a:		/* set DTA */
	    PRESERVE1(edx);
	    break;
	case 0x11:
	case 0x12:		/* findfirst/next using FCB */
	case 0x13:		/* Delete using FCB */
	case 0x16:		/* Create usring FCB */
	case 0x17:		/* rename using FCB */
	    PRESERVE1(edx);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x50);
	    break;

	case 0x29:		/* Parse a file name for FCB */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			 /* Warning: SI altered, assume old value = 0, don't touch. */
			 SEGOFF2LINEAR(RMREG(ds), 0), 0x100);
	    SET_REG(esi, _esi + RMLWORD(si));
	    MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(di)), 0x50);
	    break;

	case 0x2f:		/* GET DTA */
	    if (SEGOFF2LINEAR(RMREG(es), RMLWORD(bx)) == DTA_under_1MB) {
		if (!MSDOS_CLIENT.user_dta_sel)
		    error("Selector is not set for the translated DTA\n");
		SET_REG(es, MSDOS_CLIENT.user_dta_sel);
		SET_REG(ebx, MSDOS_CLIENT.user_dta_off);
	    } else {
		TRANSLATE_S(es);
		/* it is important to copy only the lower word of ebx
		 * and make the higher word zero, so do it here instead
		 * of relying on dpmi.c */
		SET_REG(ebx, RMLWORD(bx));
	    }
	    break;

	case 0x34:		/* Get Address of InDOS Flag */
	case 0x35:		/* GET Vector */
	case 0x52:		/* Get List of List */
	    TRANSLATE_S(es);
	    break;

	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x5b:		/* Create */
	    PRESERVE1(edx);
	    break;

	case 0x50:		/* Set PSP */
	    PRESERVE1(ebx);
	    break;

	case 0x6c:		/*  Extended Open/Create */
	    PRESERVE1(esi);
	    break;

	case 0x55:		/* create & set PSP */
	    PRESERVE1(edx);
	    if (!in_dos_space(_LWORD(edx), 0)) {
		MEMCPY_DOS2DOS(GetSegmentBase(_LWORD(edx)),
			       SEGOFF2LINEAR(RMLWORD(dx), 0), 0x100);
	    }
	    break;

	case 0x26:		/* create PSP */
	    PRESERVE1(edx);
	    MEMCPY_DOS2DOS(GetSegmentBase(_LWORD(edx)),
			   SEGOFF2LINEAR(RMLWORD(dx), 0), 0x100);
	    break;

	case 0x32:		/* get DPB */
	    if (RM_LO(ax) != 0)	/* check success */
		break;
	    TRANSLATE_S(ds);
	    break;

	case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	    if (RMLWORD(ax) == 0x22) {	/* only this code has a pointer */
		TRANSLATE_S(es);
	    }
	    break;
	case 0x38:
	    if (_LWORD(edx) != 0xffff) {	/* get country info */
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		/* FreeDOS copies only 0x18 bytes */
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x18);
	    }
	    break;
	case 0x47:		/* get CWD */
	    PRESERVE1(esi);
	    if (RMREG(flags) & CF)
		break;
	    snprintf(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32), 0x40,
		     "%s", RMSEG_ADR((char *), ds, si));
	    D_printf("MSDOS: CWD: %s\n",
		     (char *) SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32));
	    break;
#if 0
	case 0x48:		/* allocate memory */
	    if (RMREG(eflags) & CF)
		break;
	    SET_REG(eax, ConvertSegmentToDescriptor(RMLWORD(ax)));
	    break;
#endif
	case 0x4b:		/* EXEC */
	    if (get_env_sel())
		write_env_sel(ConvertSegmentToDescriptor(get_env_sel()));
	    D_printf("DPMI: Return from DOS exec\n");
	    break;
	case 0x51:		/* get PSP */
	case 0x62:
	    {			/* convert environment pointer to a descriptor */
		unsigned short psp = RMLWORD(bx);
		if (psp == CURRENT_PSP && MSDOS_CLIENT.user_psp_sel) {
		    SET_REG(ebx, MSDOS_CLIENT.user_psp_sel);
		} else {
		    SET_REG(ebx,
			    ConvertSegmentToDescriptor_lim(psp, 0xff));
		}
	    }
	    break;
	case 0x53:		/* Generate Drive Parameter Table  */
	    PRESERVE2(esi, ebp);
	    MEMCPY_2UNIX(SEL_ADR_X(_es, _ebp, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(bp)), 0x60);
	    break;
	case 0x56:		/* rename */
	    PRESERVE2(edx, edi);
	    break;
	case 0x5d:
	    if (RMREG(flags) & CF)
		break;
	    if (LO_BYTE(ax) == 0x06 || LO_BYTE(ax) == 0x0b)
		/* get address of DOS swappable area */
		/*        -> DS:SI                     */
		TRANSLATE_S(ds);
	    else
		PRESERVE1(edx);
	    if (LO_BYTE(ax) == 0x05)
		TRANSLATE_S(es);
	    break;
	case 0x63:		/* Get Lead Byte Table Address */
	    /* _LO(ax)==0 is to test for 6300 on input, RM_LO(ax)==0 for success */
	    if (LO_BYTE(ax) == 0 && RM_LO(ax) == 0)
		TRANSLATE_S(ds);
	    break;

	case 0x5f:		/* redirection */
	    switch (LO_BYTE(ax)) {
	    case 0:
	    case 1:
		break;
	    case 2:
	    case 5:
	    case 6:
		PRESERVE2(esi, edi);
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(si)),
			     16);
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     128);
		break;
	    case 3:
		PRESERVE2(esi, edi);
		break;
	    case 4:
		PRESERVE1(esi);
		break;
	    }
	    break;
	case 0x60:		/* Canonicalize file name */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(di)), 0x80);
	    break;
	case 0x65:		/* internationalization */
	    PRESERVE2(edi, edx);
	    if (RMREG(flags) & CF)
		break;
	    switch (LO_BYTE(ax)) {
	    case 1 ... 7:
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     RMLWORD(cx));
		break;
	    case 0x21:
	    case 0xa1:
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)),
			     _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		strcpy(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
		       RMSEG_ADR((void *), ds, dx));
		break;
	    }
	    break;
	case 0x71:		/* LFN functions */
	    switch (LO_BYTE(ax)) {
	    case 0x3B:
	    case 0x41:
	    case 0x43:
		PRESERVE1(edx);
		break;
	    case 0x4E:
		PRESERVE1(edx);
		/* fall thru */
	    case 0x4F:
		PRESERVE1(edi);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     0x13E);
		break;
	    case 0x47:
		PRESERVE1(esi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), ds, si));
		break;
	    case 0x60:
		PRESERVE2(esi, edi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0x6c:
		PRESERVE1(esi);
		break;
	    case 0xA0:
		PRESERVE2(edx, edi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 _LWORD(ecx), "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0x42:
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 8);
		break;
	    case 0xA6:
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x34);
		break;
	    };
	    break;

	case 0x73:		/* fat32 functions */
	    switch (LO_BYTE(ax)) {
	    case 2:		/* GET EXTENDED DPB */
	    case 3:		/* GET EXTENDED FREE SPACE ON DRIVE */
	    case 4:		/* Set DPB TO USE FOR FORMATTING */
		PRESERVE1(edi);
		if (LO_BYTE(ax) == 3)
		    PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     RMLWORD(cx));
		break;
	    case 5:
		PRESERVE1(ebx);
		if (RMREG(flags) & CF)
		    break;
		if (_LWORD(ecx) == 0xffff && !(_LWORD(esi) & 1)) {	/* read */
		    uint8_t *src = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
		    uint16_t sectors = *(uint16_t *)(src + 4);
		    uint32_t addr = *(uint32_t *)(src + 6);
		    MEMCPY_2UNIX(SEL_ADR_X(FP_SEG16(addr), FP_OFF16(addr),
					MSDOS_CLIENT.is_32),
				SEGOFF2LINEAR(trans_buffer_seg(), 0),
				sectors * 512);
		}
		break;
	    }
	    break;

	default:
	    break;
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk Write */
	/* the flags should be pushed to stack */
	if (MSDOS_CLIENT.is_32) {
	    _esp -= 4;
	    *(uint32_t *) (SEL_ADR_X(_ss, _esp, MSDOS_CLIENT.is_32))
		= RMREG(flags);
	} else {
	    _esp -= 2;
	    *(uint16_t
	      *) (SEL_ADR_X(_ss, _LWORD(esp), MSDOS_CLIENT.is_32)) =
		RMREG(flags);
	}
	PRESERVE1(ebx);
	if (_LWORD(ecx) == 0xffff && intr == 0x25) {	/* read */
	    uint8_t *src = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
	    uint16_t sectors = *(uint16_t *)(src + 4);
	    uint32_t addr = *(uint32_t *)(src + 6);
	    MEMCPY_2UNIX(SEL_ADR_X(FP_SEG16(addr), FP_OFF16(addr),
			MSDOS_CLIENT.is_32),
		    SEGOFF2LINEAR(trans_buffer_seg(), 0),
		    sectors * 512);
	}
	break;
    case 0x33:			/* mouse */
	switch (ax) {
	case 0x09:		/* Set Mouse Graphics Cursor */
	case 0x14:		/* swap call back */
	    PRESERVE1(edx);
	    break;
	case 0x19:		/* Get User Alternate Interrupt Address */
	    SET_REG(ebx, ConvertSegmentToDescriptor(RMLWORD(bx)));
	    break;
	default:
	    break;
	}
	break;
#ifdef SUPPORT_DOSEMU_HELPERS
    case DOS_HELPER_INT:	/* dosemu helpers */
	switch (LO_BYTE(ax)) {
	case DOS_HELPER_PRINT_STRING:	/* print string to dosemu log */
	    PRESERVE1(edx);
	    break;
	}
	break;
#endif
    }

    if (need_xbuf(intr, ax, _LWORD(ecx)))
	restore_ems_frame();
    rm_to_pm_regs(scp, rmreg, update_mask);
}
