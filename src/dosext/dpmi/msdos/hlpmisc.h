#ifdef DOSEMU
#define RMREG(r) (rmreg->x.r)
#define RMLWORD(r) (rmreg->x.r)
#define X_RMREG(r) (rmreg->d.r)
#endif

static inline void pm_to_rm_regs(const sigcontext_t *scp,
			  __dpmi_regs *rmreg, unsigned int mask)
{
  if (mask & (1 << eflags_INDEX))
    RMREG(flags) = _eflags_;
  if (mask & (1 << eax_INDEX))
    X_RMREG(eax) = _LWORD_(eax_);
  if (mask & (1 << ebx_INDEX))
    X_RMREG(ebx) = _LWORD_(ebx_);
  if (mask & (1 << ecx_INDEX))
    X_RMREG(ecx) = _LWORD_(ecx_);
  if (mask & (1 << edx_INDEX))
    X_RMREG(edx) = _LWORD_(edx_);
  if (mask & (1 << esi_INDEX))
    X_RMREG(esi) = _LWORD_(esi_);
  if (mask & (1 << edi_INDEX))
    X_RMREG(edi) = _LWORD_(edi_);
  if (mask & (1 << ebp_INDEX))
    X_RMREG(ebp) = _LWORD_(ebp_);
}

static inline void rm_to_pm_regs(sigcontext_t *scp,
			  const __dpmi_regs *rmreg, unsigned int mask)
{
    /* WARNING - realmode flags can contain the dreadful NT flag
     * if we don't use safety masks. */
    if (mask & (1 << eflags_INDEX))
	_eflags = 0x0202 | (0x0dd5 & RMREG(flags));
    if (mask & (1 << eax_INDEX))
	_LWORD(eax) = RMLWORD(ax);
    if (mask & (1 << ebx_INDEX))
	_LWORD(ebx) = RMLWORD(bx);
    if (mask & (1 << ecx_INDEX))
	_LWORD(ecx) = RMLWORD(cx);
    if (mask & (1 << edx_INDEX))
	_LWORD(edx) = RMLWORD(dx);
    if (mask & (1 << esi_INDEX))
	_LWORD(esi) = RMLWORD(si);
    if (mask & (1 << edi_INDEX))
	_LWORD(edi) = RMLWORD(di);
    if (mask & (1 << ebp_INDEX))
	_LWORD(ebp) = RMLWORD(bp);
}

#define RMPRESERVE1(rg) (rm_mask |= (1 << rg##_INDEX))
#define E_RMPRESERVE1(rg) (rm_mask |= (1 << e##rg##_INDEX))
#define RMPRESERVE2(rg1, rg2) (rm_mask |= ((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_RMREG(rg, val) (RMPRESERVE1(rg), RMREG(rg) = (val))
#define SET_RMLWORD(rg, val) (E_RMPRESERVE1(rg), RMREG(rg) = (val))
