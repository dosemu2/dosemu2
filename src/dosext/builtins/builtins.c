/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Purpose: handling of the dosemu-supplied utilities, AKA builtins.
 *
 * Author: Stas Sergeev
 * Some code is taken from coopthreads.c by Hans Lermen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include "dos2linux.h"
#include "doshelpers.h"
#include "cpu.h"
#include "emu.h"
#include "int.h"
#include "utilities.h"
#include "lowmem.h"
#include "coopth.h"
#include "smalloc.h"
#include "redirect.h"
#include "plugin_config.h"
#include "msetenv.h"
#include "builtins.h"

/* hope 2K is enough */
#define LOWMEM_POOL_SIZE 0x800
#define MAX_NESTING 32

static int pool_used = 0;
//#define current_builtin (pool_used - 1)
#define current_builtin pool_used

struct {
    char name[9];
    char *cmd;
    struct lowstring *cmdl;
    struct param4a *pa4;
    uint16_t es;
    uint16_t retcode;
    int allocated;
    run_dos_cb run_dos;
    get_psp_cb get_psp;
} builtin_mem[MAX_NESTING];
#define BMEM(x) (builtin_mem[current_builtin].x)

static char *com_strdup(const char *s);
static void com_strfree(char *s);

char *com_getenv(const char *keyword)
{
	struct PSP  *psp = COM_PSP_ADDR;
	char *env = SEG2UNIX(psp->envir_frame);
	char key[128];
	int len;

	len = strlen(keyword);
	memcpy(key, keyword, len+1);
	strupperDOS(key);
	while (*env) {
		if (!strncmp(key, env, len) && (env[len] == '=')) {
			return env + len + 1;
		}
		env += strlen(env) + 1;
	}
	return 0;
}

static int load_and_run_DOS_program(const char *command, const char *cmdline)
{
	BMEM(pa4) = (struct param4a *)lowmem_heap_alloc(sizeof(struct param4a));
	if (!BMEM(pa4)) return -1;

	BMEM(allocated) = 1;

	BMEM(cmd) = com_strdup(command);
	if (!BMEM(cmd)) {
		com_errno = 8;
		return -1;
	}
	BMEM(cmdl) = (struct lowstring *)lowmem_heap_alloc(256);
	if (!BMEM(cmdl)) {
		com_strfree(BMEM(cmd));
		com_errno = 8;
		return -1;
	}
	if (!cmdline) cmdline = "";
	BMEM(cmdl)->len = strlen(cmdline) + 1;
	snprintf(BMEM(cmdl)->s, 255, " %s\r", cmdline);

	BMEM(es) = SREG(es);
	/* prepare param block */
	BMEM(pa4)->envframe = 0; // ctcb->envir_frame;
	BMEM(pa4)->cmdline = MK_FARt(DOSEMU_LMHEAP_SEG, DOSEMU_LMHEAP_OFFS_OF(BMEM(cmdl)));
	BMEM(pa4)->fcb1 = MK_FARt(com_psp_seg(), offsetof(struct PSP, FCB1));
	BMEM(pa4)->fcb2 = MK_FARt(com_psp_seg(), offsetof(struct PSP, FCB2));
	SREG(es) = DOSEMU_LMHEAP_SEG;
	LWORD(ebx) = DOSEMU_LMHEAP_OFFS_OF(BMEM(pa4));
	/* path of programm to load */
	SREG(ds) = DOSEMU_LMHEAP_SEG;
	LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(BMEM(cmd));

	fake_call_to(BIOSSEG, GET_RETCODE_HELPER);
	LWORD(eax) = 0x4b00;
	real_run_int(0x21);

	return 0;
}

static int do_system(const char *command)
{
	const char *program = com_getenv("COMSPEC");
	char cmdline[128];

	if (!program) program = "C:\\COMMAND.COM";
	snprintf(cmdline, sizeof(cmdline), "/E:2048 /C %s", command);
	coopth_leave();
	fake_iret();
	return load_and_run_DOS_program(program, cmdline);
}

int com_system(const char *command, int quit)
{
	if (quit)
		msetenv("DOSEMU_EXIT", "1");
	return BMEM(run_dos)(command);
}

int com_error(const char *format, ...)
{
	int ret;
	va_list args;
	va_start(args, format);
	ret = com_vprintf(format, args);
	va_end(args);
	va_start(args, format);
	verror(format, args);
	va_end(args);
	return ret;
}

static char *com_strdup(const char *s)
{
	struct lowstring *p;
	int len = strlen(s);
	if (len > 254) {
		error("lowstring too long: %i bytes. builtin: %s\n",
			len, BMEM(name));
		len = 254;
	}

	p = (void *)lowmem_heap_alloc(len + 1 + sizeof(struct lowstring));
	if (!p) return 0;
	p->len = len;
	memcpy(p->s, s, len);
	p->s[len] = 0;
	return p->s;
}

static void com_strfree(char *s)
{
	struct lowstring *p = (void *)(s - 1);
	lowmem_heap_free((char *)p);
}

static int com_argparse(char *s, int i_argc, char **argvx, int maxarg)
{
   int mode = 0;
   int argcx = 0;
   char delim = 0;
   char *p;
   int len;

   len = strlen(s);
   p = strchr(s, '\r');
   if (p && ((p - s) < len))
     *p = 0;

   if (i_argc == 0) {
      int l;
      /* transform:
       *    dir/p to dir /p
       *    cd\ to cd \
       *    cd.. to cd ..
       */
      l = strcspn(s, "\\/. ");
      p = s + l;
      if (*p && *p != ' ' && (*p != '.' || (p[1] && strchr("\\/.", p[1]))) &&
            (!(l > 0 && p[0] == '\\' && p[-1] == ':'))) {
         memmove(p+1, p, len - (p - s) + 1/*NUL*/);
         *p = ' ';
         len++; /* update length */
      }
   }

   maxarg --;
   for ( ; *s; s++) {
      if (!mode) {
         if (*s > ' ') {
            mode = 1;
            switch (*s) {
              case '"':
              case '\'':
                delim = *s;
                mode = 2;
                argvx[argcx++] = s+1;
                break;
              default:
                argvx[argcx++] = s;
                break;
            }
            if (argcx >= maxarg)
               break;
         }
      } else if (mode == 1) {
         if (*s <= ' ') {
            mode = 0;
            *s = 0;
         }
      } else {
         if (*s == delim) {
           *s = 0;
           mode = 1;
         }
      }
   }
   argvx[argcx] = 0;
   return(argcx);
}

int com_dosallocmem(u_short para)
{
    int ret;
    pre_msdos();
    HI(ax) = 0x48;
    LWORD(ebx) = para;
    call_msdos();
    if (REG(eflags) & CF)
        ret = 0;
    else
        ret = LWORD(eax);
    post_msdos();
    return ret;
}

int com_dosfreemem(u_short para)
{
    int ret = 0;
    pre_msdos();
    HI(ax) = 0x49;
    SREG(es) = para;
    call_msdos();
    if (REG(eflags) & CF)
        ret = -1;
    post_msdos();
    return ret;
}

int com_dosgetdrive(void)
{
        int ret;
        pre_msdos();
        HI(ax) = 0x19;
        call_msdos();    /* call MSDOS */
        ret = LO(ax);  /* 0=A, 1=B, ... */
        post_msdos();
        return ret;
}

int com_dossetdrive(int drive)
{
        int ret;
        pre_msdos();
        HI(ax) = 0x0e;
        LO(dx) = drive; /* 0=A, 1=B, ... */
        call_msdos();    /* call MSDOS */
        ret = LO(ax);  /* number of available logical drives */
        post_msdos();
        return ret;
}

int com_dossetcurrentdir(char *path)
{
        /*struct com_starter_seg  *ctcb = owntcb->params;*/
        char *s = com_strdup(path);

        com_errno = 8;
        if (!s) return -1;
        pre_msdos();
        HI(ax) = 0x3b;
        SREG(ds) = DOSEMU_LMHEAP_SEG;
        LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(s);
        call_msdos();    /* call MSDOS */
        com_strfree(s);
        if (LWORD(eflags) & CF) {
                post_msdos();
                return -1;
        }
        post_msdos();
        return 0;
}

static int is_valid_drive(int drv)
{
  uint16_t ret;

  pre_msdos();

  HI(ax) = 0x36; // get free disk space
  LO(dx) = drv;
  call_msdos();
  ret = LWORD(eax);

  post_msdos();
  return ret != 0xffff;
}

int com_FindFreeDrive(void)
{
  int drive;

  for (drive = 2; drive < 26; drive++) {
    if (is_valid_drive(drive + 1))  // 0 = default, 1 = A etc
      continue;
    return drive;
  }

  return -1;
}

/********************************************
 * com_RedirectDevice - redirect a device to a remote resource
 * ON ENTRY:
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 *  resourceStr has a string with the server and name of resource
 *    (ex. 'TIM\TOOLS')
 *  deviceType indicates the type of device being redirected
 *    3 = printer, 4 = disk
 *  deviceParameter is a value to be saved with this redirection
 *  which will be returned on com_GetRedirectionList
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful,
 *  otherwise returns the DOS error code
 * NOTES:
 *  deviceParameter is used in DOSEMU to return the drive attribute
 *  It is not actually saved and returned as specified by the redirector
 *  specification.  This type of usage is common among commercial redirectors.
 ********************************************/
uint16_t com_RedirectDevice(char *deviceStr, char *slashedResourceStr,
                        uint8_t deviceType, uint16_t deviceParameter)
{
  char *dStr = com_strdup(deviceStr);
  char *sStr = com_strdup(slashedResourceStr);
  uint16_t ret = RedirectDevice(dStr, sStr, deviceType, deviceParameter);

  com_strfree(sStr);
  com_strfree(dStr);

  return ret;
}

/********************************************
 * com_CancelRedirection - delete a device mapped to a remote resource
 * ON ENTRY:
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful,
 *  otherwise returns the DOS error code
 * NOTES:
 *
 ********************************************/
uint16_t com_CancelRedirection(char *deviceStr)
{
  char *dStr = com_strdup(deviceStr);
  uint16_t ret;

  pre_msdos();

  SREG(ds) = DOSEMU_LMHEAP_SEG;
  LWORD(esi) = DOSEMU_LMHEAP_OFFS_OF(dStr);
  LWORD(eax) = DOS_CANCEL_REDIRECTION;

  call_msdos();

  ret = (LWORD(eflags) & CF) ? LWORD(eax) : CC_SUCCESS;

  post_msdos();

  com_strfree(dStr);

  return ret;
}

/********************************************
 * com_GetRedirection - get next entry from list of redirected devices
 * ON ENTRY:
 *  redirIndex has the index of the next device to return
 *    this should start at 0, and be incremented between calls
 *    to retrieve all elements of the redirection list
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful, and
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 *  resourceStr has a string with the server and name of resource
 *    (ex. 'TIM\TOOLS')
 *  deviceType indicates the type of device which was redirected
 *    3 = printer, 4 = disk
 *  deviceParameter has my rights to this resource
 * NOTES:
 *
 ********************************************/
uint16_t com_GetRedirection(uint16_t redirIndex, char *deviceStr,
                            char *slashedResourceStr,
                            uint8_t *deviceType, uint16_t *deviceParameter)
{
  char *dStr = lowmem_heap_alloc(16);
  char *sStr = lowmem_heap_alloc(128);
  uint16_t ret, deviceParameterTemp;
  uint8_t deviceTypeTemp;

  pre_msdos();

  SREG(ds) = DOSEMU_LMHEAP_SEG;
  LWORD(esi) = DOSEMU_LMHEAP_OFFS_OF(dStr);
  SREG(es) = DOSEMU_LMHEAP_SEG;
  LWORD(edi) = DOSEMU_LMHEAP_OFFS_OF(sStr);

  LWORD(ebx) = redirIndex;
  LWORD(eax) = DOS_GET_REDIRECTION;

  call_msdos();

  ret = (LWORD(eflags) & CF) ? LWORD(eax) : CC_SUCCESS;

  deviceTypeTemp = LO(bx);
  deviceParameterTemp = LWORD(edx);

  post_msdos();

  if (ret == CC_SUCCESS) {
    /* copy back unslashed portion of resource string */
    strcpy(slashedResourceStr, sStr);
    strcpy(deviceStr, dStr);
    *deviceType = deviceTypeTemp;
    *deviceParameter = deviceParameterTemp;
  }

  lowmem_heap_free(sStr);
  lowmem_heap_free(dStr);

  return ret;
}

struct REGPACK regs_to_regpack(struct vm86_regs *regs)
{
	struct REGPACK regpack;

	regpack.r_ax =  regs->eax;
	regpack.r_bx =  regs->ebx;
	regpack.r_cx =  regs->ecx;
	regpack.r_dx =  regs->edx;
	regpack.r_bp =  regs->ebp;
	regpack.r_si =  regs->esi;
	regpack.r_di =  regs->edi;
	regpack.r_ds =  regs->ds;
	regpack.r_es =  regs->es;
	regpack.r_flags =  regs->eflags;

	return regpack;
}

struct vm86_regs regpack_to_regs(struct REGPACK *regpack)
{
	struct vm86_regs regs = _regs;

	regs.eax = regpack->r_ax;
	regs.ebx = regpack->r_bx;
	regs.ecx = regpack->r_cx;
	regs.edx = regpack->r_dx;
	regs.ebp = regpack->r_bp;
	regs.esi = regpack->r_si;
	regs.edi = regpack->r_di;
	regs.ds = regpack->r_ds;
	regs.es = regpack->r_es;
	regs.eflags = regpack->r_flags;

	return regs;
}

void com_intr(int intno, struct REGPACK *regpack)
{
	struct vm86_regs saved_regs = _regs;
	_regs = regpack_to_regs(regpack);

	do_int_call_back(intno);

	*regpack = regs_to_regpack(&_regs);
	_regs = saved_regs;
}


static struct com_program_entry *com_program_list = 0;

static struct com_program_entry *find_com_program(const char *name)
{
	struct com_program_entry *com = com_program_list;

	while (com) {
		if (!strcmp(com->name, name)) return com;
		com = com->next;
	}
	return 0;
}

void register_com_program(const char *name, com_program_type *program)
{
	struct com_program_entry *com;

	if ((com = find_com_program(name)) == 0) {
		com = malloc(sizeof(struct com_program_entry));
		if (!com) return;
		com->next = com_program_list;
		com_program_list = com;
	}
	com->name = name;
	com->program = program;
}

static char *com_getarg0(void)
{
	char *env = SEG2UNIX(((struct PSP *)SEG2UNIX(COM_PSP_SEG))->envir_frame);
	return memchr(env, 1, 0x10000) + 2;
}

unsigned short com_psp_seg(void)
{
	return BMEM(get_psp)(0);
}

unsigned short com_parent_psp_seg(void)
{
	return BMEM(get_psp)(1);
}

static int unsigned short do_get_psp(int parent)
{
	if (parent) {
		struct PSP *psp = COM_PSP_ADDR;
		unsigned short parent = psp->parent_psp;

		if (!parent) {
			error("No parent PSP\n");
			return com_psp_seg();
		}
		return parent;
	}
	return COM_PSP_SEG;
}

int run_command_plugin(const char *name, const char *argv0, char *cmdbuf,
	run_dos_cb run_cb, get_psp_cb psp_cb)
{
	struct com_program_entry *com;
#define MAX_ARGS 63
	char *args[MAX_ARGS + 1];
	char builtin_name[9];
	char arg0[256];
	int argc = 0;
	int err;

	/* first parse commandline */
	if (argv0) {
		strncpy(arg0, argv0, sizeof(arg0) - 1);
		arg0[sizeof(arg0) - 1] = 0;
		strupperDOS(arg0);
		args[0] = arg0;
		argc++;
	}
	argc += com_argparse(cmdbuf, argc, &args[argc], MAX_ARGS - argc);
	strncpy(builtin_name, name, sizeof(builtin_name) - 1);
	builtin_name[sizeof(builtin_name) - 1] = 0;
	strupperDOS(builtin_name);

	com = find_com_program(builtin_name);
	if (!com) {
//		com_error("inte6: unknown builtin: %s\n", builtin_name);
		return 0;
	}

	BMEM(allocated) = 0;
	BMEM(retcode) = 0;
	strcpy(BMEM(name), builtin_name);
	BMEM(run_dos) = run_cb;
	BMEM(get_psp) = psp_cb;
	err = com->program(argc, args);
	if (err)
		return -1;

	/* if DOS prog spawned, we can't free resources here */
	return 1;
}

int commands_plugin_inte6(void)
{
	char builtin_name[9];
	char cmdbuf[256];
	char *arg0;
	struct PSP *psp;
	struct MCB *mcb;
	int i;
	int err;

	CARRY;		// prevents pligin_done() call
	if (HI(ax) != BUILTINS_PLUGIN_VERSION) {
	    com_error("builtins plugin version mismatch: found %i, required %i\n",
		HI(ax), BUILTINS_PLUGIN_VERSION);
	    com_error("You should update your generic.com, ems.sys, isemu.com and other utilities\n"
		  "from the latest dosemu package!\n");
	    return 0;
	}

	psp = SEG2UNIX(COM_PSP_SEG);
	mcb = SEG2UNIX(COM_PSP_SEG - 1);
	arg0 = com_getarg0();
	/* see if we have valid asciiz name in MCB */
	err = 0;
	for (i = 0; i < 8 && mcb->name[i]; i++) {
		if (!isprint(mcb->name[i])) {
			err = 1;
			break;
		}
	}
	if (!err) {
		/* DOS 4 and up */
		strncpy(builtin_name, mcb->name, sizeof(builtin_name) - 1);
		builtin_name[sizeof(builtin_name) - 1] = 0;
	} else {
		/* DOS 3.0->3.31 construct the program name from the environment */
		char *p = strrchr(arg0, '\\');
		strncpy(builtin_name, p+1, sizeof(builtin_name) - 1);
		builtin_name[sizeof(builtin_name) - 1] = 0;
		p = strchr(builtin_name, '.');
		if (p)
			*p = '\0';
	}
//	assert(psp->cmdline_len < sizeof(cmdbuf)); // len uint8_t, cant assert
	memcpy(cmdbuf, psp->cmdline, psp->cmdline_len);
	cmdbuf[psp->cmdline_len] = '\0';
	if (pool_used >= MAX_NESTING) {
	    com_error("Cannot invoke more than %i builtins\n", MAX_NESTING);
	    return 0;
	}
	pool_used++;
	NOCARRY;
	err = run_command_plugin(builtin_name, arg0, cmdbuf,
			do_system, do_get_psp);
	if (err <= 0) {
		pool_used--;
		CARRY;
	}
	return err;
}

int commands_plugin_inte6_done(void)
{
	if (!pool_used)
	    return 0;

	SREG(es) = BMEM(es);
	LWORD(ebx) = BMEM(retcode);
	if (BMEM(allocated)) {
	    com_strfree(BMEM(cmd));
	    lowmem_heap_free((void *)BMEM(pa4));
	    lowmem_heap_free((void *)BMEM(cmdl));
	}
	pool_used--;
	return 1;
}

int commands_plugin_inte6_set_retcode(void)
{
	if (!pool_used)
	    return 0;

	BMEM(retcode) = LWORD(ebx);
	return 1;
}

void commands_plugin_inte6_reset(void)
{
	pool_used = 0;
}
