/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* inifile.c */

/* Routines to modify INI files */
/* by David V. Mortenson */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define FALSE  0
#define TRUE   1

#include "inifile.h"

/****************************************
 * An ini file should have the extension .INI, and is formatted
 * with sections and value strings.  The following is a sample
 * .INI file with 2 sections, each with 1 variable/value string:
 * 
 *      [test]
 *      teststring=Hello World
 *      [sample]
 *      value=on
 *
 * The use of the functions to retrieve the string values and
 * set them in the INI file should be apparent.  If an error
 * occurs, then the value string for the requested section and
 * variable will return with a length of 0.  Possible errors
 * include:
 *      1) file not found
 *      2) section not found (only for "get" operation)
 *      3) variable not found (only for "get" operation)
 *
 * GetValueFromIniFilePath is the same as GetValueFromIniFile, except
 * that the file paramter can hold a colon delimited path of locations
 * to look for the section and variable.  Each one is searched in turn.
 * This mechanism could be used to allow per-user variables to override
 * system defaults, for example.
 ****************************************/

/**********************************************************************DVM***/
void GetValueFromIniFile (char *file, char *section, char *variable, 
               char *value)
{
   FILE *srcFile;
   char line [512];
   char *cptr;

   strcpy (value, "");

   srcFile = fopen (file, "rt");
   if (!srcFile) return;

   cptr = fgets (line, 512, srcFile);

   while (cptr)
   {
      /* Is this the requested section? */
      if (  line[0] == '['  &&  
            !strncasecmp (&line[1], section, strlen(section))  &&
            line [strlen(section)+1] == ']'  )
      {
         cptr = fgets (line, 512, srcFile);
         while (cptr)
         {
            /* Is this the requested variable? */
            if (  !strncasecmp (line, variable, strlen (variable))  &&
                  line [strlen(variable)] == '='  )
            {
               strcpy (value, &line[strlen(variable)+1]);
               if (value[strlen(value)-1] == '\n')
                  value[strlen(value)-1] = '\0';
               fclose (srcFile);
               return;
            }

            /* Have we entered a new section? */
            if (line[0] == '[')
            {
               break;
            }

            cptr = fgets (line, 512, srcFile);
         }
      }
      else
      {
         cptr = fgets (line, 512, srcFile);
      }
   }

   fclose (srcFile);
}

/**********************************************************************TRB***/
void GetValueFromIniFilePath (char *file, char *section, char *variable, 
               char *value)
{
        char *filePtr, *lastPtr;
	int done = FALSE;
	char *fileString;

			 
	fileString = (char *)strdup( file );
	lastPtr = file;        
	filePtr = strtok( fileString, ":;" );
	while( filePtr && !done ) {
		GetValueFromIniFile( filePtr, section, variable, value );
		if( strlen( value ) ) {
			done = TRUE;
		}
		lastPtr = filePtr;
		filePtr = strtok( NULL, ":;" );
	}
	free( fileString );
}

	       

/**********************************************************************DVM***/
void SetValueInIniFile (char *file, char* section, char* variable, 
               char *value)
{
   char oldFile [250];
   FILE *srcFile, *destFile;
   char line [512];
   char *cptr;
   int operationComplete = FALSE;

   /* Setup The File Path */

   sprintf (oldFile, "%s%s", file, ".bak");

   /* Open The Files */

   remove (oldFile);
   rename (file, oldFile);
   srcFile = fopen (oldFile, "rt");
   if (!srcFile) return;

   destFile = fopen (file, "wt");

   cptr = fgets (line, 512, srcFile);
   while (cptr)
   {
      /* Is this the requested section? */
      if (  line[0] == '['  &&  
            !strncasecmp (&line[1], section, strlen(section))  &&
            line [strlen(section)+1] == ']'  )
      {
         fprintf (destFile, "%s", line);

         cptr = fgets (line, 512, srcFile);
         while (cptr)
         {
            /* Is this the requested variable? */
            if (  !strncasecmp (line, variable, strlen (variable))  &&
                  line [strlen(variable)] == '='  )
            {
               if (!operationComplete)
                  fprintf (destFile, "%s=%s\n", variable, value);
               operationComplete = TRUE;
            }
            /* Is this a new Section? */
            else if (line[0] == '[')
            {
               if (!operationComplete)
               {
                  fprintf (destFile, "%s=%s\n", variable, value);
                  operationComplete = TRUE;
               }
               break;
            }
            else
            {
               fprintf (destFile, "%s", line);
            }

            cptr = fgets (line, 512, srcFile);
         }
      }
      else
      {
         fprintf (destFile, "%s", line);
         cptr = fgets (line, 512, srcFile);
      }
   }

   if (!operationComplete)
   {
      fprintf (destFile, "[%s]\n", section);
      fprintf (destFile, "%s=%s\n", variable, value);
   }

   fclose (srcFile);
   fclose (destFile);
}
