/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* dos2linux.h
 * 
 * Function prototypes for the DOSEMU/LINUX interface
 *
 */

#ifndef DOS2LINUX_H
#define DOS2LINUX_H

extern int misc_e6_envvar (char *str);

extern int misc_e6_commandline (char *str);
extern void misc_e6_store_command (char *str);

extern void run_unix_command (char *buffer);

#endif /* DOS2LINUX_H */
