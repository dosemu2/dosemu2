/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */


#include <dos.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef MSC
#include "borl-dos.h"
#endif

/*
   msetenv - place an environment variable in command.com's copy of
             the envrionment.
*/

int msetenv(var,value)
char *var, *value;
{
    char *envptr();
    char *env1, *env2;
    char *cp;
    int size;
    int l;

    env1 = env2 = envptr(&size);
    l = strlen(var);
    strupr(var);

    /*
       Delete any existing variable with the name (var).
    */
    while (*env2) {
        if ((strncmp(var,env2,l) == 0) && (env2[l] == '=')) {
            cp = env2 + strlen(env2) + 1;
            memcpy(env2,cp,size-(cp-env1));
        }
        else {
            env2 += strlen(env2) + 1;
        }
    }

    /*
       If the variable fits, shovel it in at the end of the envrionment.
    */
    if (strlen(value) && (size-(env2-env1)) >= (l + strlen(value) + 3)) {
        strcpy(env2,var);
        strcat(env2,"=");
        strcat(env2,value);
        env2[strlen(env2)+1] = 0;
        return(0);
    }

    /*
       Return error indication if variable did not fit.
    */
    return(-1);
}

/*
   envptr - returns pointer to parent command.com's copy of the environment
*/
static char *envptr(size)
int *size;
{
    int parent_p;

/*  memory control block */

    struct MCB {
        char id;
        unsigned int owner;
        unsigned int size;
    } far *mcb;

    parent_p=peek(_psp,0x16);    /* find pointer to parent in psp */
    if (peek(parent_p,0x2c)==0) {
       mcb = (struct MCB far *) (((long) (peek(parent_p-1,0x3) + parent_p)) << 16);
    }
    else {
       mcb = (struct MCB far *) (((long) (peek(parent_p,0x2c) - 1)) << 16);
    }
    *size = mcb->size * 16;
    return ((char far *) ((long) (FP_SEG(mcb) + 1) << 16));
}

