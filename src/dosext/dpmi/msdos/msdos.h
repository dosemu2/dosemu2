/* 	MS-DOS API translator for DOSEMU\'s DPMI Server
 *
 * DANG_BEGIN_MODULE msdos.h
 *
 * REMARK
 * MS-DOS API translator allows DPMI programs to call DOS service directly
 * in protected mode.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * First Attempted by Dong Liu,  dliu@rice.njit.edu
 *
 */

#ifndef __MSDOS_H__
#define __MSDOS_H__

void msdos_setup(void);
void msdos_reset(void);
void msdos_init(int is_32, unsigned short mseg, unsigned short psp);
void msdos_done(void);
void msdos_set_client(int num);
int msdos_get_lowmem_size(void);

#endif
