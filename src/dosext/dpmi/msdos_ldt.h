#ifndef MSDOS_LDT_H
#define MSDOS_LDT_H

int msdos_ldt_setup(unsigned char *alias, unsigned short alias_sel);
u_short DPMI_ldt_alias(void);
int msdos_ldt_fault(struct sigcontext *scp, int pref_seg);

#endif
