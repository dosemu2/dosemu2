/* inifile.h */

/* Routines to modify INI files */
/* by David V. Mortenson */

void GetValueFromIniFile (char *file, char *section, char *variable, 
               char *value);
void GetValueFromIniFilePath (char *file, char *section, char *variable, 
               char *value);
void SetValueInIniFile (char *file, char* section, char* variable, 
               char *value);

