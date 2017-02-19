#ifndef INT_H
#define INT_H

#include <stdint.h>
#include <time.h> /* for time_t */

#define WINDOWS_HACKS 1
#if WINDOWS_HACKS
enum win3x_mode_enum { INACTIVE, REAL, STANDARD, ENHANCED };
extern enum win3x_mode_enum win3x_mode;
#endif

extern unsigned int  check_date;
extern time_t        start_time;

extern uint32_t int_bios_area[0x500/sizeof(uint32_t)];

void do_int(int);
void fake_int(int, int);
void fake_int_to(int cs, int ip);
void fake_call(int, int);
void fake_call_to(int cs, int ip);
void fake_pusha(void);
void fake_retf(unsigned pop_count);
void fake_iret(void);
void do_eoi_iret(void);
void do_eoi2_iret(void);
void jmp_to(int cs, int ip);
void setup_interrupts(void);
void version_init(void);
void int_vector_setup(void);
void dos_post_boot_reset(void);

#define REVECT		0
#define NO_REVECT	1

extern int can_revector(int i);

extern int redir_state;
extern void set_int21_revectored(int);

int dos_helper(void);

void update_xtitle(void);

void int42_hook(void);

#endif
