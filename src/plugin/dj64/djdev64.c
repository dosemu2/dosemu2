/*
 *  Copyright (C) 2023  stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <djdev64/dj64init.h>
#include "djdev64.h"
#include "dosemu_debug.h"
#include "dos2linux.h"

static uint8_t *dj64_addr2ptr(uint32_t addr)
{
    return dosaddr_to_unixaddr(addr);
}

static void dj64_print(int prio, const char *format, va_list ap)
{
    switch(prio) {
    case DJ64_PRINT_TERMINAL:
        vfprintf(stderr, format, ap);
        break;
    case DJ64_PRINT_LOG:
        if (debug_level('M')) {
            log_printf(-1, "dj64: ");
            vlog_printf(-1, format, ap);
        }
        break;
    }
}

const struct dj64_api api = {
    .addr2ptr = dj64_addr2ptr,
    .print = dj64_print,
};

#if DJ64_API_VER != 1
#error wrong dj64 version
#endif

static int handle;
#define HNDL_MAX 5
struct dj64handle {
    void *dlobj;
    dj64cdispatch_t *cdisp;
    dj64cdispatch_t *ctrl;
};
static struct dj64handle dlhs[HNDL_MAX];

#define __S(x) #x
#define _S(x) __S(x)

int djdev64_open(const char *path)
{
  int h, rc;
  dj64init_t *init;
  dj64init_once_t *init_once;
  dj64dispatch_t *disp;
  dj64cdispatch_t **cdisp;
  dj64symtab_t *st;
  void *dlh = dlopen(path, RTLD_LOCAL | RTLD_NOW);
  if (!dlh) {
    fprintf(stderr, "cannot dlopen %s: %s\n", path, dlerror());
    return -1;
  }
  init_once = dlsym(dlh, _S(DJ64_INIT_ONCE_FN));
  if (!init_once) {
    fprintf(stderr, "cannot find " _S(DJ64_INIT_ONCE_FN) "\n");
    dlclose(dlh);
    return -1;
  }
  init = dlsym(dlh, _S(DJ64_INIT_FN));
  if (!init) {
    fprintf(stderr, "cannot find " _S(DJ64_INIT_FN) "\n");
    dlclose(dlh);
    return -1;
  }
  disp = dlsym(dlh, _S(DJ64_DISPATCH_FN));
  if (!disp) {
    fprintf(stderr, "cannot find " _S(DJ64_DISPATCH_FN) "\n");
    dlclose(dlh);
    return -1;
  }
  st = dlsym(dlh, _S(DJ64_SYMTAB_FN));
  if (!st) {
    fprintf(stderr, "cannot find " _S(DJ64_SYMTAB_FN) "\n");
    dlclose(dlh);
    return -1;
  }
  rc = init_once(&api, DJ64_API_VER);
  if (rc == -1) {
    fprintf(stderr, _S(DJ64_INIT_ONCE_FN) " failed\n");
    dlclose(dlh);
    return -1;
  }
  cdisp = init(handle, disp, st);
  if (!cdisp) {
    fprintf(stderr, _S(DJ64_INIT_FN) " failed\n");
    dlclose(dlh);
    return -1;
  }
  h = handle++;
  dlhs[h].dlobj = dlh;
  dlhs[h].cdisp = cdisp[0];
  dlhs[h].ctrl = cdisp[1];
  return h;
}

int djdev64_call(int handle, int libid, int fn, unsigned char *sp)
{
    if (handle >= HNDL_MAX || !dlhs[handle].dlobj)
        return -1;
    return dlhs[handle].cdisp(handle, libid, fn, sp);
}

int djdev64_ctrl(int handle, int libid, int fn, unsigned char *sp)
{
    if (handle >= HNDL_MAX || !dlhs[handle].dlobj)
        return -1;
    return dlhs[handle].ctrl(handle, libid, fn, sp);
}

void djdev64_close(int handle)
{
    if (handle >= HNDL_MAX)
        return;
    dlclose(dlhs[handle].dlobj);
    dlhs[handle].dlobj = NULL;
}
