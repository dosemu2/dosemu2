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
void dos_post_boot_reset(void);
void int_try_disable_revect(void);

enum { I_NOT_HANDLED, I_HANDLED, I_SECOND_REVECT };

extern int can_revector(int i);
far_t get_int_vector(int vec);

void update_xtitle(void);

void int42_hook(void);

enum { OWN_DEMU, OWN_d, OWN_SYS, OWN_COM };
int add_extra_drive(char *path, int ro, int cd, int owner, int index);
int find_free_drive(void);
int find_drive(int owner, int index);
uint16_t get_redirection(uint16_t redirIndex, char *deviceStr, int deviceSize,
    char *resourceStr, int resourceSize, uint8_t *deviceType, uint16_t *deviceUserData,
    uint16_t *deviceOptions, uint8_t *deviceStatus);

#endif
