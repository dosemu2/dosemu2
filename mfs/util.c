#include "mangle.h"

int case_default=-1;
BOOL case_mangle=False;
int DEBUGLEVEL=0;

/****************************************************************************
line strncpy but always null terminates. Make sure there is room!
****************************************************************************/
char *StrnCpy(char *dest,char *src,int n)
{
  char *d = dest;
  while (n-- && (*d++ = *src++)) ;
  *d = 0;
  return(dest);
}


/*******************************************************************
  convert a string to upper case
********************************************************************/
void strupper(char *s)
{
  while (*s)
    {
      if (islower(*s))
	*s = toupper(*s);
      s++;
    }
}


/****************************************************************************
does a string have any uppercase chars in it?
****************************************************************************/
BOOL strhasupper(char *s)
{
  while (*s) 
    {
      if (isupper(*s)) return(True);
      s++;
    }
  return(False);
}

/****************************************************************************
does a string have any lowercase chars in it?
****************************************************************************/
BOOL strhaslower(char *s)
{
  while (*s) 
    {
      if (islower(*s)) return(True);
      s++;
    }
  return(False);
}

/****************************************************************************
prompte a dptr (to make it recently used)
****************************************************************************/
void array_promote(char *array,int elsize,int element)
{
  char *p;
  if (element == 0)
    return;

  p = (char *)malloc(elsize);

  if (!p)
    {
      DEBUG(5,("Ahh! Can't malloc\n"));
      return;
    }
  memcpy(p,array + element * elsize, elsize);
  safe_memcpy(array + elsize,array,elsize*element);
  memcpy(array,p,elsize);
  free(p);
}


/*******************************************************************
  convert a string to lower case
********************************************************************/
void strlower(char *s)
{
  while (*s)
    {
      if (isupper(*s))
	  *s = tolower(*s);
      s++;
    }
}


/*******************************************************************
  compare 2 strings 
********************************************************************/
BOOL strequal(char *s1,char *s2)
{
  if (!s1 || !s2) return(False);
  
  return(strcasecmp(s1,s2)==0);
}



