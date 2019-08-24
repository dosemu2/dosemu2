/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "utilities.h"
#include "dos2linux.h"
#include "builtins.h"

#include "msetenv.h"

/*
   envptr - returns pointer to parent command.com's copy of the environment
*/
static char *envptr(int *size, int parent_p)
{
    struct PSP *parent_psp;
    unsigned mcbseg;
    struct MCB *mcb;

    parent_psp = (struct PSP *)SEG2UNIX(parent_p);
    if (parent_psp->envir_frame == 0) {
        error("no env pointer in PSP\n");
        return NULL;
    }
    mcbseg = parent_psp->envir_frame - 1;
    mcb = MK_FP32(mcbseg, 0);
    *size = mcb->size * 16;
    return LINEAR2UNIX(SEGOFF2LINEAR(mcbseg + 1, 0));
}


/*
   msetenv - place an environment variable in command.com's copy of
             the envrionment.
*/

static int com_msetenv(const char *variable, const char *value, int parent_p)
{
    char *env1, *env2;
    char *cp;
    char *var;
    char *tail = NULL;
    int size;
    int l, len, tail_sz = 3;

    cp = envptr(&size, parent_p);
    if (!cp)
        return -1;
    env1 = env2 = cp;
    l = strlen(variable);
    len = l + strlen(value) + 2;
    var = alloca(l+1);
    memcpy(var, variable, l+1);
    strupperDOS(var);

    /*
       Delete any existing variable with the name (var).
    */
    while (*env2) {
        if ((strncmp(var,env2,l) == 0) && (env2[l] == '=')) {
            cp = env2 + strlen(env2) + 1;
            memmove(env2,cp,size-(cp-env1));
            if (tail)
                tail -= cp - env2;
        }
        else {
            env2 += strlen(env2) + 1;
        }
    }

    if (!value || !value[0]) return 0;
    tail = env2;
    cp = tail + 1;
    if (cp[0] == '\1' && cp[1] == '\0')
        tail_sz += strlen(cp + 2) + 1;

    /*
       If the variable fits, shovel it in at the end of the envrionment.
    */
    if (size - (env2 - env1) - tail_sz >= len) {
        memmove(env2 + len, env2, tail_sz);
        strcpy(env2,var);
        strcat(env2,"=");
        strcat(env2,value);
        assert(env2[strlen(env2)+1] == 0);
        return(0);
    }
    error("ENV overflow, size=%i, used=%ti, tail=%i, need=%i\n",
            size, env2 - env1, tail_sz, len);

    /*
       Return error indication if variable did not fit.
    */
    return(-1);
}

/* set to parent env */
int msetenv(const char *var, const char *value)
{
    return com_msetenv(var, value, com_parent_psp_seg());
}

int msetenv_child(const char *var, const char *value)
{
    return com_msetenv(var, value, com_psp_seg());
}

int mresize_env(int size_plus)
{
    int size;
    u_short new_env;
    int err = 0;
    struct PSP *psp = COM_PSP_ADDR;
    char *env = envptr(&size, com_psp_seg());

    if (!env)
        return -1;
    new_env = com_dosallocmem((size + size_plus + 15) >> 4);
    if (!new_env) {
        error("cannot realloc env to %i bytes\n", size + size_plus);
        return -1;
    }
    memcpy(SEG2UNIX(new_env), env, size);
    /* DOS resize (0x4a) can't move :( */
    if (psp->envir_frame)
        err = com_dosfreemem(psp->envir_frame);
    else
        error("no env frame\n");
    if (err)
        error("cant free env frame\n");
    psp->envir_frame = new_env;
    return 0;
}

static char *_mgetenv(const char *variable, char *env, int size)
{
    char *ret = NULL;
    char *var = strdup(variable);
    int l = strlen(var);
    char *env2 = env;

    strupperDOS(var);
    while (*env && env - env2 < size) {
        if ((strncmp(var, env, l) == 0) && (env[l] == '=')) {
            ret = env + l + 1;
            break;
        }
        env += strlen(env) + 1;
    }
    free(var);
    return ret;
}

/* read from current env (not parent) */
char *mgetenv(const char *variable)
{
    int size;
    char *env = envptr(&size, com_psp_seg());

    if (!env)
        return NULL;
    return _mgetenv(variable, env, size);
}
