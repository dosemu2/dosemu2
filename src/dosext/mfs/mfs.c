/*
 * DANG_BEGIN_MODULE
 *
 * This is the file redirector code for DOSEMU. It was built on the Mach
 * DOS redirector and as such continues that copyright as well in 
 * addition the GNU copyright. This redirector uses the 
 * DOS int2f fnx 11 calls to give running DOS programs access to any 
 * Unix mounted drives that permissions exist for.
 *
 * DANG_END_MODULE
 * DANG_FIXTHIS We probably should use llseek here for file > 2 GBytes
 *
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * Purpose:
 *	V86 DOS disk emulation routines
 *
 * HISTORY:
 * $Log: mfs.c,v $

Work started by Tim Bird (tbird@novell.com) 28th October 1993

	
	added support for CONTROL_REDIRECTION in dos_fs_redirect.
	removed my_drive, and made drive arrays 0-based instead of
	first_drive based.


This module is a port of the redirector handling in the mach dos
emulator to the linux dos emulator. Started by
Andrew.Tridgell@anu.edu.au on 30th March 1993

version 1.4 12th May 1993

      added a cds_flags check in select_drive so the pointers can
      be recalculated when the cds moves. This means you can use the
      emufs drive immediately after it installs, even using it to load
      other device drivers in config.sys

      fixed the volume label bug, so now volume labels are enabled by default.
      We really should do something more useful with them.

      ran "indent" on mfs.c so emacs wouldn't choke in C mode

      did a quick fix for nested finds so they work if the inner find
      didn't finish.  I don't think it's quite right yet, probably
      find_first/next need to be completely rewritten to get perfect
      behaviour, especially for programs that may try to hack directly with
      the search template/directory entry during a find.

      added Roberts patches so the root directory gets printed when initialised

version 1.3 9th may 1993

      completely revamped find_file() so it can match mixed case names.
      This doesn't seem to have cost as much in speed as I thought
      it would. This actually involved changes to quite a few routines for
      it to work properly.

      added a check for the dos root passed to init_drive - you now get
      a "server not responding" error with a bad path - this is still not good
      but's it's better than just accepting it.

version 1.2 8th may 1993

      made some changes to get_dir() which make the redirector MUCH faster.
      It no longer stat's every file when searching for a file. This
      is most noticible when you have a long dos path.

      also added profiling. This is supported by profile.c and profile.h.

version 1.2 7th may 1993

      more changes from Stephen Tweedie to fix the dos root length
      stuff, hopefully it will now work equally well with DRDOS and MSDOS.

      also change the way the calculate_dos_pointers() works - it's
      now done drive by drive which should be better

      also fixed for MS-DOS 6. It turned out that the problem was that
      an earlier speculative dos 6 compatability patch I did managed to get
      lost in all the 0.49v? ungrades. re-applying it fixed the problem.
      *grumble*. While I was doing that I fixed up non-cds filename finding
      (which should never be used anyway as we have a valid cds), and added
      a preliminary qualify_filename (which is currently turned off
      as it's broken)

version 1.1 1st May 1993

      incorporated patches from Stephen Tweedie to
      fix things for DRDOS and to make the file attribute handling
      a lot saner. (thanks Stephen!)

      The main DRDOS change is to handle the fact that filenames appear
      to be in <DRIVE><PATH> format rather than <DRIVE>:\<PATH>. Hopefully
      the changes will not affect the operation under MS-DOS

version 1.0 17th April 1993
changes from mach dos emulator version 2.5:

   - changed SEND_FROM_EOF to SEEK_FROM_EOF and added code (untested!)
   - added better attribute mapping and set attribute capability
   - remove volume label to prevent "dir e:" bug
   - made get_attribute return file size
   - added multiple drive capability via multiple .sys files in config.sys
   - fixed so compatible with devlod so drives can be added on the fly
   - allow unix directory to be chosen with command line in config.sys
   - changed dos side (.asm) code to only use 1 .sys file (removed
     need for mfsini.exe)
   - ported linux.asm to use as86 syntax (Robert Sanders did this - thanks!)
   - added read-only option on drives
   - totally revamped drive selection to match Ralf's interrupt list
   - made reads and writes faster by not doing 4096 byte chunks
   - masked sigalrm in read and write to prevent crash under NFS
   - gave delete_file it's own hlist to prevent clashes with find_next

TODO:
   - fix volume labels
   - get filename qualification working properly
   - anything else???

*/

#if defined(__linux__) || defined(__NetBSD__)
#define DOSEMU 1		/* this is a port to dosemu */
#endif

#if PROFILE
#include "profile.h"
#else
#define PS(x)
#define PE(x)
#endif


#if !DOSEMU
#include "base.h"
#include "bios.h"
#endif

#include <stdio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <stdlib.h>
#include <malloc.h>
#include <utime.h>

#if !DOSEMU
#include <mach/message.h>
#include <mach/exception.h>

#include "dos.h"
#else
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include "mfs.h"
#include "config.h"
/* For passing through GetRedirection Status */
#include "memory.h"
#include "mangle.h"
#endif

/* these universal globals defined here (externed in dos.h) */
boolean_t mach_fs_enabled = FALSE;

#define INSTALLATION_CHECK	0x0
#define	REMOVE_DIRECTORY	0x1
#define	REMOVE_DIRECTORY_2	0x2
#define	MAKE_DIRECTORY		0x3
#define	MAKE_DIRECTORY_2	0x4
#define	SET_CURRENT_DIRECTORY	0x5
#define	CLOSE_FILE		0x6
#define	COMMIT_FILE		0x7
#define	READ_FILE		0x8
#define	WRITE_FILE		0x9
#define	LOCK_FILE_REGION	0xa
#define	UNLOCK_FILE_REGION	0xb
#define	GET_DISK_SPACE		0xc
#define	SET_FILE_ATTRIBUTES	0xe
#define	GET_FILE_ATTRIBUTES	0xf
#define RENAME_FILE		0x11
#define	DELETE_FILE		0x13
#define	OPEN_EXISTING_FILE	0x16
#define	CREATE_TRUNCATE_FILE	0x17
#define	CREATE_TRUNCATE_NO_CDS	0x18
#define	FIND_FIRST_NO_CDS	0x19
#define	FIND_FIRST		0x1b
#define	FIND_NEXT		0x1c
#define	CLOSE_ALL		0x1d
#define	CONTROL_REDIRECT	0x1e
#define PRINTER_SETUP		0x1f
#define	FLUSH_ALL_DISK_BUFFERS	0x20
#define	SEEK_FROM_EOF		0x21
#define	PROCESS_TERMINATED	0x22
#define	QUALIFY_FILENAME	0x23
/*#define TURN_OFF_PRINTER	0x24 */
#define MULTIPURPOSE_OPEN	0x2e	/* Used in DOS 4.0+ */
#define PRINTER_MODE  		0x25	/* Used in DOS 3.1+ */
#define EXTENDED_ATTRIBUTES	0x2d	/* Used in DOS 4.x */

#define EOS		'\0'
#define	SLASH		'/'
#define BACKSLASH	'\\'

/* Need to know how many drives are redirected */
static u_char redirected_drives = 0;

static int calculate_drive_pointers(int);
static boolean_t dos_fs_dev(state_t *);
static boolean_t find_file(char *, struct stat *);
static boolean_t compare(char *, char *, char *, char *);
static int dos_fs_redirect(state_t *);

/* dos_disk.c */
static struct dir_ent *get_dir();
static void auspr();

static boolean_t drives_initialized = FALSE;
static char *dos_roots[MAX_DRIVE];
static int dos_root_lens[MAX_DRIVE];
static boolean_t read_onlys[MAX_DRIVE];
static boolean_t finds_in_progress[MAX_DRIVE];
static boolean_t find_in_progress = FALSE;
static int current_drive = 0;
static u_char first_free_drive = 0;
static int num_drives = 0;
static int process_mask = 0;
static boolean_t read_only = FALSE;

static lol_t lol = NULL;
static far_t cdsfarptr;
static cds_t cds_base;
static cds_t cds;
static sda_t sda;

static int dos_major;
static int dos_minor;

static char *dos_root = "";
static int dos_root_len = 0;

/* initialize 'em to 3.1 to 3.3 */

int sdb_drive_letter_off = 0x0;
int sdb_template_name_off = 0x1;
int sdb_template_ext_off = 0x9;
int sdb_attribute_off = 0xc;
int sdb_dir_entry_off = 0xd;
int sdb_p_cluster_off = 0xf;
int sdb_file_name_off = 0x15;
int sdb_file_ext_off = 0x1d;
int sdb_file_attr_off = 0x20;
int sdb_file_time_off = 0x2b;
int sdb_file_date_off = 0x2d;
int sdb_file_st_cluster_off = 0x2f;
int sdb_file_size_off = 0x31;

int sft_handle_cnt_off = 0x0;
int sft_open_mode_off = 0x2;
int sft_attribute_byte_off = 0x4;
int sft_device_info_off = 0x5;
int sft_dev_drive_ptr_off = 0x7;
int sft_fd_off = 0xb;
int sft_start_cluster_off = 0xb;
int sft_time_off = 0xd;
int sft_date_off = 0xf;
int sft_size_off = 0x11;
int sft_position_off = 0x15;
int sft_rel_cluster_off = 0x19;
int sft_abs_cluster_off = 0x1b;
int sft_directory_sector_off = 0x1d;
int sft_directory_entry_off = 0x1f;
int sft_name_off = 0x20;
int sft_ext_off = 0x28;

int cds_record_size = 0x51;
int cds_current_path_off = 0x0;
int cds_flags_off = 0x43;
int cds_DBP_pointer_off = 0x45;
int cds_cur_cluster_off = 0x49;
int cds_rootlen_off = 0x4f;

int sda_current_dta_off = 0xc;
int sda_cur_psp_off = 0x10;
int sda_filename1_off = 0x92;
int sda_filename2_off = 0x112;
int sda_sdb_off = 0x192;
int sda_cds_off = 0x26c;
int sda_search_attribute_off = 0x23a;
int sda_open_mode_off = 0x23b;
int sda_rename_source_off = 0x2b8;
int sda_user_stack_off = 0x250;

int lol_cdsfarptr_off = 0x16;
int lol_last_drive_off = 0x21;

/*
 * These offsets only meaningful for DOS 4 or greater:
 */
int sda_ext_act_off = 0x2dd;
int sda_ext_attr_off = 0x2df;
int sda_ext_mode_off = 0x2e1;

/* here are the functions used to interface dosemu with the mach
   dos redirector code */

struct direct *dos_readdir(DIR *);

#if DOSEMU
#include "priv.h"


#endif

void build_ufs_path(char *ufs, char *path);
char *is_reserved_msdos (char *s);

/* Try and work out if the current command is for any of my drives */
static int
select_drive(state)
     state_t *state;
{
  int dd;
  int bs_pos, i;
  char fpath[1024];
  boolean_t found = 0;
  boolean_t check_cds = FALSE;
  boolean_t check_dpb = FALSE;
  boolean_t check_esdi_cds = FALSE;
  boolean_t check_sda_ffn = FALSE;
  boolean_t check_always = FALSE;
  boolean_t check_dssi_fn = FALSE;

  cds_t sda_cds = sda_cds(sda);
  cds_t esdi_cds = (cds_t) Addr(state, es, edi);
  sft_t sft = (u_char *) Addr(state, es, edi);
  int fn = LOW(state->eax);

  cdsfarptr = lol_cdsfarptr(lol);
  cds_base = (cds_t) Addr_8086(cdsfarptr.segment, cdsfarptr.offset);

  Debug0((dbg_fd, "selecting drive fn=%x sda_cds=%p\n",
	  fn, (void *) sda_cds));

  switch (fn) {
  case INSTALLATION_CHECK:	/* 0x0 */
  case CONTROL_REDIRECT:	/* 0x1e */
    check_always = TRUE;
    break;
  case QUALIFY_FILENAME:	/* 0x23 */
    check_dssi_fn = TRUE;
    break;
  case REMOVE_DIRECTORY:	/* 0x1 */
  case REMOVE_DIRECTORY_2:	/* 0x2 */
  case MAKE_DIRECTORY:		/* 0x3 */
  case MAKE_DIRECTORY_2:	/* 0x4 */
  case SET_CURRENT_DIRECTORY:	/* 0x5 */
  case SET_FILE_ATTRIBUTES:	/* 0xe */
  case GET_FILE_ATTRIBUTES:	/* 0xf */
  case RENAME_FILE:		/* 0x11 */
  case DELETE_FILE:		/* 0x13 */
  case CREATE_TRUNCATE_FILE:	/* 0x17 */
  case FIND_FIRST:		/* 0x1b */
    check_cds = TRUE;
    break;

  case CLOSE_FILE:		/* 0x6 */
  case COMMIT_FILE:		/* 0x7 */
  case READ_FILE:		/* 0x8 */
  case WRITE_FILE:		/* 0x9 */
  case LOCK_FILE_REGION:	/* 0xa */
  case UNLOCK_FILE_REGION:	/* 0xb */
  case SEEK_FROM_EOF:		/* 0x21 */
    check_dpb = TRUE;
    break;

  case GET_DISK_SPACE:		/* 0xc */
    check_esdi_cds = TRUE;
    break;

  case OPEN_EXISTING_FILE:	/* 0x16 */
  case MULTIPURPOSE_OPEN:	/* 0x2e */
  case FIND_FIRST_NO_CDS:	/* 0x19 */
  case CREATE_TRUNCATE_NO_CDS:	/* 0x18 */
    check_sda_ffn = TRUE;
    break;

  default:
    check_cds = TRUE;
    break;
   
 /* The rest are unknown - assume check_cds */
 /*
	case CREATE_TRUNCATE_NO_DIR	0x18
	case FIND_NEXT		0x1c
	case CLOSE_ALL		0x1d
	case FLUSH_ALL_DISK_BUFFERS	0x20
	case PROCESS_TERMINATED	0x22
	case UNDOCUMENTED_FUNCTION_2	0x25
 */
  }

  /* re-init the cds stuff for any drive that I think is mine, but
	where the cds flags seem to be unset. This allows me to reclaim a
 	drive in the strange and unnatural case where the cds has moved. */
  for (dd = 0; dd < num_drives && !found; dd++)
    if (dos_roots[dd] && (cds_flags(drive_cds(dd)) &
			  (CDS_FLAG_REMOTE | CDS_FLAG_READY)) !=
	(CDS_FLAG_REMOTE | CDS_FLAG_READY)) {
      calculate_drive_pointers(dd);
    }

  if (check_always)
    found = 1;

  if (!found && check_cds)
    for (dd = 0; dd < num_drives && !found; dd++) {
      if (!dos_roots[dd])
	continue;
      if (drive_cds(dd) == sda_cds)
	found = 1;
    }

  if (!found && check_esdi_cds)
    for (dd = 0; dd < num_drives && !found; dd++) {
      if (!dos_roots[dd])
	continue;
      if (drive_cds(dd) == esdi_cds)
	found = 1;
    }

  if (!found && check_dpb)
    for (dd = 0; dd < num_drives && !found; dd++) {
      if (!dos_roots[dd])
	continue;
        /* 11/20/95 by RGPP:
          Some Novell applications seem to trash the sft_device_info
          in such a way that it looks like an existing DOSEMU re-
          directed drive.  This condition seems to be detectable as
          a non-zero value in the second four bits of the entry.
	*/
	if ((sft_device_info(sft) & 0x0f1f) == dd)
	found = 1;
    }

  if (!found && check_sda_ffn) {
    char *fn1 = sda_filename1(sda);

    for (dd = 0; dd < num_drives && !found; dd++) {
      if (!dos_roots[dd])
	continue;
      /* removed ':' check so DRDOS would be happy,
	     there is a slight worry about possible device name
	     troubles with this - will PRN ever be interpreted as P:\RN ? */
      if ((toupper(fn1[0]) - 'A') == dd)
	found = 1;
    }
  }

  if (!found && check_dssi_fn) {

    char *nametmp;
    char *name = (char *) Addr(state, ds, esi);

    nametmp = (char *) malloc(MAXPATHLEN);
    strcpy(nametmp, cds_current_path(cds));
    Debug0((dbg_fd, "FNX=%.15s\n", name));
    for (dd = 0; dd < num_drives && !found; dd++) {
      if (!dos_roots[dd])
	continue;
      if ((toupper(name[0]) - 'A') == dd
	  && (name[1] == ':' || name[1] == '\\'))
	found = 1;
    }

    if (found && name[0] == '.' && name[1] == '\\') {
      Debug0((dbg_fd, "MFS: ---> %s\n", nametmp));
      if (nametmp[2] == '\\')
	strcat(nametmp, &name[2]);
      else
	strcat(nametmp, &name[1]);
      strcpy(name, nametmp);
      Debug0((dbg_fd, "name changed to %s\n", name));
    }

    build_ufs_path(fpath, name);
    bs_pos=0;
    for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
      if (fpath[i] == SLASH)
	bs_pos = i;
    }
    free(nametmp);
  }

  /* for find next we will check the drive letter in the
     findfirst block which is in the DTA */
  if (!found && fn == FIND_NEXT) {
    u_char *dta = sda_current_dta(sda);

    for (dd = 0; dd < num_drives && !found; dd++) {
      if (!dos_roots[dd])
	continue;
      if ((*dta & ~0x80) == dd)
	found = 1;
    }
  }

  if (!found) {
    Debug0((dbg_fd, "no drive selected fn=%x\n", fn));
    if (fn == 0x1f) {
      SETWORD(&(state->ebx), WORD(state->ebx) - redirected_drives);
      Debug0((dbg_fd, "Passing %d to PRINTER SETUP anyway\n",
	      (int) WORD(state->ebx)));
    }
    return (0);
  }

  /* dd is always 1 too large here because of for loop structure above */
  current_drive = dd - 1;

  cds = drive_cds(current_drive);
  find_in_progress = finds_in_progress[current_drive];
  dos_root = dos_roots[current_drive];
  dos_root_len = dos_root_lens[current_drive];
  read_only = read_onlys[current_drive];

  Debug0((dbg_fd, "selected drive %d: %s\n", current_drive, dos_root));
  return (1);
}

static boolean_t
is_hidden(char *fname)
{
  char *p = strrchr(fname,'/');
  if (p) fname = p+1;
  return(fname[0] == '.' && strcmp(fname,"..") && fname[1]);
}

static int
get_dos_attr(int mode,boolean_t hidden)
{
  int attr = 0;

  if (S_ISDIR(mode) && !S_ISCHR(mode) && !S_ISBLK(mode))
    attr |= DIRECTORY;
  if (!(mode & S_IWRITE))
    attr |= READ_ONLY_FILE;
  if (!(mode & S_IEXEC))
    attr |= ARCHIVE_NEEDED;
  if (hidden)
    attr |= HIDDEN_FILE;
  return (attr);
}

static int
get_unix_attr(int mode, int attr)
{
	enum { S_IWRITEA = S_IWUSR | S_IWGRP | S_IWOTH };

#if 0
#define S_IWRITEA (S_IWUSR | S_IWGRP | S_IWOTH)
#endif
  mode &= ~(S_IFDIR | S_IWRITE | S_IEXEC);
  if (attr & DIRECTORY)
    mode |= S_IFDIR;
  if (!(attr & READ_ONLY_FILE))
    mode |= S_IWRITEA;
  if (!(attr & ARCHIVE_NEEDED))
    mode |= S_IEXEC;
  /* Apply current umask to rwx_group and rwx_other permissions */
  mode &= ~(process_mask & (S_IRWXG | S_IRWXO));
  /* Don't set WG or WO unless corresponding Rx is set! */
  mode &= ~(((mode & S_IROTH) ? 0 : S_IWOTH) |
	    ((mode & S_IRGRP) ? 0 : S_IWGRP));
  return (mode);
}

static int
get_disk_space(char *cwd, int *free, int *total)
{
  struct statfs fsbuf;

  if (statfs(cwd, &fsbuf) >= 0) {
    *free = (fsbuf.f_bsize / 512) * fsbuf.f_bavail;
    *total = (fsbuf.f_bsize / 512) * fsbuf.f_blocks;
    return (1);
  }
  else
    return (0);
}

static void
init_all_drives(void)
{
  int dd;

  if (!drives_initialized) {
    Debug0((dbg_fd, "Inside initialization\n"));
    drives_initialized = TRUE;
    for (dd = 0; dd < MAX_DRIVE; dd++) {
      dos_roots[dd] = NULL;
      dos_root_lens[dd] = 0;
      finds_in_progress[dd] = FALSE;
      read_onlys[dd] = FALSE;
    }
    process_mask = umask(0);
    umask(process_mask);
  }
}

/*
  *  this routine takes care of things
  * like getting environment variables
  * and such.  \ is used as an escape character.
  * \\ -> \
  * \${VAR} -> value of environment variable VAR
  * \T -> current tmp directory
  *
  */
static void
get_unix_path(char *new_path, char *path)
{
  char str[MAXPATHLEN];
  char var[256];
  char *orig_path = path;
  char *val;
  char *tmp_str;
  int i;
  int esc;

  i = 0;
  esc = 0;
  for (; *path != 0; path++) {
    if (esc) {
      switch (*path) {
      case '\\':		/* really meant a \ */
	str[i++] = '\\';
	break;
      case 'T':		/* the name of the temporary directory */
	strncpy(&str[i], tmpdir, MAXPATHLEN - 2 - i);
	str[MAXPATHLEN - 2] = 0;
	i = strlen(str);
	break;
      case '$':		/* substitute an environment variable. */
	path++;
	if (*path != '{') {
	  var[0] = *path;
	  var[1] = 0;
	}
	else {
	  for (tmp_str = var; *path != 0 && tmp_str != &var[255]; tmp_str++) {
	    path++;
	    if (*path == '}')
	      break;
	    *tmp_str = *path;
	  }
	  *tmp_str = 0;
	  val = getenv(var);
	  if (val == NULL) {
	    Debug0((dbg_fd,
		    "In path=%s, Environment Variable $%s is not set.\n",
		    orig_path, var));
	    break;
	  }
	  strncpy(&str[i], val, MAXPATHLEN - 2 - i);
	  str[MAXPATHLEN - 2] = 0;
	  i = strlen(str);
	  esc = 0;
	  break;
	}
      }
    }
    else {
      if (*path == '\\') {
	esc = 1;
      }
      else {
	str[i++] = *path;
      }
    }
    if (i >= MAXPATHLEN - 2) {
      i = MAXPATHLEN - 2;
      break;
    }
  }
  if (i == 0 || str[i - 1] != '/')
    str[i++] = '/';
  str[i] = 0;
  strcpy(new_path, str);
  Debug0((dbg_fd,
	  "unix_path returning %s from %s.\n",
	  new_path, orig_path));
  return;
}

static int
init_drive(int dd, char *path, char *options)
{
  struct stat st;
  char *new_path;

  new_path = malloc(MAXPATHLEN + 1);
  if (new_path == NULL) {
    Debug0((dbg_fd,
	    "Out of memory in path %s.\n",
	    path));
    return ((int) NULL);
  }
  get_unix_path(new_path, path);
  Debug0((dbg_fd, "new_path=%s\n", new_path));
  dos_roots[dd] = new_path;
  dos_root_lens[dd] = strlen(dos_roots[dd]);
  Debug0((dbg_fd, "next_aval %d path %s opts %s root %s length %d\n",
	  dd, path, options, dos_roots[dd], dos_root_lens[dd]));

  /* now a kludge to find the true name of the path */
  if (dos_root_lens[dd] != 1) {
    boolean_t find_file();

    dos_roots[dd][dos_root_lens[dd] - 1] = 0;
    dos_root_len = 1;
    dos_root = "/";
    if (!find_file(dos_roots[dd], &st)) {
      error("MFS ERROR: couldn't find root path %s\n", dos_roots[dd]);
      return (0);
    }
    if (!(st.st_mode & S_IFDIR)) {
      error("MFS ERROR: root path is not a directory %s\n", dos_roots[dd]);
      return (0);
    }
    dos_roots[dd][dos_root_lens[dd] - 1] = '/';
  }

  if (num_drives <= dd)
    num_drives = dd + 1;
  read_onlys[dd] = (options && (toupper(options[0]) == 'R'));
  Debug0((dbg_fd, "initialised drive %d as %s with access of %s\n", dd, dos_roots[dd],
	  read_onlys[dd] ? "READ_ONLY" : "READ_WRITE"));
#if 0  
  calculate_drive_pointers (dd);
#endif
  return (1);
}

/***************************
 * mfs_redirector - perform redirector emulation for int 2f, ah=11
 * on entry - nothing
 * on exit - returns non-zero if call was handled
 * 	returns 0 if call was not handled, and should be passed on.
 * notes:
 ***************************/
int
mfs_redirector(void)
{
  int dos_fs_redirect();
  int ret;

  PS(MFS);
  if (!enter_priv_off()) {
    leave_priv_setting();
    return 0;
  }

  ret = dos_fs_redirect(&REGS);
  leave_priv_setting();
  PE(MFS);

  Debug0((dbg_fd, "Finished dos_fs_redirect\n"));

  finds_in_progress[current_drive] = find_in_progress;

  switch (ret) {
  case FALSE:
    REG(eflags) |= CF;
    return 1;
  case TRUE:
    REG(eflags) &= ~CF;
    return 1;
  case UNCHANGED:
    return 1;
  case REDIRECT:
    return 0;
  }

  return 0;
}

int
mfs_inte6(void)
{
  boolean_t dos_fs_dev();
  boolean_t result;

  if (!enter_priv_off()) {
    leave_priv_setting();
    return 0;
  }
  result = dos_fs_dev(&REGS);
  leave_priv_setting();
  return (result);
}

/* include a few necessary functions from dos_disk.c in the mach
   code as well */
static boolean_t
extract_filename(filename, name, ext)
     char *filename;
     char *name;
     char *ext;
{
  int pos;
  int dec_found;
  boolean_t invalid;
  int end_pos;
  int slen;
  int flen;

  pos = 1;
  end_pos = 0;
  dec_found = 0;
  invalid = FALSE;
  while ((pos < 13) && !invalid) {
    char ch = filename[pos];

    if (ch == 0) {
      end_pos = pos - 1;
      pos = 20;
      continue;
    }
    if (dec_found) {
      /* are there more than one .'s ? */
      if (ch == '.') {
	invalid = TRUE;
	continue;
      }
      /* is extension > 3 long ? */
      if (pos - dec_found > 3) {
	invalid = TRUE;
	continue;
      }
    }
    else {
      /* is filename > 8 long ? */
      if ((pos > 7) && (ch != '.')) {
	invalid = TRUE;
	continue;
      }
    }
	if (ch == '.')
	{
	  dec_found = pos;
	}
	else if (strchr("\"/\\[]:<>+=;,", ch) != NULL)
	{
	  invalid = TRUE;
	}
    pos++;
  }
  if (invalid)
    return (FALSE);

  if ((pos > 11) && (pos != 20))
    return (FALSE);

  if (dec_found == 0) {
    slen = end_pos + 1;
  }
  else {
    slen = dec_found;
  }
  strncpy(name, filename, slen);
  if (slen < 8) {
    if ((flen = 8 - slen) > 0)
      strncpy((name + slen), "        ", flen);
  }

  if (dec_found) {
    if (end_pos) {
      slen = end_pos - dec_found;
    }
    else {
      slen = 3;
    }
    strncpy(ext, (filename + dec_found + 1), slen);
    if (3 - slen > 0)
      strncpy((ext + slen), "   ", 3 - slen);
  }
  else {
    strncpy(ext, "   ", 3);
  }

  for (pos = 0; pos < 8; pos++) {
    char ch = name[pos];

    if (isalpha(ch) && islower(ch))
      name[pos] = toupper(ch);
  }

  for (pos = 0; pos < 3; pos++) {
    char ch = ext[pos];

    if (isalpha(ch) && islower(ch))
      ext[pos] = toupper(ch);
  }

  return (TRUE);
}

static struct dir_ent *
make_entry(void)
{
  struct dir_ent *entry;

/* DANG_FIXTHIS returned size of struct dir_ent seems wrong at 28 bytes. */
/* DANG_BEGIN_REMARK
 * The msdos_dir_ent structure has much more than 28 bytes. 
 * Is this significant?
 * DANG_END_FUNCTION
 */
  entry = (struct dir_ent *) malloc(sizeof(struct dir_ent)+10);
  entry->hidden = FALSE;
  entry->next = NULL;
  return (entry);
}

static  void
free_list(list)
     struct dir_ent *list;
{
  struct dir_ent *next;

  if (list == NULL)
    return;

  while (list->next != NULL) {
    next = list->next;
    free(list);
    list = next;
  }
  free(list);
}

static  struct dir_ent *
_get_dir(char *name, char *mname, char *mext)
{
  DIR *cur_dir;
  struct direct *cur_ent;
  struct dir_ent *dir_list;
  struct dir_ent *entry;
  struct stat sbuf;
  int slen;
  char buf[256];
  char *sptr;
  char fname[8];
  char fext[3];
/*
  boolean_t find_file();
*/
  boolean_t compare();

  if(!find_file(name, &sbuf))
     return NULL;

  if ((cur_dir = opendir(name)) == NULL) {
    extern int errno;

    Debug0((dbg_fd, "get_dir(): couldn't open '%s' errno = %s\n", name, strerror(errno)));
    return (NULL);
  }

  Debug0((dbg_fd, "get_dir() opened '%s'\n", name));

  dir_list = NULL;
  entry = dir_list;

/* DANG_BEGIN_REMARK
 * 
 * Added compares to NUL so that newer versions of Foxpro which test directories
 * using xx\yy\nul perform closer to whats DOS does.
 * 
 * DANG_END_FUNCTION
 */
  if (strncasecmp(mname, "NUL     ", strlen(mname)) == 0 &&
      strncasecmp(mext, "   ", strlen(mext)) == 0) {

    entry = make_entry();
    dir_list = entry;
    entry->next = NULL;

    strncpy(entry->name, mname, 8);
    strncpy(entry->ext, mext, 3);
    entry->mode = S_IFREG;
    entry->size = 0;
    entry->time = 0;

    closedir(cur_dir);
    return (dir_list);
  }
  else
    while ((cur_ent = dos_readdir(cur_dir))) {
      char tmpname[100];
      int namlen;

      /* this while loop can take a _long_ time ... better avoid signal queue
       *overflows AV
       */
      handle_signals();

      Debug0((dbg_fd, "get_dir(): `%s' (%d)\n", cur_ent->d_name,
	       cur_ent->d_namlen));
      if (cur_ent->d_ino == 0)
	continue;

      if (!name_convert(tmpname,cur_ent->d_name,MANGLE,NULL))
	continue;

      namlen = strlen(tmpname);
	
      if (tmpname[0] == '.') {
	if (namlen > 2)
	  continue;
	if (strncasecmp(name, dos_root, strlen(name)) == 0)
	  continue;
	if ((namlen == 2) &&
	    (tmpname[1] != '.'))
	  continue;
	strncpy(fname, "..      ", namlen);
	strncpy(fname + namlen, "        ",
		8 - namlen);
	strncpy(fext, "   ", 3);
      }
      else {
	if (!extract_filename(tmpname, fname, fext))
	  continue;
	if (mname && mext && !compare(fname, fext, mname, mext))
	  continue;
      }

      if (entry == NULL) {
	entry = make_entry();
	dir_list = entry;
      }
      else {
	entry->next = make_entry();
	entry = entry->next;
      }
      entry->next = NULL;

      strncpy(entry->name, fname, 8);
      strncpy(entry->ext, fext, 3);

      entry->hidden = is_hidden(cur_ent->d_name);

      strcpy(buf, name);
      slen = strlen(buf);
      sptr = buf + slen + 1;
      buf[slen] = '/';
      strcpy(sptr, cur_ent->d_name);

      if (!find_file(buf, &sbuf)) {
	Debug0((dbg_fd, "Can't findfile %s\n", buf));
	entry->mode = S_IFREG;
	entry->size = 0;
	entry->time = 0;
      }
      else {
	entry->mode = sbuf.st_mode;
	entry->size = sbuf.st_size;
	entry->time = sbuf.st_mtime;
      }

    }
  closedir(cur_dir);
  return (dir_list);
}

static __inline__ struct dir_ent *
get_dir(char *name, char *fname, char *fext)
{
  struct dir_ent *r;

  PS(GETDIR);
  r = _get_dir(name, fname, fext);
  PE(GETDIR);
  return (r);
}

/*
 * Another useless specialized parsing routine!
 * Assumes that a legal string is passed in.
 */
static void
auspr(filestring0, name, ext)
     char *filestring0;
     char *name;
     char *ext;
{
  char filestring[100];

  int pos = 0;
  int dot_pos = 0;
  int elen;

  name_convert(filestring,filestring0,MANGLE,NULL);

  Debug1((dbg_fd, "auspr '%s'\n", filestring));
  for (pos = 0;; pos++) {
    if (filestring[pos] == '.') {
      dot_pos = pos;
      continue;
    }
    if (filestring[pos] == '\0')
      break;
  }

  if (dot_pos > 0) {
    strncpy(name, filestring, dot_pos);
    if (8 - dot_pos > 0)
      strncpy(name + dot_pos, "        ", 8 - dot_pos);
    elen = pos - dot_pos - 1;
    strncpy(ext, filestring + dot_pos + 1, elen);
    if (3 - elen > 0)
      strncpy(ext + elen, "   ", 3 - elen);
  }
  else {
    strncpy(name, filestring, pos);
    if (8 - pos > 0)
      strncpy(name + pos, "        ", 8 - pos);
    strncpy(ext, "   ", 3);
  }

  for (pos = 0; pos < 8; pos++) {
    char ch = name[pos];

    if (isalpha(ch) && islower(ch))
      name[pos] = toupper(ch);
  }

  for (pos = 0; pos < 3; pos++) {
    char ch = ext[pos];

    if (isalpha(ch) && islower(ch))
      ext[pos] = toupper(ch);
  }

  Debug0((dbg_fd,"auspr(%s,%.8s,%.3s)\n",filestring0,name,ext));

}

static void
init_dos_offsets(ver)
     int ver;
{
  Debug0((dbg_fd, "dos_fs: using dos version = %d.\n", ver));
  switch (ver) {
  case DOSVER_31_33:
    {
      sdb_drive_letter_off = 0x0;
      sdb_template_name_off = 0x1;
      sdb_template_ext_off = 0x9;
      sdb_attribute_off = 0xc;
      sdb_dir_entry_off = 0xd;
      sdb_p_cluster_off = 0xf;
      sdb_file_name_off = 0x15;
      sdb_file_ext_off = 0x1d;
      sdb_file_attr_off = 0x20;
      sdb_file_time_off = 0x2b;
      sdb_file_date_off = 0x2d;
      sdb_file_st_cluster_off = 0x2f;
      sdb_file_size_off = 0x31;

      sft_handle_cnt_off = 0x0;
      sft_open_mode_off = 0x2;
      sft_attribute_byte_off = 0x4;
      sft_device_info_off = 0x5;
      sft_dev_drive_ptr_off = 0x7;
      sft_fd_off = 0xb;
      sft_start_cluster_off = 0xb;
      sft_time_off = 0xd;
      sft_date_off = 0xf;
      sft_size_off = 0x11;
      sft_position_off = 0x15;
      sft_rel_cluster_off = 0x19;
      sft_abs_cluster_off = 0x1b;
      sft_directory_sector_off = 0x1d;
      sft_directory_entry_off = 0x1f;
      sft_name_off = 0x20;
      sft_ext_off = 0x28;

      cds_record_size = 0x51;
      cds_current_path_off = 0x0;
      cds_flags_off = 0x43;
      cds_rootlen_off = 0x4f;

      sda_current_dta_off = 0xc;
      sda_cur_psp_off = 0x10;
      sda_filename1_off = 0x92;
      sda_filename2_off = 0x112;
      sda_sdb_off = 0x192;
      sda_cds_off = 0x26c;
      sda_search_attribute_off = 0x23a;
      sda_open_mode_off = 0x23b;
      sda_rename_source_off = 0x2b8;
      sda_user_stack_off = 0x250;

      lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;
      break;
    }
  case DOSVER_50:
  case DOSVER_41:
    {
      sdb_drive_letter_off = 0x0;
      sdb_template_name_off = 0x1;
      sdb_template_ext_off = 0x9;
      sdb_attribute_off = 0xc;
      sdb_dir_entry_off = 0xd;
      sdb_p_cluster_off = 0xf;
      sdb_file_name_off = 0x15;
      sdb_file_ext_off = 0x1d;
      sdb_file_attr_off = 0x20;
      sdb_file_time_off = 0x2b;
      sdb_file_date_off = 0x2d;
      sdb_file_st_cluster_off = 0x2f;
      sdb_file_size_off = 0x31;

      /* same */ sft_handle_cnt_off = 0x0;
      sft_open_mode_off = 0x2;
      sft_attribute_byte_off = 0x4;
      sft_device_info_off = 0x5;
      sft_dev_drive_ptr_off = 0x7;
      sft_fd_off = 0xb;
      sft_start_cluster_off = 0xb;
      sft_time_off = 0xd;
      sft_date_off = 0xf;
      sft_size_off = 0x11;
      sft_position_off = 0x15;
      sft_rel_cluster_off = 0x19;
      sft_abs_cluster_off = 0x1b;
      sft_directory_sector_off = 0x1d;
      sft_directory_entry_off = 0x1f;
      sft_name_off = 0x20;
      sft_ext_off = 0x28;

      /* done */ cds_record_size = 0x58;
      cds_current_path_off = 0x0;
      cds_flags_off = 0x43;
      cds_rootlen_off = 0x4f;

      /* done */ sda_current_dta_off = 0xc;
      sda_cur_psp_off = 0x10;
      sda_filename1_off = 0x9e;
      sda_filename2_off = 0x11e;
      sda_sdb_off = 0x19e;
      sda_cds_off = 0x282;
      sda_search_attribute_off = 0x24d;
      sda_open_mode_off = 0x24e;
      sda_ext_act_off = 0x2dd;
      sda_ext_attr_off = 0x2df;
      sda_ext_mode_off = 0x2e1;
      sda_rename_source_off = 0x300;
      sda_user_stack_off = 0x264;

      /* same */ lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;

      break;
    }
    /* at the moment dos 6 is the same as dos 5,
	 anyone care to fix these for dos 6 ?? */
  case DOSVER_60:
  default:
    {
      sdb_drive_letter_off = 0x0;
      sdb_template_name_off = 0x1;
      sdb_template_ext_off = 0x9;
      sdb_attribute_off = 0xc;
      sdb_dir_entry_off = 0xd;
      sdb_p_cluster_off = 0xf;
      sdb_file_name_off = 0x15;
      sdb_file_ext_off = 0x1d;
      sdb_file_attr_off = 0x20;
      sdb_file_time_off = 0x2b;
      sdb_file_date_off = 0x2d;
      sdb_file_st_cluster_off = 0x2f;
      sdb_file_size_off = 0x31;

      /* same */ sft_handle_cnt_off = 0x0;
      sft_open_mode_off = 0x2;
      sft_attribute_byte_off = 0x4;
      sft_device_info_off = 0x5;
      sft_dev_drive_ptr_off = 0x7;
      sft_fd_off = 0xb;
      sft_start_cluster_off = 0xb;
      sft_time_off = 0xd;
      sft_date_off = 0xf;
      sft_size_off = 0x11;
      sft_position_off = 0x15;
      sft_rel_cluster_off = 0x19;
      sft_abs_cluster_off = 0x1b;
      sft_directory_sector_off = 0x1d;
      sft_directory_entry_off = 0x1f;
      sft_name_off = 0x20;
      sft_ext_off = 0x28;

      /* done */ cds_record_size = 0x58;
      cds_current_path_off = 0x0;
      cds_flags_off = 0x43;
      cds_rootlen_off = 0x4f;

      /* done */ sda_current_dta_off = 0xc;
      sda_cur_psp_off = 0x10;
      sda_filename1_off = 0x9e;
      sda_filename2_off = 0x11e;
      sda_sdb_off = 0x19e;
      sda_cds_off = 0x282;
      sda_search_attribute_off = 0x24d;
      sda_open_mode_off = 0x24e;
      sda_ext_act_off = 0x2dd;
      sda_ext_attr_off = 0x2df;
      sda_ext_mode_off = 0x2e1;
      sda_rename_source_off = 0x300;
      sda_user_stack_off = 0x264;

      /* same */ lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;

      break;
    }
  }
}


struct direct *
dos_readdir(dir)
     DIR *dir;
{
  struct direct *ret;

  ret = (struct direct *)RPT_SYSCALL(readdir(dir));

  return (ret);
}

static inline int
dos_flush(fd)
     int fd;
{
  int ret;

  ret = RPT_SYSCALL(fsync(fd));

  return (ret);
}

static inline int
dos_read(fd, data, cnt)
     int fd;
     char *data;
     int cnt;
{
  int ret;

  if (cnt <= 0)
    return (0);
  ret = RPT_SYSCALL(read(fd, data, cnt));
  return (ret);
}

static inline int
dos_write(fd, data, cnt)
     int fd;
     char *data;
     int cnt;
{
  int ret;

  if (cnt <= 0)
    return (0);
  ret = RPT_SYSCALL(write(fd, data, cnt));
  Debug0((dbg_fd, "Wrote %10.10s\n", data));
  return (ret);
}

static int
calculate_drive_pointers(int dd)
{
  far_t cdsfarptr;
  char *cwd;
  cds_t cds_base;

  if (!lol)
    return (0);
  if (!dos_roots[dd])
    return (0);

  cdsfarptr = lol_cdsfarptr(lol);
  cds_base = (cds_t) Addr_8086(cdsfarptr.segment, cdsfarptr.offset);

  cds = drive_cds(dd);

  /* if it's already done then don't bother */
  if ((cds_flags(cds) & (CDS_FLAG_REMOTE | CDS_FLAG_READY)) ==
      (CDS_FLAG_REMOTE | CDS_FLAG_READY))
    return (1);

  Debug0((dbg_fd, "Calculated DOS Information for %d:\n", dd));
  Debug0((dbg_fd, "  cwd=%20s\n", cds_current_path(cds)));
  Debug0((dbg_fd, "  cds flags =%x\n", cds_flags(cds)));
  Debug0((dbg_fd, "  cdsfar = %x, %x\n", cdsfarptr.segment,
	  cdsfarptr.offset));

  cds_flags(cds) |= (CDS_FLAG_REMOTE | CDS_FLAG_READY | CDS_FLAG_NOTNET);

  cwd = cds_current_path(cds);
  sprintf(cwd, "%c:\\", 'A' + dd);
  cds_rootlen(cds) = strlen(cwd) - 1;
  Debug0((dbg_fd, "cds_current_path=%s\n", cwd));
  return (1);
}

static boolean_t
dos_fs_dev(state)
     state_t *state;
{
  u_char drive_to_redirect;
  int dos_ver;

  Debug0((dbg_fd, "emufs operation: 0x%08lx\n", state->ebx));

  if (WORD(state->ebx) == 0x500) {
    init_all_drives();
    mach_fs_enabled = TRUE;

    lol = (lol_t) Addr(state, es, edx);
    sda = (sda_t) Addr(state, ds, esi);
    dos_major = LOW(state->ecx);
    dos_minor = HIGH(state->ecx);
    Debug0((dbg_fd, "dos_fs: dos_major:minor = 0x%d:%d.\n",
	    dos_major, dos_minor));
    Debug0((dbg_fd, "lol=%p\n", (void *) lol));
    Debug0((dbg_fd, "sda=%p\n", (void *) sda));
    if ((dos_major == 3) && (dos_minor > 9) && (dos_minor <= 31)) {
      dos_ver = DOSVER_31_33;
    }
    else if ((dos_major == 4) && (dos_minor >= 0) && (dos_minor <= 1)) {
      dos_ver = DOSVER_41;
    }
    else if ((dos_major == 5) && (dos_minor >= 0)) {
      dos_ver = DOSVER_50;
    }
    else if ((dos_major >= 6) && (dos_minor >= 0)) {
      dos_ver = DOSVER_60;
    }
    else {
      dos_ver = 0;
    }
    init_dos_offsets(dos_ver);
    SETWORD(&(state->eax), 1);
  }

  if (WORD(state->ebx) == 0) {
    u_char *ptr;

    ptr = (u_char *) Addr_8086(state->es, state->edi) + 22;

    drive_to_redirect = *ptr;
    /* if we've never set our first free drive, set it now, and */
    /* initialize our drive tables */
    if (first_free_drive == 0) {
      Debug0((dbg_fd, "Initializing all drives\n"));
      first_free_drive = drive_to_redirect;
      init_all_drives();
    }

    if (drive_to_redirect - (int) first_free_drive < 0) {
      SETWORD(&(state->eax), 0);
      Debug0((dbg_fd, "Invalid drive - maybe increase LASTDRIVE= in config.sys?\n"));
      return (UNCHANGED);
    }

    *(ptr - 9) = 1;
    Debug0((dbg_fd, "first_free_drive = %d\n", first_free_drive));
    {
      u_short *seg = (u_short *) (ptr - 2);
      u_short *ofs = (u_short *) (ptr - 4);
      char *clineptr = (char *) Addr_8086(*seg, *ofs);
      char *dirnameptr = (char *) Addr_8086(state->ds, state->esi);
      char cline[256];
      char *t;
      int i = 0;

      while (*clineptr != '\n' && *clineptr != '\r')
	cline[i++] = *(clineptr++);
      cline[i] = 0;

      t = strtok(cline, " \n\r\t");
      if (t) {
	t = strtok(NULL, " \n\r\t");
      }

      if (!init_drive(drive_to_redirect, t,
		      t ? strtok(NULL, " \n\r\t") : NULL)) {
	SETWORD(&(state->eax), 0);
	return (UNCHANGED);
      }

      if (strncmp(dirnameptr - 10, "directory ", 10) == 0) {
	*dirnameptr = 0;
	strncpy(dirnameptr, dos_roots[drive_to_redirect], 128);
	strcat(dirnameptr, "\n\r$");
      }
      else
	Debug0((dbg_fd, "WARNING! old version of emufs.sys!\n"));
    }

    mach_fs_enabled = TRUE;

    /*
     * So that machfs.sys v1.1+ will know that
     * we're running Mach too.
     */
    SETWORD(&(state->eax), 1);

    return (UNCHANGED);
  }

  return (UNCHANGED);
}

__inline__ void
time_to_dos(clock, date, time)
     time_t *clock;
     u_short *date;
     u_short *time;
{
  struct tm *tm;

  tm = localtime(clock);

  *date = ((((tm->tm_year - 80) & 0x1f) << 9) |
	   (((tm->tm_mon + 1) & 0xf) << 5) |
	   (tm->tm_mday & 0x1f));

  *time = (((tm->tm_hour & 0x1f) << 11) |
	   ((tm->tm_min & 0x3f) << 5) |
	   ((tm->tm_sec>>1) & 0x1f));
}

__inline__ time_t 
time_to_unix(u_short dos_date, u_short dos_time) 
{
   struct tm T;
   T.tm_sec  = (dos_time & 0x1f) << 1;    dos_time >>= 5;
   T.tm_min  = dos_time & 0x3f;           dos_time >>= 6;
   T.tm_hour = dos_time;
   T.tm_mday = dos_date & 0x1f;           dos_date >>= 5;
   T.tm_mon  = (dos_date & 0x0f) - 1;     dos_date >>= 4;
   T.tm_year = dos_date + 80;
   T.tm_isdst = -1;

   /* tm_wday, tm_yday are ignored by mktime() */

   /* OOPS! it's not true for tm_isdst! (at least with libc 4.6.27) -- peak */
   /* autodetect dst */
   T.tm_isdst = -1;

   return mktime(&T);
}

__inline__ int
strip_char(ptr, ch)
     char *ptr;
     char ch;
{
  int len = 0;
  char *wptr;
  char *rptr;

  wptr = ptr;
  rptr = ptr;

  while (*rptr != EOS) {
    if (*rptr == ch) {
      rptr++;
    }
    else {
      if (wptr != rptr)
	*wptr = *rptr;
      wptr++;
      rptr++;
      len++;
    }
  }
  *wptr = EOS;

  return (len);
}

__inline__ static void
path_to_ufs(char *ufs, char *path, int PreserveEnvVar)
{
  char ch;
  int len = 0, inenv = 0;
  char *wptr = ufs;
  char *rptr = path;

  while ((ch = *rptr++) != EOS) {
    switch (ch) {
    case ' ':
      rptr++;
      continue;
    case BACKSLASH:
      if (PreserveEnvVar) {	/* Check for environment variable */
	if ((rptr[0] == '$') && (rptr[1] == '{'))
	  inenv = 1;
	else
	  ch = SLASH;
      }
      else
	ch = SLASH;
      *wptr = ch;
      break;
    case '}':
      inenv = 0;
    default:
      if (!inenv)
	*wptr = tolower(ch);
      else
	*wptr = ch;

      break;
    }
    wptr++;
    len++;
  }
  *wptr = EOS;

  if (ufs[len] == '.')
    ufs[len] = EOS;

  Debug0((dbg_fd, "dos_gen: path_to_ufs '%s'\n", ufs));
}

void
build_ufs_path(ufs, path)
     char *ufs;
     char *path;
{
  char *p, *s;

  Debug0((dbg_fd, "dos_fs: build_ufs_path for DOS path '%s'\n", path));

 if ((s=is_reserved_msdos(path))) {
  Debug0((dbg_fd,"MFS: Special File, so name changed to '%s'\n", s));
  strcpy (ufs, s);
 }
 else {
  /* Set ufs to dos_roots[current_drive] */
  strcpy(ufs, dos_root);
  /* Skip over leading <drive>:\ in the path */
  if (path[1]==':')
      path += cds_rootlen(cds);

  /* remove spaces */
  s = path; p = NULL;
  while (*s && (*s==' ')) *s++ = '_';
  while (*s && (*s!='.')) {
  	if ((*s==' ') && (p==NULL)) p=s;
  	s++;
  }
  if (p) {	/* space found */
  	if (*s=='.') { while (*s && (*s!=' ')) *p++=*s++; }
  	*p = 0;
  }
  Debug0((dbg_fd,"MFS: dos_gen: ufs '%s', path '%s', l=%d\n", ufs, path, dos_root_len));

  path_to_ufs(ufs + dos_root_len, path, 0);

  /* remove any double slashes */
  path = ufs;
  while (*path) {
    if (*path == '/' && *(path + 1) == '/')
      strcpy(path, path + 1);
    else
      path++;
  }
 }
  Debug0((dbg_fd, "dos_fs: build_ufs_path result is '%s'\n", ufs));
}

/*
 * scan a directory for a matching filename
 */
static boolean_t
scan_dir(char *path, char *name)
{
  DIR *cur_dir;
  struct direct *cur_ent;

  /* handle null paths */
  if (*path == 0)
    path = "/";

  Debug0((dbg_fd,"scan_dir(%s,%s)\n",path,name));

  /* open the directory */
  if ((cur_dir = opendir(path)) == NULL) {
    Debug0((dbg_fd, "scan_dir(): failed to open dir: %s\n", path));
    return (FALSE);
  }

  /* now scan for matching names */
  while ((cur_ent = dos_readdir(cur_dir))) {
    char tmpname[100];

    if (cur_ent->d_ino == 0)
      continue;

    if (!name_convert(tmpname,cur_ent->d_name,MANGLE,NULL))
      continue;

    if (tmpname[0] == '.' &&
	strncasecmp(path, dos_root, strlen(path)) != 0)
      continue;

    if (strcasecmp(name, tmpname) != 0)
      continue;

    Debug0((dbg_fd, "scan_dir found %s\n",cur_ent->d_name));

    /* we've found the file, change it's name and return */
    strcpy(name, cur_ent->d_name);
    closedir(cur_dir);
    return (TRUE);
  }

  closedir(cur_dir);

  if (MANGLE && is_mangled(name))
    check_mangled_stack(name,NULL);

  Debug0((dbg_fd, "scan_dir gave %s FALSE\n",name));

  return (FALSE);
}

/*
 * a new find_file that will do complete upper/lower case matching for the
 * whole path
 */
static boolean_t
_find_file(char *fpath, struct stat * st)
{
  char *slash1, *slash2;

  Debug0((dbg_fd, "find file %s\n", fpath));

  /* first see if the path exists as is */
  if (stat(fpath, st) == 0)
    return (TRUE);

  /* if it isn't an absolute path then we're in trouble */
  if (*fpath != '/') {
    error("MFS: non-absolute path in find_file: %s\n", fpath);
    return (FALSE);
  }

  /* slash1 will point to the beginning of the section we're looking
     at, and slash2 will point at the end */
  slash1 = fpath + dos_root_len - 1;

  /* maybe if we make it all lower case, this is a "best guess" */
  for (slash2 = slash1; *slash2; ++slash2)
    *slash2 = tolower(*slash2);
  if (stat(fpath, st) == 0)
    return (TRUE);

  if (!strncmp((u_char *) (fpath + strlen(fpath) - 3), "nul", 3)) {
    Debug0((dbg_fd, "nul cmpr\n"));
    stat("/dev/null", st);
    return (TRUE);
  }

  /* now match each part of the path name separately, trying the names
     as is first, then tring to scan the directory for matching names */
  while (slash1) {
    slash2 = strchr(slash1 + 1, '/');
    if (slash2)
      *slash2 = 0;
    if (stat(fpath, st) == 0) {
      /* the file exists as is */
      if (st->st_mode & S_IFDIR || !slash2) {
	if (slash2)
	  *slash2 = '/';
	slash1 = slash2;
	continue;
      }

      Debug0((dbg_fd, "find_file(): not a directory: %s\n", fpath));
      if (slash2)
	*slash2 = '/';
      return (FALSE);
    }
    else {
      char remainder[1024];
      *slash1 = 0;
      if (slash2) {
	remainder[0] = '/';
	strcpy(remainder+1,slash2+1);
      }
      if (!scan_dir(fpath, slash1 + 1)) {
	*slash1 = '/';
	Debug0((dbg_fd, "find_file(): no match: %s\n", fpath));
	if (slash2)
	  strcat(slash1+1,remainder);
	return (FALSE);
      }
      else {
        *slash1 = '/';
        if (slash2)
	  strcat(slash1+1,remainder);
        slash1 = strchr(slash1+1,'/');
      }
    }
  }

  /* we've found the file - now stat it */
  if (stat(fpath, st) != 0) {
    Debug0((dbg_fd, "find_file(): can't stat %s\n", fpath));
    return (FALSE);
  }

  Debug0((dbg_fd, "found file %s\n", fpath));
  return (TRUE);
}

static boolean_t
find_file(fpath, st)
     char *fpath;
     struct stat *st;
{
  boolean_t r;

  Debug0((dbg_fd, "find_file(%s)\n",fpath));

  PS(FINDFILE);
  r = _find_file(fpath, st);
  PE(FINDFILE);

  Debug0((dbg_fd, "find_file gave %s %d\n",fpath,(int)r));

  return (r);
}

static boolean_t
compare(fname, fext, mname, mext)
     char *fname;
     char *fext;
     char *mname;
     char *mext;
{
  int i;

  Debug0((dbg_fd, "dos_gen: compare '%.8s'.'%.3s' to '%.8s'.'%.3s'\n",
	  mname, mext, fname, fext));
  /* match name first */
  for (i = 0; i < 8; i++) {
    if (mname[i] == '?') {
      continue;
    }
    if (mname[i] == ' ') {
      if (fname[i] == ' ') {
	break;
      }
      else {
	return (FALSE);
      }
    }
    if (mname[i] == '*') {
      break;
    }
    if (isalpha(mname[i]) && isalpha(fname[i])) {
      char x = isupper(mname[i]) ?
      mname[i] : toupper(mname[i]);
      char y = isupper(fname[i]) ?
      fname[i] : toupper(fname[i]);

      if (x != y)
	return (FALSE);
    }
    else if (mname[i] != fname[i]) {
      return (FALSE);
    }
  }
  /* if got here then name matches */
  /* match ext next */
  for (i = 0; i < 3; i++) {
    if (mext[i] == '?') {
      continue;
    }
    if (mext[i] == ' ') {
      if (fext[i] == ' ') {
	break;
      }
      else {
	return (FALSE);
      }
    }
    if (mext[i] == '*') {
      break;
    }
    if (isalpha(mext[i]) && isalpha(fext[i])) {
      char x = isupper(mext[i]) ?
      mext[i] : toupper(mext[i]);
      char y = isupper(fext[i]) ?
      fext[i] : toupper(fext[i]);

      if (x != y)
	return (FALSE);
    }
    else if (mext[i] != fext[i]) {
      return (FALSE);
    }
  }
  return (TRUE);
}

static struct dir_ent *
_match_filename_prune_list(list, name, ext)
     struct dir_ent *list;
     char *name;
     char *ext;
{
  struct dir_ent *last_ptr;
  struct dir_ent *tmp_ptr;
  struct dir_ent *first_ptr;

  /* special case checks */
  if ((strncmp(name, "????????", 8) == 0) &&
      (strncmp(ext, "???", 3) == 0))
    return (list);

  /* more special case checks */
  if ((strncmp(name, "????????", 8) == 0) &&
      (strncmp(ext, "   ", 3) == 0))
    return (list);

  first_ptr = NULL;
  last_ptr = NULL;

  while (list != NULL) {
    if (compare(list->name, list->ext, name, ext)) {
      if (first_ptr == NULL) {
	first_ptr = list;
      }
      last_ptr = list;
      tmp_ptr = list->next;
      list = tmp_ptr;
    }
    else {
      last_ptr->next = list->next;
      tmp_ptr = list->next;
      free(list);
      list = tmp_ptr;
    }
  }
  return (first_ptr);
}

static __inline__ struct dir_ent *
match_filename_prune_list(list, name, ext)
     struct dir_ent *list;
     char *name;
     char *ext;
{
  struct dir_ent *r;

  PS(MATCH);
  r = _match_filename_prune_list(list, name, ext);
  PE(MATCH);
  return (r);
}

#if 0
#define HLIST_STACK_SIZE 32
#else
#define HLIST_STACK_SIZE 256
#endif
static int hlist_stack_indx = 0;
static struct dir_ent *hlist = NULL;
static int hlist_index = 0; 
static struct dir_ent *hlist_stack[HLIST_STACK_SIZE];
static unsigned hlist_psp_stack[HLIST_STACK_SIZE];

static __inline__ int
hlist_push(hlist, psp)
     struct dir_ent *hlist;
     unsigned psp;
{
  Debug0((dbg_fd, "hlist_push: %d hlist=%p PSP=%d\n", hlist_stack_indx, hlist, psp));
  if (hlist_stack_indx >= HLIST_STACK_SIZE) {
    Debug0((dbg_fd, "hlist_push: past maximum stack"));
    return -1;
  }
  else {
    hlist_stack[hlist_stack_indx] = hlist;
    hlist_psp_stack[hlist_stack_indx] = psp;
    return hlist_stack_indx++;
  }
}

static __inline__ void
hlist_pop(indx, psp)
     int indx;
     unsigned psp;
{
  struct dir_ent *hl;
  int ind;
  Debug0((dbg_fd, "hlist_pop: %d popping=%d PSP=%d\n", hlist_stack_indx, indx, psp));
  if (hlist_stack_indx <= indx)
    return;
  if (hlist_psp_stack[indx] != psp) {
    Debug0((dbg_fd, "hlist_pop: psp mismatch\n"));
    return;
  }
  hl = hlist_stack[indx];
  if (hl != NULL) {
    Debug0((dbg_fd, "hlist_pop: popped list not empty?!\n"));
    free_list(hl);
  }
  hlist_stack[indx] = NULL;
  hlist_psp_stack[indx] = 0;
  for (ind = hlist_stack_indx-1; ind >= 0 && hlist_stack[ind] == NULL; --ind) ;
  hlist_stack_indx = ind + 1;
}

static __inline__ void
hlist_pop_psp(psp)
     unsigned psp;
{
  struct dir_ent *hl;
  Debug0((dbg_fd, "hlist_pop_psp: PSP=%d\n", psp));
  while (hlist_stack_indx > 0) {
    if (hlist_psp_stack[--hlist_stack_indx] != psp) {
      ++hlist_stack_indx;
      return;
    }
    hl = hlist_stack[hlist_stack_indx];
    Debug0((dbg_fd, "hlist_pop_psp: deleting hlist=%p\n", hl));
    free_list(hl);
  }
}

#if 0
static void
debug_dump_sft(handle)
     char handle;
{
  u_short *ptr;
  u_char *sptr;
  int sftn;

  ptr = (u_short *) (FARPTR((far_t *) (lol + 0x4)));

  Debug0((dbg_fd, "SFT: han = 0x%x, sftptr = %p\n",
	  handle, (void *) ptr));

  /* Assume 3.1 or 3.3 Dos */
  sftn = handle;
  while (TRUE) {
    if ((*ptr == 0xffff) && (ptr[2] < sftn)) {
      Debug0((dbg_fd, "handle invalid.\n"));
      break;
    }
    if (ptr[2] > sftn) {
      sptr = (u_char *) & ptr[3];
      while (sftn--)
	sptr += 0x35;		/* dos 3.1 3.3 */
      Debug0((dbg_fd, "handle_count = %x\n",
	      sft_handle_cnt(sptr)));
      Debug0((dbg_fd, "open_mode = %x\n",
	      sft_open_mode(sptr)));
      Debug0((dbg_fd, "attribute byte = %x\n",
	      sft_attribute_byte(sptr)));
      Debug0((dbg_fd, "device_info = %x\n",
	      sft_device_info(sptr)));
      Debug0((dbg_fd, "dev_drive_ptr = %lx\n",
	      sft_dev_drive_ptr(sptr)));
      Debug0((dbg_fd, "starting cluster = %x\n",
	      sft_start_cluster(sptr)));
      Debug0((dbg_fd, "file time = %x\n",
	      sft_time(sptr)));
      Debug0((dbg_fd, "file date = %x\n",
	      sft_date(sptr)));
      Debug0((dbg_fd, "file size = %lx\n",
	      sft_size(sptr)));
      Debug0((dbg_fd, "pos = %lx\n",
	      sft_position(sptr)));
      Debug0((dbg_fd, "rel cluster = %x\n",
	      sft_rel_cluster(sptr)));
      Debug0((dbg_fd, "abs cluster = %x\n",
	      sft_abs_cluster(sptr)));
      Debug0((dbg_fd, "dir sector = %x\n",
	      sft_directory_sector(sptr)));
      Debug0((dbg_fd, "dir entry = %x\n",
	      sft_directory_entry(sptr)));
      Debug0((dbg_fd, "name = %.8s\n",
	      sft_name(sptr)));
      Debug0((dbg_fd, "ext = %.3s\n",
	      sft_ext(sptr)));
      return;
    }
    sftn -= ptr[2];
    ptr = (u_short *) Addr_8086(ptr[1], ptr[0]);
  }
}
#endif

/* convert forward slashes to back slashes for DOS */

static void
path_to_dos(char *path)
{
  char *s;

  for (s = path; (s = strchr(s, '/')) != NULL; ++s)
    *s = '\\';
}

static int
GetRedirection(state, index)
     state_t *state;
     u_short index;
{
  int dd;
  u_short returnBX;		/* see notes below */
  u_short returnCX;
  char *resourceName;
  char *deviceName;
  u_short *userStack;

  /* Set number of redirected drives to 0 prior to getting new
	   Count */
  /* BH has device status 0=valid */
  /* BL has the device type - 3 for printer, 4 for disk */
  /* CX is supposed to be used to return the stored redirection parameter */
  /* I'm going to cheat and return a read-only flag in it */
  Debug0((dbg_fd, "GetRedirection, index=%d\n", index));
  for (dd = 0; dd < num_drives; dd++) {
    if (dos_roots[dd]) {
      if (index == 0) {
	/* return information for this drive */
	Debug0((dbg_fd, "redirection root =%s\n", dos_roots[dd]));
	deviceName = (u_char *) Addr(state, ds, esi);
	deviceName[0] = 'A' + dd;
	deviceName[1] = ':';
	deviceName[2] = EOS;
	resourceName = (u_char *) Addr(state, es, edi);
	strcpy(resourceName, LINUX_RESOURCE);
	strcat(resourceName, dos_roots[dd]);
	path_to_dos(resourceName);
	Debug0((dbg_fd, "resource name =%s\n", resourceName));
	Debug0((dbg_fd, "device name =%s\n", deviceName));
	userStack = (u_short *) sda_user_stack(sda);

	/* have to return BX, and CX on the user return stack */
	/* return a "valid" disk redirection */
	returnBX = 4;		/*BH=0, BL=4 */

	/* set the high bit of the return CL so that */
	/* NetWare shell doesn't get confused */
	returnCX = read_onlys[dd] | 0x80;

	Debug0((dbg_fd, "GetRedirection "
		"user stack=%p, CX=%x\n",
		(void *) userStack, returnCX));
	userStack[1] = returnBX;
	userStack[2] = returnCX;
	/* XXXTRB - should set session number in returnBP if */
	/* we are doing an extended getredirection */
	return (TRUE);
      }
      else {
	/* count down until the index is exhausted */
	index--;
      }
    }
  }
  if (IS_REDIRECTED(0x2f)) {
    redirected_drives = WORD(state->ebx) - index;
    SETWORD(&(state->ebx), index);
    Debug0((dbg_fd, "GetRedirect passing index of %d, Total redirected=%d\n", index, redirected_drives));
    return (REDIRECT);
  }

  SETWORD(&(state->eax), NO_MORE_FILES);
  return (FALSE);
}

/*****************************
 * RedirectDevice - redirect a drive to the Linux file system
 * on entry:
 *		cds_base should be set
 * on exit:
 * notes:
 *****************************/
static int
RedirectDevice(state_t * state)
{
  char *resourceName;
  char *deviceName;
  char path[256];

  /* first, see if this is our resource to be redirected */
  resourceName = (u_char *) Addr(state, es, edi);
  deviceName = (u_char *) Addr(state, ds, esi);
  path[0] = 0;

  Debug0((dbg_fd, "RedirectDevice %s to %s\n", deviceName, resourceName));
  if (strncmp(resourceName, LINUX_RESOURCE,
	      strlen(LINUX_RESOURCE)) != 0) {
    /* pass call on to next redirector, if any */
    return (REDIRECT);
  }
  /* see what device is to be redirected */
  /* we only support disk redirection right now */
  if (LOW(state->ebx) != 4 || deviceName[1] != ':') {
    SETWORD(&(state->eax), FUNC_NUM_IVALID);
    return (FALSE);
  }
  current_drive = toupper(deviceName[0]) - 'A';

  /* see if drive is in range of valid drives */
  if (current_drive < 0 || current_drive > lol_last_drive(lol)) {
    SETWORD(&(state->eax), DISK_DRIVE_INVALID);
    return (FALSE);
  }
  cds = drive_cds(current_drive);
  /* see if drive is already redirected */
  if (cds_flags(cds) & CDS_FLAG_REMOTE) {
    SETWORD(&(state->eax), DUPLICATE_REDIR);
    return (FALSE);
  }
  /* see if drive is currently substituted */
  if (cds_flags(cds) & CDS_FLAG_SUBST) {
    SETWORD(&(state->eax), DUPLICATE_REDIR);
    return (FALSE);
  }

  path_to_ufs(path, &resourceName[strlen(LINUX_RESOURCE)], 1);

  /* if low bit of CX is set, then set for read only access */
  Debug0((dbg_fd, "read-only attribute = %d\n",
	  (int) (state->ecx & 1)));
  if (init_drive(current_drive, path, (state->ecx & 1) ? "R" : NULL) == 0) {
    SETWORD(&(state->eax), NETWORK_NAME_NOT_FOUND);
    return (FALSE);
  }
  else {
    return (TRUE);
  }
}

/*****************************
 * CancelRedirection - cancel a drive redirection
 * on entry:
 *		cds_base should be set
 * on exit:
 * notes:
 *****************************/
static int
CancelRedirection(state_t * state)
{
  char *deviceName;
  char *path;
  far_t DBPptr;

  /* first, see if this is one of our current redirections */
  deviceName = (u_char *) Addr(state, ds, esi);

  Debug0((dbg_fd, "CancelRedirection on %s\n", deviceName));
  if (deviceName[1] != ':') {
    /* we only handle drive redirections, pass it through */
    return (REDIRECT);
  }
  current_drive = toupper(deviceName[0]) - 'A';

  /* see if drive is in range of valid drives */
  if (current_drive < 0 || current_drive > lol_last_drive(lol)) {
    SETWORD(&(state->eax), DISK_DRIVE_INVALID);
    return (FALSE);
  }
  cds = drive_cds(current_drive);
  if (dos_roots[current_drive] == NULL) {
    /* we don't own this drive, pass it through to next redirector */
    return (REDIRECT);
  }

  /* first, clean up my information */
  free(dos_roots[current_drive]);
  dos_roots[current_drive] = NULL;
  dos_root_lens[current_drive] = 0;
  finds_in_progress[current_drive] = FALSE;
  read_onlys[current_drive] = FALSE;

  /* reset information in the CDS for this drive */
  cds_flags(cds) = 0;		/* default to a "not ready" drive */

  path = cds_current_path(cds);
  /* set the current path for the drive */
  path[0] = current_drive + 'A';
  path[1] = ':';
  path[2] = '\\';
  path[3] = EOS;
  cds_rootlen(cds) = CDS_DEFAULT_ROOT_LEN;
  cds_cur_cluster(cds) = 0;	/* reset us at the root of the drive */

  /* see if there is a physical drive behind this redirection */
  DBPptr = cds_DBP_pointer(cds);
  if (DBPptr.offset | DBPptr.segment) {
    /* if DBP_pointer is non-NULL, set the drive status to ready */
    cds_flags(cds) = CDS_FLAG_READY;
  }

  Debug0((dbg_fd, "CancelRedirection on %s completed\n", deviceName));
  return (TRUE);
}

static int
dos_fs_redirect(state)
     state_t *state;
{
  char *filename1;
  char *filename2;
  char *dta;
  u_char firstfind = 0;
  long s_pos=0;
  u_char attr;
  u_char subfunc;
  u_short dos_mode, unix_mode;
  u_short FCBcall = 0;
  u_char create_file=0;
  int fd;
  int cnt;
  int ret = REDIRECT;
  cds_t my_cds;
  sft_t sft;
  sdb_t sdb;
  int i;
  int bs_pos;
  char fname[8];
  char fext[3];
  char fpath[1024];
  char buf[1024];
  struct dir_ent *tmp;
  struct stat st;
#if 0
  static char last_find_name[8] = "";
  static char last_find_ext[3] = "";
  static u_char last_find_dir = 0;
  static u_char last_find_drive = 0;
  char *resourceName;
  char *deviceName;
#endif
 dos_mode = 0;

  if (!mach_fs_enabled)
    return (REDIRECT);

  my_cds = sda_cds(sda);

  sft = (u_char *) Addr(state, es, edi);

  Debug0((dbg_fd, "Entering dos_fs_redirect, FN=%02X\n",(int)LOW(state->eax)));

  if (!select_drive(state))
    return (REDIRECT);

  filename1 = sda_filename1(sda);
  filename2 = sda_filename2(sda);
  sdb = sda_sdb(sda);
  dta = sda_current_dta(sda);

#if 0
  Debug0((dbg_fd, "CDS current path: %s\n", cds_current_path(cds)));
  Debug0((dbg_fd, "Filename1 %s\n", filename1));
  Debug0((dbg_fd, "Filename2 %s\n", filename2));
  Debug0((dbg_fd, "sft %x\n", sft));
  Debug0((dbg_fd, "dta %x\n", dta));
  fflush(NULL);
#endif

  switch (LOW(state->eax)) {
  case INSTALLATION_CHECK:	/* 0x00 */
    Debug0((dbg_fd, "Installation check\n"));
    SETLOW(&(state->eax), 0xFF);
    return (TRUE);
  case REMOVE_DIRECTORY:	/* 0x01 */
  case REMOVE_DIRECTORY_2:	/* 0x02 */
    Debug0((dbg_fd, "Remove Directory %s\n", filename1));
    if (read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }

    build_ufs_path(fpath, filename1);
    if (find_file(fpath, &st)) {
      if (rmdir(fpath) != 0) {
	Debug0((dbg_fd, "failed to remove directory %s\n", fpath));
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
    }
    else {
      Debug0((dbg_fd, "couldn't find directory %s\n", fpath));
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      return (FALSE);
    }
    return (TRUE);
  case MAKE_DIRECTORY:		/* 0x03 */
  case MAKE_DIRECTORY_2:	/* 0x04 */
    Debug0((dbg_fd, "Make Directory %s\n", filename1));
    if (read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    build_ufs_path(fpath, filename1);
    if (find_file(fpath, &st)) {
      Debug0((dbg_fd, "make failed already dir or file '%s'\n",
	      fpath));
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    if (mkdir(fpath, 0755) != 0) {
      for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
	if (fpath[i] == SLASH)
	  bs_pos = i;
      }
      strncpy(buf, fpath, bs_pos);
      buf[bs_pos] = EOS;
      find_file(buf, &st);
      strncpy(fpath, buf, bs_pos);
      Debug0((dbg_fd, "trying '%s'\n", fpath));
      if (mkdir(fpath, 0755) != 0) {
	Debug0((dbg_fd, "make directory failed '%s'\n",
		fpath));
	SETWORD(&(state->eax), PATH_NOT_FOUND);
	return (FALSE);
      }
    }
    return (TRUE);
  case SET_CURRENT_DIRECTORY:	/* 0x05 */
    Debug0((dbg_fd, "set directory to: %s\n", filename1));
    build_ufs_path(fpath, filename1);
    Debug0((dbg_fd, "set directory to ufs path: %s\n", fpath));

    /* Try the given path */
    if (!find_file(fpath, &st)) {
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      return (FALSE);
    }
    if (!(st.st_mode & S_IFDIR)) {
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      Debug0((dbg_fd, "Set Directory %s not found\n", fpath));
      return (FALSE);
    }
    /* what we do now is update the cds_current_path, although it is
       probably superflous in most cases as dos seems to do it for us */
    {
      char *fp, *cwd;

      fp = fpath + strlen(dos_root);

      /* Skip over leading <drive>:\ */
      cwd = cds_current_path(cds) + cds_rootlen(cds) + 1;

      /* now copy the rest of the path, changing / to \ */
      do
	*cwd++ = (*fp == '/' ? '\\' : *fp);
      while (*fp++);
    }
    Debug0((dbg_fd, "New CWD is %s\n", cds_current_path(cds)));
    return (TRUE);
  case CLOSE_FILE:		/* 0x06 */
    fd = sft_fd(sft);
    filename1 = (u_char *)sft_dev_drive_ptr(sft);
    Debug0((dbg_fd, "Close file %x (%s)\n", fd, filename1));
    Debug0((dbg_fd, "Handle cnt %d\n",
	    sft_handle_cnt(sft)));
    sft_handle_cnt(sft)--;
    if (sft_handle_cnt(sft) > 0) {
      Debug0((dbg_fd, "Still more handles\n"));
      return (TRUE);
    }
    else if (close(fd) != 0) {
      Debug0((dbg_fd, "Close file fails\n"));
      free(filename1);
      return (FALSE);
    }
    else {
      Debug0((dbg_fd, "Close file succeeds\n"));

      /* if bit 14 in device_info is set, dos requests to set the file 
         date/time on closing. R.Brown states this incorrectly (inverted).
      */
         
      if (sft_device_info(sft) & 0x4000) {  
         struct utimbuf ut;
         u_short dos_date=sft_date(sft),dos_time=sft_time(sft);
         
         Debug0((dbg_fd,"close: setting file date=%04x time=%04x [<- dos format]\n",
                 dos_date,dos_time));
         ut.actime=ut.modtime=time_to_unix(dos_date,dos_time);

         if (filename1!=NULL && *filename1) 
            utime(filename1,&ut);
      }
      else {
         Debug0((dbg_fd,"close: not setting file date/time\n"));
      }
      /* free(filename1); Tim Josling says: prob not opened dont free stg */
      return (TRUE);
    }
  case READ_FILE:
    {				/* 0x08 */
      int return_val;
      int itisnow;

      cnt = WORD(state->ecx);
      fd = sft_fd(sft);
      Debug0((dbg_fd, "Read file fd=%x, dta=%p, cnt=%d\n",
	      fd, (void *) dta, cnt));
      Debug0((dbg_fd, "Read file pos = %ld\n",
	      sft_position(sft)));
      Debug0((dbg_fd, "Handle cnt %d\n",
	      sft_handle_cnt(sft)));
      itisnow = lseek(fd, sft_position(sft), L_SET);
      Debug0((dbg_fd, "Actual pos %d\n",
	      itisnow));
      ret = dos_read(fd, dta, cnt);

      Debug0((dbg_fd, "Read returned : %d\n",
	      ret));
      if (ret < 0) {
	Debug0((dbg_fd, "ERROR IS: %s\n", strerror(errno)));
	return (FALSE);
      }
      else if (ret < cnt) {
	SETWORD(&(state->ecx), ret);
	return_val = TRUE;
      }
      else {
	SETWORD(&(state->ecx), cnt);
	return_val = TRUE;
      }
      sft_position(sft) += ret;
      sft_abs_cluster(sft) = 0x174a;	/* XXX a test */
/*      Debug0((dbg_fd, "File data %02x %02x %02x\n",
	      dta[0], dta[1], dta[2])); */
      Debug0((dbg_fd, "Read file pos after = %ld\n",
	      sft_position(sft)));
      return (return_val);
    }
  case WRITE_FILE:		/* 0x09 */
    cnt = WORD(state->ecx);
    fd = sft_fd(sft);

    Debug0((dbg_fd, "Write file fd=%x count=%x sft_mode=%x\n", fd, cnt, sft_open_mode(sft)));
    if (read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }

    /* According to U DOS 2, any write with a (cnt)=CX=0 should truncate fd to
   sft_size , do to how ftruncate works, I'll only do an ftruncate
   if the file's size is greater than the current file position. */

    if (!cnt && sft_size(sft) > sft_position(sft)) {
      Debug0((dbg_fd, "Applying O_TRUNC at %x\n", (int)s_pos));
      if (ftruncate(fd, (off_t) sft_position(sft))) {
	Debug0((dbg_fd, "O_TRUNC failed\n"));
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
      sft_size(sft) = sft_position(sft);
    }

    if (us_debug_level > Debug_Level_0) {
      s_pos = lseek(fd, sft_position(sft), L_SET);
    }
    Debug0((dbg_fd, "Handle cnt %d\n",
	    sft_handle_cnt(sft)));
    Debug0((dbg_fd, "sft_size = %x, sft_pos = %x, dta = %p, cnt = %x\n", (int)sft_size(sft), (int)sft_position(sft), (void *) dta, (int)cnt));
    if (us_debug_level > Debug_Level_0) {
      ret = dos_write(fd, dta, cnt);
      if ((ret + s_pos) > sft_size(sft)) {
	sft_size(sft) = ret + s_pos;
      }
    }
    Debug0((dbg_fd, "write operation done,ret=%x\n", ret));
    if (us_debug_level > Debug_Level_0)
      if (ret < 0) {
	Debug0((dbg_fd, "Write Failed : %s\n", strerror(errno)));
	return (FALSE);
      }
    Debug0((dbg_fd, "sft_position=%lu, Sft_size=%lu\n",
	    sft_position(sft), sft_size(sft)));
    SETWORD(&(state->ecx), ret);
    sft_position(sft) += ret;
    sft_abs_cluster(sft) = 0x174a;	/* XXX a test */
    if (us_debug_level > Debug_Level_0)
      return (TRUE);
  case GET_DISK_SPACE:
    {				/* 0x0c */
#ifdef USE_DF_AND_AFS_STUFF
      unsigned int free, tot;

      Debug0((dbg_fd, "Get Disk Space\n"));
      build_ufs_path(fpath, cds_current_path(cds));

      if (find_file(fpath, &st)) {
	if (get_disk_space(fpath, &free, &tot)) {
	  /* return unit = 512-byte blocks @ 1 spc, std for floppy */
	  int spc = 1;
	  int bps = 512;

	  while ((spc<32) && ((tot > 65535) || (free > 65535))) {
	    /* we're on an HD here! */
	    if (bps==512) {
	      bps = 1024;	/* try 1024-byte sectors first */
	    }
	    else
	      spc *= 2;		/* try bigger clusters, up to 32 */
	    free /= 2;
	    tot  /= 2;
	  }
	  /* report no more than 32*1024*64K = 2G, even if some
	     DOS version 7 can see more */
	  if (tot>65535) tot=65535;
	  if (free>65535) free=65535;

	  /* Ralf Brown says: AH=media ID byte - can we let it at 0 here? */
	  SETWORD(&(state->eax), spc);
	  SETWORD(&(state->edx), free);
	  SETWORD(&(state->ecx), bps);
	  SETWORD(&(state->ebx), tot);
	  Debug0((dbg_fd, "free=%d, tot=%d, bps=%d, spc=%d\n",
		  free, tot, bps, spc));

	  return (TRUE);
	}
	else {
	  Debug0((dbg_fd, "no ret gds\n"));
	}
      }
#endif /* USE_DF_AND_AFS_STUFF */
      break;
    }
  case SET_FILE_ATTRIBUTES:	/* 0x0e */
    {
      u_short att = *(u_short *) Addr(state, ss, esp);

      Debug0((dbg_fd, "Set File Attributes %s 0%o\n", filename1, att));
      if (read_only) {
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }

      build_ufs_path(fpath, filename1);
      Debug0((dbg_fd, "Set attr: '%s' --> 0%o\n", fpath, att));
      if (!find_file(fpath, &st)) {
	SETWORD(&(state->eax), FILE_NOT_FOUND);
	return (FALSE);
      }
      if (chmod(fpath, get_unix_attr(st.st_mode, att)) != 0) {
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
    }
    return (TRUE);

    break;
  case GET_FILE_ATTRIBUTES:	/* 0x0f */
    Debug0((dbg_fd, "Get File Attributes %s\n", filename1));
    build_ufs_path(fpath, filename1);
    if (!find_file(fpath, &st)) {
      Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }

    SETWORD(&(state->eax), get_dos_attr(st.st_mode,is_hidden(fpath)));
    state->ebx = st.st_size >> 16;
    state->edi = MASK16(st.st_size);
    return (TRUE);
  case RENAME_FILE:		/* 0x11 */
    Debug0((dbg_fd, "Rename file fn1=%s fn2=%s\n", filename1, filename2));
    if (read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    build_ufs_path(fpath, filename2);
    for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
      if (fpath[i] == SLASH)
	bs_pos = i;
    }
    strncpy(buf, fpath, bs_pos);
    buf[bs_pos] = EOS;
    find_file(buf, &st);
    strncpy(fpath, buf, bs_pos);

    build_ufs_path(buf, filename1);
    if (!find_file(buf, &st)) {
      Debug0((dbg_fd, "Rename '%s' error.\n", fpath));
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      return (FALSE);
    }

    if (rename(buf, fpath) != 0) {
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      return (FALSE);
    }
    else {
      Debug0((dbg_fd, "Rename file %s to %s\n",
	      fpath, buf));
      return (TRUE);
    }
  case DELETE_FILE:		/* 0x13 */
    {
      struct dir_ent *de = NULL;

      Debug0((dbg_fd, "Delete file %s\n", filename1));
      if (read_only) {
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
      build_ufs_path(fpath, filename1);
      for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
	if (fpath[i] == SLASH)
	  bs_pos = i;
      }
      fpath[bs_pos] = EOS;
      auspr(fpath + bs_pos + 1, fname, fext);
      if (bs_pos == 0) {
#if 0 /* 95/01/01 */
	bs_pos = -1;
#endif
	strcpy(fpath, "/");
      }

      de = match_filename_prune_list(get_dir(fpath, fname, fext),
				     fname, fext);

      if (de == NULL) {
	build_ufs_path(fpath, filename1);
	if (!find_file(fpath, &st)) {
	  SETWORD(&(state->eax), FILE_NOT_FOUND);
	  return (FALSE);
	}
	if (unlink(fpath) != 0) {
	  Debug0((dbg_fd, "Deleted %s\n", fpath));
	  SETWORD(&(state->eax), FILE_NOT_FOUND);
	  return (FALSE);
	}
	return (TRUE);
      }

      bs_pos = strlen( fpath);
      while (de != NULL) {
	if ((de->mode & S_IFMT) == S_IFREG) {
	  strncpy(fpath + bs_pos + 1, de->name, 8);
	  fpath[bs_pos] = SLASH;
	  fpath[bs_pos + 9] = '.';
	  fpath[bs_pos + 13] = EOS;
	  strncpy(fpath + bs_pos + 10, de->ext, 3);
	  strip_char(fpath, ' ');
	  cnt = strlen(fpath);
	  if (fpath[cnt - 1] == '.')
	    fpath[cnt - 1] = EOS;
	  if (find_file(fpath, &st)) {
	    unlink(fpath);
	    Debug0((dbg_fd, "Deleted %s\n", fpath));
	  }
	}
	tmp = de->next;
	free(de);
	de = tmp;
      }
      return (TRUE);
    }

  case OPEN_EXISTING_FILE:	/* 0x16 */
	/* according to the appendix in undoc dos 2 the top word on the
       stack holds the open mode.  Other than the definition in the
       appendix, I can find nothing else which supports this
       statement. */

    /* get the high byte */
    dos_mode = *(u_char *) (Addr(state, ss, esp) + 1);
    dos_mode <<= 8;

    /* and the low one (isn't there a way to do this with one Addr ??) */
    dos_mode |= *(u_char *)Addr(state, ss, esp);

    /* check for a high bit set indicating an FCB call */
    FCBcall = sft_open_mode(sft) & 0x8000;

    Debug0((dbg_fd, "(mode = 0x%04x)\n", dos_mode));
    Debug0((dbg_fd, "(sft_open_mode = 0x%04x)\n", sft_open_mode(sft)));

    if (FCBcall) {
	sft_open_mode(sft) |= 0x00f0;
    }
    else {
	sft_open_mode(sft) = dos_mode & 0xF;
    }
    dos_mode &= 0xF;
	
	/*
	   This method is ALSO in undoc dos.  They have the command
	   defined differently in two different places.  The important
	   thing is that this doesn't work under dr-dos 6.0

    attr = *(u_short *) Addr(state, ss, esp) */


    Debug0((dbg_fd, "Open existing file %s\n", filename1));

  do_open_existing:
    if (read_only && dos_mode != READ_ACC) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    build_ufs_path(fpath, filename1);
    if (!find_file(fpath, &st)) {
      Debug0((dbg_fd, "open failed: '%s'\n", fpath));
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }

    if (st.st_mode & S_IFDIR) {
      Debug0((dbg_fd, "S_IFDIR: '%s'\n", fpath));
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }
    if (dos_mode == READ_ACC) {
      unix_mode = O_RDONLY;
    }
    else if (dos_mode == WRITE_ACC) {
      unix_mode = O_WRONLY;
    }
    else if (dos_mode == READ_WRITE_ACC) {
      unix_mode = O_RDWR;
    }
    else if (dos_mode == 0x40) { /* what's this mode ?? */
      unix_mode = O_RDWR;
    }
    else {
      Debug0((dbg_fd, "Illegal access_mode 0x%x\n", dos_mode));
      unix_mode = O_RDONLY;
    }
    if (read_only && unix_mode != O_RDONLY) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }

    /* Handle DOS requests for drive:\patch\NUL */
    if (!strncmp((u_char *) (fpath + strlen(fpath) - 3), "nul", 3)) {
      if ((fd = open("/dev/null", unix_mode)) < 0) {
	Debug0((dbg_fd, "access denied:'%s'\n", fpath));
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
    }
    else if ((fd = open(fpath, unix_mode )) < 0) {
      Debug0((dbg_fd, "access denied:'%s'\n", fpath));
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }

    for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
      if (fpath[i] == SLASH)
	bs_pos = i;
    }

    auspr(fpath + bs_pos + 1, sft_name(sft), sft_ext(sft));

#if 0
    if (FCBcall)
      sft_open_mode(sft) |= 0x00F0;
#endif

    /* store the name for FILE_CLOSE */
    sft_dev_drive_ptr(sft) = (u_long)strdup(fpath);
    sft_directory_entry(sft) = 0;
    sft_directory_sector(sft) = 0;
#ifdef NOTEST
    sft_attribute_byte(sft) = attr;
#else
    sft_attribute_byte(sft) = 0x20;
#endif /* NOTEST */
    sft_device_info(sft) = current_drive + (0x8040);
    time_to_dos(&st.st_mtime,
		&sft_date(sft), &sft_time(sft));
    sft_size(sft) = st.st_size;
    sft_position(sft) = 0;
    sft_fd(sft) = fd;
    Debug0((dbg_fd, "open succeeds: '%s' fd = 0x%x\n", fpath, fd));
    Debug0((dbg_fd, "Size : %ld\n", (long) st.st_size));

    /* If FCB open requested, we need to call int2f 0x120c */
    if (FCBcall) {
      unsigned char *ssp;
      unsigned long sp;
  
      ssp = (unsigned char *)(REG(ss)<<4);
      sp = (unsigned long) LWORD(esp);

      Debug0((dbg_fd, "FCB Open calling int2f 0x120c\n"));
      pushw(ssp, sp, vflags);
      pushw(ssp, sp, LWORD(cs));
      pushw(ssp, sp, LWORD(eip));
      LWORD(esp) -= 6;
      REG(cs) = (us) INTE7_SEG;
      REG(eip) = (us) INTE7_OFF;

      /* clear TF (trap flag, singlestep), IF (interrupt flag), and
       * NT (nested task) bits of EFLAGS
      REG(eflags) &= ~(VIF | TF | IF | NT);
       */

    }

    return (TRUE);
  case CREATE_TRUNCATE_NO_CDS:	/* 0x18 */
  case CREATE_TRUNCATE_FILE:	/* 0x17 */

    FCBcall = sft_open_mode(sft) & 0x8000;
    Debug0((dbg_fd, "FCBcall=0x%x\n", FCBcall));

    /* 01 in high byte = create new, 00 s just create truncate */
    create_file = *(u_char *) (Addr(state, ss, esp) + 1);

    attr = *(u_short *) Addr(state, ss, esp);
    Debug0((dbg_fd, "CHECK attr=0x%x, create=0x%x\n", attr, create_file));

    /* make it a byte - we thus ignore the new bit */
    attr &= 0xFF;
    if (attr & DIRECTORY) return(REDIRECT);

    Debug0((dbg_fd, "Create truncate file %s attr=%x\n", filename1, attr));

  do_create_truncate:
    if (read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    build_ufs_path(fpath, filename1);

    if (find_file(fpath, &st)) {
      Debug0((dbg_fd, "st.st_mode = 0x%02x, handles=%d\n", st.st_mode, sft_handle_cnt(sft)));
      if ( /* !(st.st_mode & S_IFREG) || */ create_file) {
	SETWORD(&(state->eax), FILE_ALREADY_EXISTS);
	Debug0((dbg_fd, "File exists '%s'\n", fpath));
	return (FALSE);
      }
    }

    bs_pos=0;
    for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
      if (fpath[i] == SLASH)
	bs_pos = i;
    }

    if ((fd = open(fpath, (O_RDWR | O_CREAT | O_TRUNC),
		   get_unix_attr(0664, attr))) < 0) {
      strncpy(buf, fpath, bs_pos);
      buf[bs_pos] = EOS;
      find_file(buf, &st);
      strncpy(fpath, buf, bs_pos);
      Debug0((dbg_fd, "trying '%s'\n", fpath));
      if ((fd = open(fpath, (O_RDWR | O_CREAT | O_TRUNC),
		     get_unix_attr(0664, attr))) < 0) {
	Debug0((dbg_fd, "can't open %s: %s (%d)\n",
		fpath, strerror(errno), errno));
#if 1
	SETWORD(&(state->eax), FILE_NOT_FOUND);
#else
	SETWORD(&(state->eax), ACCESS_DENIED);
#endif
	return (FALSE);
      }
    }

    auspr(fpath + bs_pos + 1, sft_name(sft), sft_ext(sft));
    sft_dev_drive_ptr(sft) = (u_long)strdup(fpath);

    /* This caused a bug with temporary files so they couldn't be read,
   they were made write-only */
#if 0
    sft_open_mode(sft) = 0x1;
#else
    if (FCBcall)
      sft_open_mode(sft) |= 0x00f0;
    else
  #if 0
      sft_open_mode(sft) = 0x3; /* write only */
  #else
      sft_open_mode(sft) = 0x2; /* read/write */
  #endif
#endif
    sft_directory_entry(sft) = 0;
    sft_directory_sector(sft) = 0;
    sft_attribute_byte(sft) = attr;
    sft_device_info(sft) = current_drive + (0x8040);
    time_to_dos(&st.st_mtime, &sft_date(sft),
		&sft_time(sft));

    /* file size starts at 0 bytes */
    sft_size(sft) = 0;
    sft_position(sft) = 0;
    sft_fd(sft) = fd;
    Debug0((dbg_fd, "create succeeds: '%s' fd = 0x%x\n", fpath, fd));
    Debug0((dbg_fd, "size = 0x%lx\n", sft_size(sft)));

    /* If FCB open requested, we need to call int2f 0x120c */
    if (FCBcall) {
      unsigned char *ssp;
      unsigned long sp;
  
      ssp = (unsigned char *)(REG(ss)<<4);
      sp = (unsigned long) LWORD(esp);

      Debug0((dbg_fd, "FCB Open calling int2f 0x120c\n"));
      pushw(ssp, sp, vflags);
      pushw(ssp, sp, LWORD(cs));
      pushw(ssp, sp, LWORD(eip));
      LWORD(esp) -= 6;
      REG(cs) = (us) INTE7_SEG;
      REG(eip) = (us) INTE7_OFF;

      /* clear TF (trap flag, singlestep), IF (interrupt flag), and
       * NT (nested task) bits of EFLAGS
      REG(eflags) &= ~(VIF | TF | IF | NT);
       */

    }
    return (TRUE);

  case FIND_FIRST_NO_CDS:	/* 0x19 */
  case FIND_FIRST:		/* 0x1b */

    attr = sda_search_attribute(sda);

    Debug0((dbg_fd, "findfirst %s attr=%x\n", filename1, attr));

    /* no more needed, this code didn't seem to work anyway */
#if 0
    if (!strncmp(&filename1[strlen(filename1) - 3], "NUL", 3)) {
      Debug0((dng_fd, "Found a NUL, translating\n"));
      filename1[strlen(filename1) - 4] = 0;
      attr = attr | DIRECTORY;
    }
#endif

    hlist = NULL;

    Debug0((dbg_fd, "attr = 0x%x\n", attr));

    build_ufs_path(fpath, filename1);

    for (i = 0, bs_pos = 0; fpath[i] != EOS; i++) {
      if (fpath[i] == SLASH)
	bs_pos = i;
    }
    fpath[bs_pos] = EOS;

    /* check for NUL quasi-existence */
    /* note: path_to_ufs turns the name into lowercase! */
    if (strcmp(fpath + bs_pos + 1, "nul") == 0) {
   struct stat st;
   sdb_dir_entry(sdb) = HLIST_STACK_SIZE*2; /* no findnext */
   /* check directory existence, bs_pos == 0 -> root, no check needed */
   if (bs_pos > 0 && !find_file(fpath, &st)) {
     SETWORD(&(state->eax), PATH_NOT_FOUND);
     return (FALSE);
   }
   sdb_file_attr(sdb) = 0;
   st.st_mtime = 0;
   time_to_dos(&st.st_mtime, &sdb_file_date(sdb), &sdb_file_time(sdb));
   sdb_file_size(sdb) = 0;
   strncpy(sdb_file_name(sdb), "NUL     ", 8);
   strncpy(sdb_file_ext(sdb), "   ", 3);
   return (TRUE);
    }

    auspr(fpath + bs_pos + 1, fname, fext);
    strncpy(sdb_template_name(sdb), fname, 8);
    strncpy(sdb_template_ext(sdb), fext, 3);
    sdb_attribute(sdb) = attr;
    sdb_drive_letter(sdb) = 0x80 + current_drive;
    sdb_dir_entry(sdb) = HLIST_STACK_SIZE*2;  /* correct value later */

    Debug0((dbg_fd, "Find first %8.8s.%3.3s\n",
	    (char *) sdb_template_name(sdb),
	    (char *) sdb_template_ext(sdb)));
/*
    strncpy(last_find_name, sdb_template_name(sdb), 8);
    strncpy(last_find_ext, sdb_template_ext(sdb), 3);
    last_find_dir = sdb_dir_entry(sdb);
    last_find_drive = sdb_drive_letter(sdb);
*/
#ifndef NO_VOLUME_LABELS
    if (attr & VOLUME_LABEL &&
	strncmp(sdb_template_name(sdb), "????????", 8) == 0 &&
	strncmp(sdb_template_ext(sdb), "???", 3) == 0) {
      Debug0((dbg_fd, "DO LABEL!!\n"));
#if 0
      auspr(VOLUMELABEL, fname, fext);
      sprintf(fext, "%d", current_drive);
#else
      {
	char *label, *root, *p;

	p = dos_roots[current_drive];
	label = (char *) malloc(8 + 3 + 1);
	root = (char *) malloc(strlen(p) + 1);
	strcpy(root, p);
	if (root[strlen(root) - 1] == '/' && strlen(root) > 1)
	  root[strlen(root) - 1] = '\0';
#if 0
	sprintf(label, "%d:", current_drive);
#else
	label[0] = '\0';
#endif
	if (strlen(label) + strlen(root) <= 8 + 3) {
	  strcat(label, root);
	}
	else {
#if 0
	  strcat(label, "...");
#endif
	  strcat(label, root + strlen(root) - (8 + 3 - strlen(label)));
	}
	for (p = label + strlen(label); p < label + 8 + 3; p++)
	  *p = ' ';

	strncpy(fname, label, 8);
	strncpy(fext, label + 8, 3);
	free(label);
	free(root);
      }
#endif
      strncpy(sdb_file_name(sdb), fname, 8);
      strncpy(sdb_file_ext(sdb), fext, 3);
      sdb_file_attr(sdb) = VOLUME_LABEL;
      sdb_dir_entry(sdb) = 0x0;
      /* if (attr != VOLUME_LABEL)
	find_in_progress = TRUE; */
      auspr(fpath + bs_pos + 1, fname, fext);
      if (bs_pos == 0)
	strcpy(fpath, "/");
	   
      hlist = get_dir(fpath, fname, fext);
      if (hlist==NULL)  {
         SETWORD(&(state->eax), PATH_NOT_FOUND);
         return (FALSE);
      }
   
      hlist = match_filename_prune_list(hlist,fname,fext);
				  
      hlist_index = hlist_push(hlist, sda_cur_psp(sda));
      sdb_dir_entry(sdb) = hlist_index;
      return (TRUE);
    }
#else
    if (attr == VOLUME_LABEL)
      return (FALSE);
#endif
    if (bs_pos == 0)
      strcpy(fpath, "/");

    hlist = get_dir(fpath, fname, fext);
    if (hlist==NULL)  {
       SETWORD(&(state->eax), PATH_NOT_FOUND);
       return (FALSE);
    }

    hlist = match_filename_prune_list(hlist, fname, fext);
    hlist_index = hlist_push(hlist, sda_cur_psp(sda));
    sdb_dir_entry(sdb) = hlist_index;
    firstfind = 1;

  find_again:

    Debug0((dbg_fd, "find_again entered with %.8s.%.3s\n", hlist->name, hlist->ext));
    if (hlist == NULL) {
      /* no matches or empty directory */
      Debug0((dbg_fd, "No more matches\n"));
#if 0 /* Hardly any directory is really empty (there are always some vol.labels,
         `.', or `..'), and NO_MORE_FILES is more convenient for this case.
         Moreover, Volkov Commander doesn't like FILE_NOT_FOUND to be returned
         as a result of findfirst, and keeps annoying me due to it. */
      if (firstfind)
        SETWORD(&(state->eax), FILE_NOT_FOUND);
      else
#endif
        SETWORD(&(state->eax), NO_MORE_FILES);
      hlist_stack[hlist_index] = hlist;
      hlist_pop(hlist_index, sda_cur_psp(sda));
      return (FALSE);
    }
    else {
      sdb_file_attr(sdb) = get_dos_attr(hlist->mode,hlist->hidden);

      if (hlist->mode & S_IFDIR) {
	Debug0((dbg_fd, "Directory ---> YES 0x%x\n", hlist->mode));
	if (!(attr & DIRECTORY)) {
	  tmp = hlist->next;
	  free(hlist);
	  hlist = tmp;
	  goto find_again;
	}
      }
      time_to_dos(&hlist->time,
		  &sdb_file_date(sdb),
		  &sdb_file_time(sdb));
      sdb_file_size(sdb) = hlist->size;
      strncpy(sdb_file_name(sdb), hlist->name, 8);
      strncpy(sdb_file_ext(sdb), hlist->ext, 3);

#if 0
      sdb_dir_entry(sdb)++;
#endif

      Debug0((dbg_fd, "'%.8s'.'%.3s' hlist=%d\n",
	      sdb_file_name(sdb),
	      sdb_file_ext(sdb), hlist_index));

      tmp = hlist->next;
      free(hlist);
      hlist = tmp;
    }
    firstfind = 0;
    hlist_stack[hlist_index] = hlist;
    return (TRUE);

  case FIND_NEXT:		/* 0x1c */
    hlist_index = sdb_dir_entry(sdb);
    hlist = hlist_index < hlist_stack_indx ? hlist_stack[hlist_index] : NULL;
    Debug0((dbg_fd, "Find next %8.8s.%3.3s, pointer->hlist=%d\n",
	    (char *) sdb_template_name(sdb),
	    (char *) sdb_template_ext(sdb), (int)hlist));
#if 0
    if (last_find_drive && ((strncmp(last_find_name, sdb_template_name(sdb), 8) != 0 ||
		   strncmp(last_find_ext, sdb_template_ext(sdb), 3) != 0) ||
/*			    (last_find_dir != sdb_dir_entry(sdb)) || */
			    (last_find_drive != sdb_drive_letter(sdb)))) {
      Debug0((dbg_fd, "last_template!=this_template. popping. %.8s,%.3s\n", last_find_name, last_find_ext));
      Debug0((dbg_fd, "last_dir=%x,last_drive=%d sdb_dir=%d,sdb_drive=%d\n", last_find_dir, last_find_drive, sdb_dir_entry(sdb), sdb_drive_letter(sdb)));
      hlist = hlist_pop(sda_cur_psp(sda));

      strncpy(last_find_name, sdb_template_name(sdb), 8);
      strncpy(last_find_ext, sdb_template_ext(sdb), 3);
      last_find_dir = sdb_dir_entry(sdb);
      last_find_drive = sdb_drive_letter(sdb);
      /*
	  if (hlist == NULL)
	    {
	      Debug0((dbg_fd,"double popping!!\n"));
	      hlist = hlist_pop ();
	    }
*/
    }
#endif
    attr = sdb_attribute(sdb);
    goto find_again;
  case CLOSE_ALL:		/* 0x1d */
    Debug0((dbg_fd, "Close All\n"));
    break;
  case FLUSH_ALL_DISK_BUFFERS:	/* 0x20 */
    Debug0((dbg_fd, "Flush Disk Buffers\n"));
    return TRUE;
  case SEEK_FROM_EOF:		/* 0x21 */
    {
      int offset = (state->ecx << 16) + (state->edx);

      fd = sft_fd(sft);
      Debug0((dbg_fd, "Seek From EOF fd=%x ofs=%d\n",
	      fd, offset));
      if (offset > 0)
	offset = -offset;
      offset = lseek(fd, offset, SEEK_END);
      Debug0((dbg_fd, "Seek returns fs=%d ofs=%d\n",
	      fd, offset));
      if (offset != -1) {
	sft_position(sft) = offset;
	state->edx = offset >> 16;
	state->eax = WORD(offset);
	return (TRUE);
      }
      else {
	SETWORD(&(state->eax), SEEK_ERROR);
	return (FALSE);
      }
    }
    break;
  case QUALIFY_FILENAME:	/* 0x23 */
    {
      char *fn = (char *) Addr(state, ds, esi);

#if DOQUALIFY
      char *qfn = (char *) Addr(state, es, edi);
      char *cpath = cds_current_path(cds);

      if (fn[1] == ':') {
	if (fn[2] == '\\')
	  strcpy(qfn, fn);
	else {
	  strcpy(qfn, cpath);
	  if (cpath[strlen(cpath) - 1] != '\\')
	    strcat(qfn, "\\");
	  strcat(qfn, fn + 2);
	}
      }
      else {
	strcpy(qfn, cpath);
	strcat(qfn, fn);
      }
      Debug0((dbg_fd, "Qualify filename %s->%s\n", fn, qfn));
      return (TRUE);
#else
      Debug0((dbg_fd, "Qualify filename %s\n", fn));
#endif
    }
    break;
  case LOCK_FILE_REGION:	/* 0x0a */
	/* The following code only apply to DOS 4.0 and later */
	/* It manage both LOCK and UNLOCK */
	/* I don't know how to find out from here which DOS is running */
	{
		int is_lock = !(state->ebx & 1);
		int fd = sft_fd(sft);
		int ret;
		struct LOCKREC{
			long offset,size;
		} *pt = (struct LOCKREC*) Addr (state,ds,edx);
		struct flock larg;
		unsigned long mask = 0xC0000000;

		larg.l_type = is_lock ? F_WRLCK : F_UNLCK;
		larg.l_whence = SEEK_SET;
		larg.l_start = pt->offset;
		larg.l_len = pt->size;
		larg.l_pid = 0;

		/*
			This is a superdooper patch extract from the Samba project
			We have no idea why this is there but it seem to work
			So a program running in DOSEMU will successfully cooperate
			with the same program running on a remote station and using
			samba to access the same database.

			After doing some experiment with a Clipper program
			(database driver from SuccessWare (SAX)), we have wittness
			unexpected large offset for small database. From this we
			assume that some DOS program are playing game with lock/unlock.
		*/

		/* make sure the count is reasonable, we might kill the lockd otherwise */
		larg.l_len &= ~mask;

		/* the offset is often strange - remove 2 of its bits if either of
			the top two bits are set. Shift the top ones by two bits. This
			still allows OLE2 apps to operate, but should stop lockd from
			dieing */
		if ((larg.l_start & mask) != 0)
			larg.l_start = (larg.l_start & ~mask) | ((larg.l_start & mask) >> 2);

		ret = fcntl (fd,F_SETLK,&larg);
		{
			#if 0
			FILE *fout = fopen ("/tmp/lock.log","a");
			if (fout != NULL){
				fprintf (fout,"lock(%d) %d %d %d %d %d\n",ret
					,larg.l_type,larg.l_whence
					,larg.l_start,larg.l_len);
				fclose (fout);
			}
			#endif
		}
		return ret != -1 ? TRUE : FALSE;
	}
    break;
  case UNLOCK_FILE_REGION:	/* 0x0b */
    Debug0((dbg_fd, "Unlock file region\n"));
    break;
  case PROCESS_TERMINATED:	/* 0x22*/
    Debug0((dbg_fd, "Process terminated PSP=%d\n", state->ds));
    hlist_pop_psp(state->ds);
    return (REDIRECT);
  case CONTROL_REDIRECT:	/* 0x1e */
    /* get low word of parameter, should be one of 2, 3, 4, 5 */
    subfunc = LOW(*(u_short *) Addr(state, ss, esp));
    Debug0((dbg_fd, "Control redirect, subfunction %d\n",
	    subfunc));
    switch (subfunc) {
      /* XXXTRB - need to support redirection index pass-thru */
    case GET_REDIRECTION:
    case EXTENDED_GET_REDIRECTION:
      return (GetRedirection(state, WORD(state->ebx)));
      break;
    case REDIRECT_DEVICE:
      return (RedirectDevice(state));
      break;
    case CANCEL_REDIRECTION:
      return (CancelRedirection(state));
      break;
    default:
      SETWORD(&(state->eax), FUNC_NUM_IVALID);
      return (FALSE);
      break;
    }
    break;
  case COMMIT_FILE:		/* 0x07 */
    Debug0((dbg_fd, "Commit\n"));
    fd = sft_fd(sft);
    return (dos_flush(fd));
    break;
  case MULTIPURPOSE_OPEN:
    {
      boolean_t file_exists;
      u_short action = sda_ext_act(sda);
	  u_short mode;
	  
      mode = sda_ext_mode(sda) & 0x7f;
      attr = *(u_short *) Addr(state, ss, esp);
      Debug0((dbg_fd, "Multipurpose open file: %s\n", filename1));
      Debug0((dbg_fd, "Mode, action, attr = %x, %x, %x\n",
	      mode, action, attr));

      build_ufs_path(fpath, filename1);
      file_exists = find_file(fpath, &st);

      if (((action & 0x10) == 0) && !file_exists) {
	/* Fail if file does not exist */
	SETWORD(&(state->eax), FILE_NOT_FOUND);
	return (FALSE);
      }

      if (((action & 0xf) == 0) && file_exists) {
	/* Fail if file does exist */
	SETWORD(&(state->eax), FILE_ALREADY_EXISTS);
	return (FALSE);
      }

      if (((action & 0xf) == 1) && file_exists) {
	/* Open if does exist */
	SETWORD(&(state->ecx), 0x1);
	goto do_open_existing;
      }

      if (((action & 0xf) == 2) && file_exists) {
	/* Replace if file exists */
	SETWORD(&(state->ecx), 0x3);
	goto do_create_truncate;
      }

      if (((action & 0x10) != 0) && !file_exists) {
	/* Replace if file exists */
	SETWORD(&(state->ecx), 0x2);
	goto do_create_truncate;
      }

      Debug0((dbg_fd, "Multiopen failed: 0x%02x\n",
	      (int) LOW(state->eax)));
      /* Fail if file does exist */
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }
  case EXTENDED_ATTRIBUTES:{
      switch (LOW(state->ebx)) {
      case 2:			/* get extended attributes */
      case 3:			/* get extended attribute properties */
	if (WORD(state->ecx) >= 2) {
	  *(short *) (Addr(state, es, edi)) = 0;
	  SETWORD(&(state->ecx), 0x2);
	}
      case 4:			/* set extended attributes */
	return (TRUE);
      }
      return (FALSE);
    }
  case PRINTER_MODE:{
      Debug0((dbg_fd, "Printer Mode: %02x\n",(int) LOW(state->eax)));
      SETLOW(&(state->edx), 1);
      return (TRUE);
    }
#ifdef UNDOCUMENTED_FUNCTION_2
  case UNDOCUMENTED_FUNCTION_2:
    Debug0((dbg_fd, "Undocumented function: %02x\n",
	    (int) LOW(state->eax)));
    return (TRUE);
#endif
  case PRINTER_SETUP:
    SETWORD(&(state->ebx), WORD(state->ebx) - redirected_drives);
    Debug0((dbg_fd, "Passing %d to PRINTER SETUP CALL\n",
	    (int) WORD(state->ebx)));
    return (REDIRECT);
  default:
    Debug0((dbg_fd, "Default for undocumented function: %02x\n",
	    (int) LOW(state->eax)));
    return (REDIRECT);
  }
  return (ret);
}
