#include "cpu.h"
#include "dpmi.h"
#include "dosemu_debug.h"
#include "msdos_ldt.h"

#define LDT_INIT_LIMIT 0xfff

unsigned char *ldt_alias;
static unsigned short dpmi_ldt_alias;

int msdos_ldt_setup(unsigned char *alias, unsigned short alias_sel)
{
    if (SetSelector(alias_sel, DOSADDR_REL(alias),
		    LDT_INIT_LIMIT, 0,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
	return 0;
    ldt_alias = alias;
    dpmi_ldt_alias = alias_sel;
    return 1;
}

u_short DPMI_ldt_alias(void)
{
  return dpmi_ldt_alias;
}

int msdos_ldt_fault(struct sigcontext *scp, int pref_seg)
{
    if ((_err & 0xffff) != 0)
	return 0;
    if (pref_seg == -1)
	pref_seg = _ds;
    if (pref_seg == DPMI_ldt_alias()) {
	unsigned limit = GetSegmentLimit(DPMI_ldt_alias());
	D_printf("DPMI: expanding LDT, old_lim=0x%x\n", limit);
	SetSegmentLimit(DPMI_ldt_alias(), limit + DPMI_page_size);
	return 1;
    }

    return 0;
}
