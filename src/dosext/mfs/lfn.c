/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"

#include "emu.h"
#include "mfs.h"
#include "mangle.h"
#include "bios.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <utime.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>

#define EOS '\0'
#define BACKSLASH '\\'
#define SLASH '/'

struct lfndir {
	DIR *dir;
	unsigned dirattr;
	char pattern[PATH_MAX];
	char dirbase[PATH_MAX];
};

#define MAX_OPEN_DIRS 20
struct lfndir *lfndirs[MAX_OPEN_DIRS];

static int current_drive;

static unsigned long long unix_to_win_time(time_t ut)
{
	return ((unsigned long long)ut + (369 * 365 + 89)*24*60*60ULL) *
		10000000;
}

static time_t win_to_unix_time(unsigned long long wt)
{
	return (wt / 10000000) - (369 * 365 + 89)*24*60*60ULL;
}

static void make_dos_mangled_path(char *fpath)
{
	char *src, *dest;
	dest = LFN_string - (long)bios_f000 + (BIOSSEG << 4);
	*dest++ = current_drive + 'A';
	*dest++ = ':';
	*dest = '\\';
	src = fpath + strlen(dos_root);
	if (*src == '/') src++;
	while (src != NULL && *src != '\0') {
		char *src2 = strchr(src, '/');
		if (src2 == src) break;
		if (src2 != NULL)
			*src2++ = '\0';
		dest++;
		d_printf("LFN: src=%s len=%d\n", src, strlen(src));
		if (!strcmp(src, "..") || !strcmp(src, ".")) {
			strcpy(dest, src);
		} else {
			name_convert(dest, src, MANGLE, NULL);
			strupperDOS(dest);
		}
		dest += strlen(dest);
		*dest = '\\';
		src = src2;
	}
	if (dest[-1] == ':') dest++;
	*dest = '\0';
}

static int build_posix_path(char *dest, const char *src)
{
	char filename[PATH_MAX];
	int dd;

	d_printf("LFN: build_posix_path: %s\n", src);
	
	if (src[1] == ':') {
		dd = toupperDOS(src[0]) - 'A';
		src += 2;
	} else {
		dd = sda_cur_drive(sda);
	}
	if (dd < 0 || dd >= MAX_DRIVE || !dos_roots[dd])
		return 0;
	current_drive = dd;
	dos_root = dos_roots[current_drive];
	dos_root_len = dos_root_lens[current_drive];
	cds = drive_cds(current_drive);
	while(src[0] == '.' && (src[1] == '/' || src[1] == '\\'))
		src += 2;
	if (src[0] != '/' && src[0] != '\\') {
		strcpy(filename, cds_current_path(cds));
		strcat(filename, "/");
		strcat(filename, src);
		build_ufs_path(dest, filename);
	} else {
		build_ufs_path(dest, src);
	}
	dest = strchr(dest, '\r');
	if (dest) *dest = '\0';
	return 1;
}

static int getfindnext(char *fpath)
{
	struct stat st;
	char *dest = (char *)SEGOFF2LINEAR(_ES, _DI);
	
	memset(dest, 0, 0x20);
	d_printf("LFN: findnext %s\n", fpath);
	if (!find_file(fpath, &st))
		return 0;
	*dest = get_dos_attr(st.st_mode,is_hidden(fpath));
	*((unsigned *)(dest + 0x20)) = st.st_size;
	if (_SI == 1) {
		d_printf("LFN: using DOS date/time\n");
		time_to_dos(st.st_mtime,
			    (ushort *)(dest+0x16),
			    (ushort *)(dest+0x14));
		time_to_dos(st.st_ctime,
			    (ushort *)(dest+0x6),
			    (ushort *)(dest+0x4));
		time_to_dos(st.st_atime,
			    (ushort *)(dest+0xe),
			    (ushort *)(dest+0xc));
	} else {
		d_printf("LFN: using WIN date/time\n");
		*(unsigned long long *)(dest+0x14) =
			unix_to_win_time(st.st_mtime);
		*(unsigned long long *)(dest+0x4) =
			unix_to_win_time(st.st_ctime);
		*(unsigned long long *)(dest+0xc) =
			unix_to_win_time(st.st_atime);
	}

	fpath = strrchr(fpath, '/') + 1;
	strcpy(dest + 0x2c, fpath);
	dest += 0x130;
	name_convert(dest,fpath,MANGLE,NULL);
	strupperDOS(dest);
	return 1;
}

static int lfn_error(int errorcode)
{
	_AX = sda_error_code(sda) = errorcode;
	CARRY;
	return 1;
}

static void call_dos_helper(int ax)
{
	unsigned char *ssp = (unsigned char *)(_SS<<4);
	unsigned long sp = (unsigned long) _SP;
	_AX = ax;
	pushw(ssp, sp, _CS);
	pushw(ssp, sp, _IP);
	_SP -= 4;
	_CS = LFN_HELPER_SEG;
	_IP = LFN_HELPER_OFF;
}


/* the general idea here is:
  - convert given pathname to a Unix path by prepending the relevant
    "lredir" directory and flipping slashes
  - then we must do a case-*insensitive* and SFN matching search to
    find out if the path exists. This is done by find_file(); this
    can be a little slow but is necessary for compatibility.
  - following that the operation can be done directly or must be
    passed on to DOS using SFNs (chdir, open) using a small BIOS stub.
  - the SFNs are the Samba style SFNs that DOSEMU has used for as
    long as I can remember.
*/  

int mfs_lfn(void)
{
	char *cwd;
	char fpath[PATH_MAX];
	char fpath2[PATH_MAX];
	
	int dd, dirhandle = 0;
	char *dest = (char *)SEGOFF2LINEAR(_ES, _DI);
	char *src = (char *)SEGOFF2LINEAR(_DS, _DX);
	struct stat st;
	struct utimbuf utimbuf;
	size_t size;
	struct dirent *de;
	char *slash;
	struct lfndir *dir = NULL;
	
	d_printf("LFN: doing LFN!, AX=%x DL=%x\n", _AX, _DL);
	NOCARRY;
	switch (_AL) {
	case 0x0D: /* reset drive, nothing to do */
		break;
	case 0x39: /* mkdir */
		d_printf("LFN: mkdir %s\n", src);
		if (!build_posix_path(fpath, src))
			return 0;
		if (is_dos_device(fpath))
			return lfn_error(PATH_NOT_FOUND);
		slash = strrchr(fpath, '/');
		strcpy(fpath2, slash);
		*slash = '\0';
		if (!find_file(fpath, &st))
			return lfn_error(PATH_NOT_FOUND);
		strcat(fpath, fpath2);
		if (find_file(fpath, &st) || (mkdir(fpath, 0755) != 0))
			return lfn_error(ACCESS_DENIED);
		break;
	case 0x3a: /* rmdir */
		if (!build_posix_path(fpath, src))
			return 0;
		if (!find_file(fpath, &st))
			return lfn_error(PATH_NOT_FOUND);
		d_printf("LFN: rmdir %s\n", fpath);
		if (rmdir(fpath) != 0)
			return lfn_error(PATH_NOT_FOUND);
		break;
	case 0x3b: /* chdir */
		Debug0((dbg_fd, "set directory to: %s\n", src));
		d_printf("LFN: chdir %s %d\n", src, strlen(src));
		if (!build_posix_path(fpath, src))
			return 0;			 
		if (!find_file(fpath, &st) || !S_ISDIR(st.st_mode))
			return lfn_error(PATH_NOT_FOUND);
		make_dos_mangled_path(fpath);
		d_printf("LFN: New CWD will be %s\n", dest);
		call_dos_helper(0x3b00);
		break;
	case 0x41: /* remove file */
		if (!build_posix_path(fpath, src))
			return 0;			 
		if (!find_file(fpath, &st))
			return lfn_error(FILE_NOT_FOUND);
		d_printf("LFN: deleting %s\n", fpath);
		if (unlink(fpath) != 0)
			return lfn_error(FILE_NOT_FOUND);
		break;
	case 0x43: /* get/set file attributes */
		d_printf("LFN: attribute %s %d\n", src, _BL);
		if (!build_posix_path(fpath, src))
			return 0;			 
		if (!find_file(fpath, &st) || is_dos_device(fpath)) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			return lfn_error(FILE_NOT_FOUND);
		}
		utimbuf.actime = st.st_atime;
		utimbuf.modtime = st.st_mtime;
		switch (_BL) {
		case 0: /* retrieve attributes */
			_CX = get_dos_attr(st.st_mode,is_hidden(fpath));
			break;
		case 1: /* set attributes */
			if (chmod(fpath, get_unix_attr(st.st_mode, _CX)) != 0) {
				return lfn_error(ACCESS_DENIED);
			}
			break;
		case 2: /* get physical size of uncompressed file */
			_DX = st.st_size >> 16;
			_AX = st.st_size & 0xffff;
			break; 
		case 3: /* set last write date/time */
			utimbuf.modtime = time_to_unix(_DI, _CX);
			if (utime(fpath, &utimbuf) != 0)
				return lfn_error(ACCESS_DENIED);
			break;
		case 4: /* get last write date/time */
			time_to_dos(st.st_mtime, &_DI, &_CX);
			break;
		case 5: /* set last access date */
			utimbuf.actime = time_to_unix(_DI, _CX);
			if (utime(fpath, &utimbuf) != 0)
				return lfn_error(ACCESS_DENIED);
			break;
		case 6: /* get last access date */
		{
			short scratch;
			time_to_dos(st.st_atime, &_DI, &scratch);
			break;
		}
		case 7: /* set creation date/time, impossible in Linux */
			break;
		case 8: /* get creation date/time */
			time_to_dos(st.st_ctime, &_DI, &_CX);
			_SI = (st.st_ctime & 1) ? 100 : 0;
			break;
		}
		break;
	case 0x47: /* get current directory */
		if (_DL == 0)
			dd = sda_cur_drive(sda);
		else
			dd = _DL - 1;
		if (dd < 0 || dd >= MAX_DRIVE)
			return lfn_error(DISK_DRIVE_INVALID);
		if (!dos_roots[dd])
			return 0;
			
		current_drive = dd;
		dos_root = dos_roots[current_drive];
		dos_root_len = dos_root_lens[current_drive];
		cds = drive_cds(current_drive);
		cwd = cds_current_path(cds);
		dest = (char *)SEGOFF2LINEAR(_DS, _SI);
		build_ufs_path(fpath, cwd);
		d_printf("LFN: getcwd %s %s %s\n", cwd, fpath, dos_root);
		find_file(fpath, &st);
		d_printf("LFN: getcwd %s %s %s\n", cwd, fpath, dos_root);
		for (cwd = fpath; *cwd; cwd++)
			if (*cwd == '/')
				*cwd = '\\';
		d_printf("LFN: %p %d %p %s\n", cds, current_drive, dest, fpath+strlen(dos_root));
		strcpy(dest, fpath+strlen(dos_root));
		break;
	case 0x4e: /* find first */
	{
		int dotpos;

		if (!build_posix_path(fpath, src))
			return 0;			 
		slash = strrchr(fpath, '/');
		d_printf("LFN: posix:%s\n", fpath);
		if (!strchr(slash, '*') && !strchr(slash, '?')) {
			if (getfindnext(fpath)) {
				_AX = _CX = 0;
				break;
			} else {
				return lfn_error(NO_MORE_FILES);
			}
		}
		*slash++ = '\0';
		if (slash - 3 > fpath && slash[-3] == '/' && slash[-2] == '.')
			slash[-3] = '\0';
		/* note: DJGPP doesn't like "0" as a directory handle */
		for (dirhandle = 1; dirhandle < MAX_OPEN_DIRS; dirhandle++)
			if (lfndirs[dirhandle] == NULL)
				break;
		if (dirhandle == MAX_OPEN_DIRS) {
			d_printf("LFN: too many dirs open\n");
			return lfn_error(NO_MORE_FILES);
		}
		dir = malloc(sizeof *dir);
		strcpy(dir->dirbase, fpath);
		dir->dirattr = _CX;
		strcpy(dir->pattern, slash);
		dotpos = strlen(dir->pattern) - 2;
		if (dotpos > 0 && strcmp(&dir->pattern[dotpos], ".*") == 0)
			dir->pattern[dotpos] = '*';
		/* XXX check for device (special dir entry) */
		if (!find_file(fpath, &st) || is_dos_device(fpath)) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			return lfn_error(NO_MORE_FILES);
		}
		dir->dir = opendir(fpath);
		if (dir->dir == NULL) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			free(dir);
			return lfn_error(NO_MORE_FILES);
		}
		d_printf("LFN: findfirst %s %s %s %x\n", slash, src, fpath, _CX);
		lfndirs[dirhandle] = dir;
		/* fall through! */
	}
	case 0x4f: /* find next */
	case 0xa2:
	{
		if (_AL != 0x4e && _BX < MAX_OPEN_DIRS) {
			dirhandle = _BX;
			dir = lfndirs[dirhandle];
		}
		if (dir == NULL)
			return lfn_error(NO_MORE_FILES);
		while (1) {
			de = readdir(dir->dir);
			if (de == NULL) {
				closedir(dir->dir);
				free(dir);
				lfndirs[dirhandle] = NULL;
				return lfn_error(NO_MORE_FILES);
			}
			d_printf("LFN: findnext %s %x\n", de->d_name, dir->dirattr);
			if (de->d_type == DT_DIR) {
				if ((dir->dirattr & DIRECTORY) == 0)
					continue;
			} else {
				if (((dir->dirattr >> 8) & DIRECTORY))
					continue;
			}
			if (fnmatch(dir->pattern, de->d_name, FNM_CASEFOLD) == 0) {
				strcpy(fpath, dir->dirbase);
				strcat(fpath, "/");
				strcat(fpath, de->d_name);
				getfindnext(fpath);
				if (_AL != 0x4e)
					_AX = 0x4f00 + dirhandle;
				else
					_AX = dirhandle;
				_CX = 0;
				break;
			}
		}
		break;
	}
	case 0x56: /* rename file */
	{
		d_printf("LFN: rename to %s\n", dest);
		if (!build_posix_path(fpath2, dest))
			return 0;			 
		slash = strrchr(fpath2, '/');
		strcpy(fpath, slash);
		*slash = '\0';
		if (!find_file(fpath2, &st))
			return lfn_error(PATH_NOT_FOUND);
		strcat(fpath2, fpath);
		if (is_dos_device(fpath2))
			return lfn_error(FILE_NOT_FOUND);
		d_printf("LFN: rename from %s\n", src);
		if (!build_posix_path(fpath, src))
			return 1;			 
		if (!find_file(fpath, &st) || is_dos_device(fpath)) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			return lfn_error(FILE_NOT_FOUND);
		}
		if (rename(fpath, fpath2) != 0)
			return lfn_error(FILE_NOT_FOUND);
		break;
	}	
	case 0x60: /* truename */
		src = (char *)SEGOFF2LINEAR(_DS, _SI);
		d_printf("LFN: truename %s, cl=%d\n", src, _CL);
		if (!build_posix_path(fpath, src))
			return 0;
		find_file(fpath, &st);
		d_printf("LFN: %s %s\n", fpath, dos_root);
		for (cwd = fpath; *cwd; cwd++)
			if (*cwd == '/')
				*cwd = '\\';
		dest[0] = current_drive + 'A';
		dest[1] = ':';
		dest[2] = '\\';
		strcpy(dest + 3, fpath+strlen(dos_root));
		if (_CL == 1)
			make_dos_mangled_path(dest + 3);
		d_printf("LFN: %s %s\n", dest, dos_root);
		break;
	case 0x6c: /* create/open */
		src = (char *)SEGOFF2LINEAR(_DS, _SI);
		d_printf("LFN: open %s\n", src);
		if (!build_posix_path(fpath, src))
			return 0;		 
		if (is_dos_device(fpath)) {
                        dest = LFN_string - (long)bios_f000 + (BIOSSEG << 4);
			strcpy(dest, strrchr(fpath, '/') + 1);
		} else {
			slash = strrchr(fpath, '/');
			strcpy(fpath2, slash);
			*slash = '\0';
			if (!find_file(fpath, &st))
				return lfn_error(PATH_NOT_FOUND);
			strcat(fpath, fpath2);
			if ((_DX & 0x10) && !find_file(fpath, &st)) {
				int fd = open(fpath, (O_RDWR | O_CREAT),
					      get_unix_attr(0664, _CL |
							    ARCHIVE_NEEDED));
				if (fd < 0) {
					d_printf("LFN: creat problem: %o %s %s\n",
						 get_unix_attr(0644, _CX),
						 fpath, strerror(errno));
					return lfn_error(ACCESS_DENIED);
				}
				d_printf("LFN: open: created %s\n", fpath);
				close(fd);
			}
			make_dos_mangled_path(fpath);
		}
		call_dos_helper(0x6c00);
		break;
	case 0xa0: /* get volume info */
		if (!build_posix_path(fpath, src))
			return 0;			 
		size = _CX;
		_AX = 0;
		_BX = 0x4002;
		_CX = 255;
		_DX = 260;
		if (size >= 4)
			strcpy(dest, "MFS");
		break;
	case 0xa1: /* findclose */
		d_printf("LFN: findclose %x\n", _BX);
		if (_BX < MAX_OPEN_DIRS && lfndirs[_BX]) {
			closedir(lfndirs[_BX]->dir);
			free(lfndirs[_BX]);
			lfndirs[_BX] = NULL;
		}
		break;
	case 0xa6: /* get file info by handle */
		d_printf("LFN: get file info by handle %x\n", _BX);
		return 0;
	case 0xa7: /* file time to DOS time and v.v. */
		if (_BL == 0) {
			src = (char *)SEGOFF2LINEAR(_DS, _SI);
			time_to_dos(
				win_to_unix_time(*(unsigned long long *)src),
				&_DX, &_CX);
			_BH = 0;
		} else {
			*(unsigned long long *)dest =
				unix_to_win_time(time_to_unix(_DX, _CX));
		}
		break;
	case 0xa8: /* generate short filename */
	{
		char name[8];
		char ext[3];
		src = (char *)SEGOFF2LINEAR(_DS, _SI);
		strcpy(fpath, src);
		auspr(fpath, name, ext);
		if (_DH == 0) {
			memcpy(dest, name, 8);
			memcpy(dest+8, ext, 3);
		} else {
			strcpy(dest, fpath);
		}
		d_printf("LFN: name convert %s %s %x\n", src, fpath, _DH);
		break;
	}
	case 0xa9: /* server create or open file */
	case 0xaa: /* create/terminate/query subst */
	default: 
		return 0;
	}
	return 1; /* finished: back to caller */
}
