#ifndef HOOKS_H
#define HOOKS_H

void fdpp_fatfs_hook(struct sys_dsc *sfiles, fatfs_t *fat);
int do_fdpp_call(uint16_t seg, uint16_t off);
int fdpp_coopth_wait(void);
int fdpp_coopth_yield(void);

#endif
