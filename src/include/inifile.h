/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* inifile.h */

/* Routines to modify INI files */
/* by David V. Mortenson */

extern void GetValueFromIniFile (char *file, char *section, char *variable, 
               char *value);
extern void GetValueFromIniFilePath (char *file, char *section, char *variable, 
               char *value);
extern void SetValueInIniFile (char *file, char* section, char* variable, 
               char *value);

