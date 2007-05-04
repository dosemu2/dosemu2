/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <wctype.h>

#include "emu.h"
#include "mfs.h"
#include "mangle.h"
#include "dos2linux.h"
#include "bios.h"
#include "lfn.h"

#define EOS '\0'
#define BACKSLASH '\\'
#define SLASH '/'

struct lfndir {
	int drive;
	struct mfs_dir *dir;
	unsigned dirattr;
	unsigned psp;
	char pattern[PATH_MAX];
	char dirbase[PATH_MAX];
};

#define MAX_OPEN_DIRS 20
struct lfndir *lfndirs[MAX_OPEN_DIRS];

static unsigned long long unix_to_win_time(time_t ut)
{
	return ((unsigned long long)ut + (369 * 365 + 89)*24*60*60ULL) *
		10000000;
}

static time_t win_to_unix_time(unsigned long long wt)
{
	return (wt / 10000000) - (369 * 365 + 89)*24*60*60ULL;
}

static int close_dirhandle(int handle)
{
	struct lfndir *dir;
	if (handle < MAX_OPEN_DIRS && lfndirs[handle]) {
		dir = lfndirs[handle];
		if (dir->dir) dos_closedir(dir->dir);
		free(dir);
		lfndirs[handle] = NULL;
		return 1;
	}
	return 0;
}

/* close all findfirst handles belonging to the current psp */
void close_dirhandles(unsigned psp)
{
	int dirhandle;
	for (dirhandle = 1; dirhandle < MAX_OPEN_DIRS; dirhandle++) {
		struct lfndir *dir = lfndirs[dirhandle];
		if (dir && dir->psp == psp) {
			close_dirhandle(dirhandle);
		}
	}
}

static int vfat_search(char *dest, char *src, char *path, int alias)
{
	struct mfs_dir *dir = dos_opendir(path);
	struct mfs_dirent *de;
	if (dir == NULL)
		return 0;
	if (dir->dir == NULL) while ((de = dos_readdir(dir)) != NULL) {
		d_printf("LFN: vfat_search %s %s %s %s\n", de->d_name,
			 de->d_long_name, src, path);
		if ((strcasecmp(de->d_long_name, src) == 0) ||
		    (strcasecmp(de->d_name, src) == 0)) {
			char *name = alias ? de->d_name : de->d_long_name;
			if (!name_ufs_to_dos(dest, name) || alias) {
				name_convert(dest, MANGLE);
				strupperDOS(dest);
			}
			dos_closedir(dir);
			return 1;
		}
	}
	dos_closedir(dir);	  
	return 0;
}      

/* input: fpath = unix path
	  current_drive = drive for DOS path
	  alias=1: mangle, alias=0: don't mangle
   output: dest = DOS path
*/
void make_unmake_dos_mangled_path(char *dest, char *fpath,
					 int current_drive, int alias)
{
	char *src;
	*dest++ = current_drive + 'A';
	*dest++ = ':';
	*dest = '\\';
	src = fpath + strlen(drives[current_drive].root);
	if (*src == '/') src++;
	while (src != NULL && *src != '\0') {
		char *src2 = strchr(src, '/');
		if (src2 == src) break;
		if (src - 1 > fpath)
			src[-1] = '\0';
		if (src2 != NULL)
			*src2++ = '\0';
		dest++;
		d_printf("LFN: src=%s len=%zd\n", src, strlen(src));
		if (!strcmp(src, "..") || !strcmp(src, ".")) {
			strcpy(dest, src);
		} else if (!vfat_search(dest, src, fpath, alias)) {
			if (!name_ufs_to_dos(dest, src) || alias) {
				name_convert(dest, MANGLE);
				strupperDOS(dest);
			}
		}
		dest += strlen(dest);
		*dest = '\\';
		if (src - 1 > fpath)
			src[-1] = '/';
		src = src2;
	}
	if (dest[-1] == ':') dest++;
	*dest = '\0';
}

/* truename function, adapted from the FreeDOS kernel truename;
   (Svante Frey, James Tabor, Steffen Kaiser, Bart Oldeman,
   Ron Cemer) but now for LFNs */

#define PATH_ERROR goto errRet
#define PATHLEN 256

#define PNE_WILDCARD 1
#define PNE_WILDCARD_STAR 2

#define addChar(c) \
{ \
  if (p >= dest + 256) PATH_ERROR; /* path too long */	\
  *p++ = c; \
}

#define CDSVALID	(CDS_FLAG_REMOTE | CDS_FLAG_READY)
#define CDSJOINED	0x2000 /* not in combination with NETWDRV or SUBST */
#define CDSSUBST	0x1000 /* not in combination with NETWDRV or JOINED */

static const char *get_root(const char * fname)
{
	/* find the end					*/
	register unsigned length = strlen(fname);
	char c;

	/* now back up to first path seperator or start */
	fname += length;
	while (length) {
		length--;
		c = *--fname;
		if (c == '/' || c == '\\' || c == ':') {
			fname++;
			break;
		}
	}
	return fname;
}

/* helper for truename: parses either name or extension */
static int parse_name(const char **src, char **cp, char *dest)
{
	int retval = 0;
	char *p = *cp;
	char c;
  
	while(1) switch(c=*(*src)++) {
	case '/':
	case '\\':
	case '\0':
		/* delete trailing periods and spaces */
		p[0] = '\0';
		while ((p[-1] == '.' || p[-1] == ' ') && p != *cp)
			p--;
		if (p == *cp)
			return -1;
		/* keep one trailing period if a * was encountered */
		if (p[0] == '.' && (retval & PNE_WILDCARD_STAR))
			p++;		
		*cp = p;
		return retval;
	case '*':
		retval |= PNE_WILDCARD_STAR;
		/* fall through */
	case '?':
		retval |= PNE_WILDCARD;
		/* fall through */
	default:
		addChar(c);
	}
  
 errRet:
	return -1;
}

static int truename(char *dest, const char *src, int allowwildcards)
{
	int i;
	int result;
	int gotAnyWildcards = 0;
	cds_t cds;
	char *p = dest;	  /* dynamic pointer into dest */
	char *rootPos;
	char src0;
	enum { DONT_ADD, ADD, ADD_UNLESS_LAST } addSep;
	unsigned flags;
	const char *froot = get_root(src);

	d_printf("truename(%s)\n", src);

	/* In opposite of the TRUENAME shell command, an empty string is
	   rejected by MS DOS 6 */
	src0 = src[0];
	if (src0 == '\0')
		return -FILE_NOT_FOUND;

	if (src0 == '\\' && src[1] == '\\') {
		const char *unc_src = src;
		/* Flag UNC paths and short circuit processing.	 Set current LDT   */
		/* to sentinel (offset 0xFFFF) for redirector processing.	   */
		d_printf("Truename: UNC detected\n");
		do {
			src0 = unc_src[0];
			addChar(src0);
			unc_src++;
		} while (src0);
		((far_t *)&sda[sda_cds_off])->offset = 0xFFFF;
		((far_t *)&sda[sda_cds_off])->segment = 0xFFFF;
		d_printf("Returning path: \"%s\"\n", dest);
		/* Flag as network - drive bits are empty but shouldn't get */
		/* referenced for network with empty current_ldt.	    */
		return 0;
	}
  
	/* Do we have a drive?						*/
	if (src[1] == ':')
		result = toupper(src0) - 'A';
	else
		result = sda_cur_drive(sda);

	if (result < 0 || result >= MAX_DRIVE || result >= lol_last_drive(lol))
		return -PATH_NOT_FOUND;

	cds = drive_cds(result);
	flags = cds_flags(cds);

	/* Entry is disabled or JOINed drives are accessable by the path only */
	if (!(flags & CDSVALID) || (flags & CDSJOINED) != 0)
		return -PATH_NOT_FOUND;

	if (!drives[result].root) {
		if (!(flags & CDSSUBST))
			return result;
		result = toupper(cds_current_path(cds)[0]) - 'A';
		if (result < 0 || result >= MAX_DRIVE || 
		    result >= lol_last_drive(lol))
			return -PATH_NOT_FOUND;
		if (!drives[result].root)
			return result;
		flags = cds_flags(drive_cds(result));
	}

	if (!(flags & CDS_FLAG_REMOTE))
		return result;

	d_printf("CDS entry: #%u @%p (%u) '%s'\n", result, cds,
		   cds_rootlen(cds), cds_current_path(cds));
	((far_t *)&sda[sda_cds_off])->offset = 
		lol_cdsfarptr(lol).offset + result * cds_record_size;
 
	dest[0] = (result & 0x1f) + 'A';
	dest[1] = ':';

	/* Do we have a drive? */
	if (src[1] == ':')
		src += 2;

	/* check for a device  */
	dest[2] = '\\';

	froot = get_root(src);
	if (is_dos_device(froot)) {
		if (froot == src || froot == src + 5) {
			if (froot == src + 5) {
				int j;
				memcpy(dest + 3, src, 5);
				for (j = 0; j < 5; j++)
					dest[3+j] = toupper(dest[3+j]);
				if (dest[3] == '/') dest[3] = '\\';
				if (dest[7] == '/') dest[7] = '\\';
			}
			if (froot == src ||
			    memcmp(dest + 3, "\\DEV\\", 5) == 0) {
				/* \dev\nul -> c:/nul */
				dest[2] = '/';
				src = froot;
			}
		}
	}
    
	/* Make fully-qualified logical path */
	/* register these two used characters and the \0 terminator byte */
	/* we always append the current dir to stat the drive;
	   the only exceptions are devices without paths */
	rootPos = p = dest + 2;
	if (*p != '/') { /* i.e., it's a backslash! */
		d_printf("SUBSTing from: %s\n", cds_current_path(cds));
/* What to do now: the logical drive letter will be replaced by the hidden
   portion of the associated path. This is necessary for NETWORK and
   SUBST drives. For local drives it should not harm.
   This is actually the reverse mechanism of JOINED drives. */

		memcpy(dest, cds_current_path(cds), cds_rootlen(cds));
		if (cds_flags(cds) & CDSSUBST) {
			/* The drive had been changed --> update the CDS pointer */
			if (dest[1] == ':') { 
				/* sanity check if this really 
				   is a local drive still */
				unsigned i = toupper(dest[0]) - 'A';
				
				if (i < lol_last_drive(lol))
					/* sanity check #2 */
					result = (result & 0xffe0) | i;
				}
		}
		rootPos = p = dest + cds_rootlen(cds);
		*p = '\\'; /* force backslash! */
		p++;

		if (cds_current_path(cds)[cds_rootlen(cds)] == '\0')
			p[0] = '\0';
		else
			strcpy(p, cds_current_path(cds) + cds_rootlen(cds) + 1);
		if (*src != '\\' && *src != '/')
			p += strlen(p);
		else /* skip the absolute path marker */
			src++;
		/* remove trailing separator */
		if (p[-1] == '\\') p--;
	}

	/* append the path specified in src */
	addSep = ADD;			/* add separator */

	while(*src) {
		/* New segment.	 If any wildcards in previous
		   segment(s), this is an invalid path. */
		if (gotAnyWildcards)
			return -PATH_NOT_FOUND;
		switch(*src++)
		{   
		case '/':
		case '\\':	/* skip multiple separators (duplicated slashes) */
			addSep = ADD;
			break;
		case '.':	/* special directory component */
			switch(*src)
			{
			case '/':
			case '\\':
			case '\0':
				/* current path -> ignore */
				addSep = ADD_UNLESS_LAST;
				/* If (/ or \) && no ++src
				   --> addSep = ADD next turn */
				continue;	/* next char */
			case '.':	/* maybe ".." entry */
			another_dot:
				switch(src[1])
				{
				case '/':
				case '\\':
				case '\0':
				case '.':
					/* remove last path component */
					while(*--p != '\\')
						if (p <= rootPos) /* already on root */
							return -PATH_NOT_FOUND;
					/* the separator was removed -> add it again */
					++src;		/* skip the second dot */
					if (*src == '.')
						goto another_dot;
					/* If / or \, next turn will find them and
					   assign addSep = ADD */
					addSep = ADD_UNLESS_LAST;
					continue;	/* next char */
				}
			}
		default:	/* normal component */
			if (addSep != DONT_ADD) {
				/* append backslash */
				addChar(*rootPos);
				addSep = DONT_ADD;
			}
	
			--src;
			/* first character skipped in switch() */
			i = parse_name(&src, &p, dest);
			if (i == -1)
				PATH_ERROR;
			if (i & PNE_WILDCARD)
				gotAnyWildcards = TRUE;
			--src;			/* terminator or separator was skipped */
			break;
		}
	}
	if (gotAnyWildcards && !allowwildcards)
		return -PATH_NOT_FOUND;
	if (addSep == ADD || p == dest + 2) {
		/* MS DOS preserves a trailing '\\', so an access to "C:\\DOS\\"
		   or "CDS.C\\" fails. */
		/* But don't add the separator, if the last component was ".." */
		/* we must also add a seperator if dest = "c:" */  
		addChar('\\');
	}
  
	*p = '\0';				/* add the string terminator */
  
	d_printf("Absolute logical path: \"%s\"\n", dest);
  
	/* look for any JOINed drives */
	if (dest[2] != '/' && lol_njoined(lol)) {
		cds_t cdsp = cds_base;
		for(i = 0; i < lol_last_drive(lol); ++i, ++cdsp) {
			/* How many bytes must match */
			size_t j = strlen(cds_current_path(cdsp));
			/* the last component must end before the backslash
			   offset and the path the drive is joined to leads
			   the logical path */
			if ((cds_flags(cdsp) & CDSJOINED)
			    && (dest[j] == '\\' || dest[j] == '\0')
			    && memcmp(dest, cds_current_path(cdsp), j) == 0) { 
				/* JOINed drive found */
				dest[0] = i + 'A'; /* index is physical here */
				dest[1] = ':';
				if (dest[j] == '\0') {/* Reduce to rootdirec */
					dest[2] = '\\';
					dest[3] = 0;
					/* move the relative path right behind
					   the drive letter */
				}
				else if (j != 2) {
					strcpy(dest + 2, dest + j);
				}
				result = (result & 0xffe0) | i;
				((far_t *)&sda[sda_cds_off])->offset = 
					lol_cdsfarptr(lol).offset +
					(cdsp - cds_base);
				d_printf("JOINed path: \"%s\"\n", dest);
				return result;
			}
		}
		/* nothing found => continue normally */
	}
	d_printf("Physical path: \"%s\"\n", dest);
	return result;

 errRet:
	/* The error is either PATHNOTFND or FILENOTFND
	   depending on if it is not the last component */
	return strchr(src, '/') == 0 && strchr(src, '\\') == 0
		? -FILE_NOT_FOUND
		: -PATH_NOT_FOUND;
}

static inline int build_ufs_path(char *ufs, const char *path, int drive)
{
	return build_ufs_path_(ufs, path, drive, 0);
}

static int lfn_error(int errorcode)
{
	_AX = sda_error_code(sda) = errorcode;
	CARRY;
	return 1;
}

static int build_truename(char *dest, const char *src, int mode)
{
	int dd;

	d_printf("LFN: build_posix_path: %s\n", src);
	dd = truename(dest, src, mode);
	
	if (dd < 0) {
		lfn_error(-dd);
		return -1;
	}

	if (src[0] == '\\' && src[1] == '\\') {
		if (strncasecmp(src, LINUX_RESOURCE, strlen(LINUX_RESOURCE)) != 0)
			return  -2;
		return MAX_DRIVE - 1;
	}

	if (dd > MAX_DRIVE || !drives[dd].root)
		return -2;

	if (!((cds_flags(drive_cds(dd))) & CDS_FLAG_REMOTE) ||
	    !drives[dd & 0x1f].root)
		return -2;
	return dd;
}


static int build_posix_path(char *dest, const char *src, int allowwildcards)
{
	char filename[PATH_MAX];
	int dd;

	dd = build_truename(filename, src, allowwildcards);
	if (dd < 0)
		return dd;

	build_ufs_path(dest, filename, dd);
	return dd;
}


/* wildcard match routine, losely based upon the GLIBC fnmatch
   routine */
static int recur_match(const char *pattern, const char *string)
{
	unsigned char c;
	while ((c = *pattern++) != '\0') {
		if (c == '*') {
			while ((c = *pattern++) == '?' || c == '*') {
				if (c == '?' && *string++ == '\0')
					return 1;
			}
			if (c == '\0')
				/* * at end of pattern, matches anything */
				return 0;
			for (pattern--; *string != '\0'; string++) {
				/* recursive search if rest of pattern
				   matches a string part */
				if (recur_match(pattern, string) == 0)
					return 0;
			}
			/* no match, failure... */
			return 1;				
		} else if (c == '?') {
			if (*string++ == '\0')
				return 1;
		} else {
			if ((unsigned char)pattern[-1] != toupperDOS(*string++))
				return 1;
		}
	}
	/* at end of string, then no differences, success == 0 */
	/* also check for a trailing dot so that *. works      */
	return !((*string == '\0') || (*string == '.' && string[1] == '\0'));
}

static int wild_match(char *pattern, char *string)
{
	char *dotpos;
	int rc;
	size_t slen = strlen(string);

	/* add a trailing period if there is no period, so that *.* matches */
	dotpos = NULL;
	if (!strchr(string, '.')) {
		dotpos = string + slen;
		strcpy(dotpos, ".");
		slen++;
	}
	rc = recur_match(pattern, string);
	if (dotpos) *dotpos = '\0';
	return rc;
}

static int lfn_sfn_match(char *pattern, struct mfs_dirent *de, char *lfn, char *sfn)
{
	if (!name_ufs_to_dos(lfn, de->d_long_name)) {
		name_convert(lfn, MANGLE);
		strupperDOS(lfn);
	}
	name_ufs_to_dos(sfn, de->d_name);
	name_convert(sfn, MANGLE);
	return wild_match(pattern, lfn) != 0 &&
		wild_match(pattern, sfn) != 0;
}

static int getfindnext(struct mfs_dirent *de, struct lfndir *dir)
{
	char name_8_3[PATH_MAX];
	char name_lfn[PATH_MAX];
	struct stat st;
	char *fpath;
	unsigned int dest;

	if (lfn_sfn_match(dir->pattern, de, name_lfn, name_8_3) != 0)
		return 0;	

	if (!strcmp(de->d_long_name,".") || !strcmp(de->d_long_name,"..")) {
		if (strlen(dir->dirbase) <= drives[dir->drive].root_len)
			return 0;
	}

	fpath = malloc(strlen(dir->dirbase) + 1 + strlen(de->d_long_name) + 1);
	strcpy(fpath, dir->dirbase);
	strcat(fpath, "/");
	strcat(fpath, de->d_long_name);
	d_printf("LFN: findnext %s\n", fpath);
	if (stat(fpath, &st) != 0) {
		free(fpath);
		return 0;
	}
	if (st.st_mode & S_IFDIR) {
		if ((dir->dirattr & DIRECTORY) == 0) {
			free(fpath);
			return 0;
		}
	} else {
		if ((dir->dirattr >> 8) & DIRECTORY) {
			free(fpath);
			return 0;
		}
	}

	dest = SEGOFF2LINEAR(_ES, _DI);
	MEMSET_DOS(dest, 0, 0x20);
	WRITE_BYTE(dest, get_dos_attr(fpath,st.st_mode,is_hidden(de->d_long_name)));
	free(fpath);
	WRITE_DWORD(dest + 0x20, st.st_size);
	if (_SI == 1) {
		u_short date, time;
		d_printf("LFN: using DOS date/time\n");
		time_to_dos(st.st_mtime, &date, &time);
		WRITE_WORD(dest+0x16, date);
		WRITE_WORD(dest+0x14, time);
		time_to_dos(st.st_ctime, &date, &time);
		WRITE_WORD(dest+0x6, date);
		WRITE_WORD(dest+0x4, time);
		time_to_dos(st.st_atime, &date, &time);
		WRITE_WORD(dest+0xe, date);
		WRITE_WORD(dest+0xc, time);
	} else {
		unsigned long long wtime;	  
		d_printf("LFN: using WIN date/time\n");
		wtime = unix_to_win_time(st.st_mtime);
		WRITE_DWORD(dest+0x14, wtime);
		WRITE_DWORD(dest+0x18, wtime >> 32);
		wtime = unix_to_win_time(st.st_ctime);
		WRITE_DWORD(dest+0x4, wtime);
		WRITE_DWORD(dest+0x8, wtime >> 32);
		wtime = unix_to_win_time(st.st_atime);
		WRITE_DWORD(dest+0xc, wtime);
		WRITE_DWORD(dest+0x10, wtime >> 32);
	}
	MEMCPY_2DOS(dest + 0x2c, name_lfn, strlen(name_lfn)+1);
	WRITE_BYTE(dest + 0x130, 0);
	strupperDOS(name_8_3);
	if (strcmp(name_8_3, name_lfn) != 0) {
		MEMCPY_2DOS(dest + 0x130, name_8_3, strlen(name_8_3)+1);
	}
	return 1;
}

static void call_dos_helper(int ah)
{
	unsigned char *ssp = SEG2LINEAR(_SS);
	unsigned long sp = (unsigned long) _SP;
	_AH = ah;
	pushw(ssp, sp, _CS);
	pushw(ssp, sp, _IP);
	_SP -= 4;
	_CS = LFN_HELPER_SEG;
	_IP = LFN_HELPER_OFF;
}


static int wildcard_delete(char *fpath, int drive)
{
	struct mfs_dir *dir;
	struct mfs_dirent *de;
	struct stat st;
	unsigned dirattr = _CX;
	char *pattern, *slash;
	int errcode;

	slash = strrchr(fpath, '/');
	d_printf("LFN: posix:%s\n", fpath);
	if (slash - 2 > fpath && slash[-2] == '/' && slash[-1] == '.')
		slash -= 2;
	*slash = '\0';
	if (slash == fpath)
		strcpy(fpath, "/");
	/* XXX check for device (special dir entry) */
	if (!find_file(fpath, &st, drive) || is_dos_device(fpath)) {
		Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
		return lfn_error(PATH_NOT_FOUND);
	}
	slash = fpath + strlen(fpath);
	dir = dos_opendir(fpath);
	if (dir == NULL) {
		Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
		free(dir);
		return lfn_error(PATH_NOT_FOUND);
	}
	pattern = malloc(strlen(slash + 1) + 1);
	name_ufs_to_dos(pattern, slash + 1);
	strupperDOS(pattern);

	d_printf("LFN: wildcard delete %s %s %x\n", pattern, fpath, dirattr);
	errcode = FILE_NOT_FOUND;
	while ((de = dos_readdir(dir))) {
		char name_8_3[PATH_MAX];
		char name_lfn[PATH_MAX];
		if (lfn_sfn_match(pattern, de, name_lfn, name_8_3) == 0) {
			*slash = '/';
			strcpy(slash + 1, de->d_long_name);

			d_printf("LFN: wildcard delete %s\n", fpath);
			stat(fpath, &st);
			/* don't remove directories */
			if (st.st_mode & S_IFDIR)
				continue;

			if ((dirattr >> 8) & DIRECTORY)
				continue;

			if (access(fpath, W_OK) == -1) {
				errcode = EACCES;
			} else {
				errcode = unlink(fpath) ? errno : 0;
			}
			if (errcode != 0) {
				Debug0((dbg_fd, "Delete failed(%s) %s\n",
					strerror(errcode), fpath));
				free(pattern);
				dos_closedir(dir);
				if (errcode == EACCES) {
					return lfn_error(ACCESS_DENIED);
				} else {
					return lfn_error(FILE_NOT_FOUND);
				}
			}
			Debug0((dbg_fd, "Deleted %s\n", fpath));
		}
	}
	free(pattern);
	dos_closedir(dir);
	if (errcode)
		return lfn_error(errcode);	
	return 1;
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

static int mfs_lfn_(void)
{
	char *cwd;
	char fpath[PATH_MAX];
	char fpath2[PATH_MAX];
	
	int drive, dirhandle = 0, rc;
	unsigned int dest = SEGOFF2LINEAR(_ES, _DI);
	char *src = (char *)SEGOFF2LINEAR(_DS, _DX);
	struct stat st;
	struct utimbuf utimbuf;
	size_t size;
	struct mfs_dirent *de;
	char *slash;
	struct lfndir *dir = NULL;
	
	d_printf("LFN: doing LFN!, AX=%x DL=%x\n", _AX, _DL);
	NOCARRY;
	switch (_AL) {
	case 0x0D: /* reset drive, nothing to do */
		break;
	case 0x39: /* mkdir */
	case 0x3a: /* rmdir */
		d_printf("LFN: %sdir %s\n", _AL == 0x39 ? "mk" : "rm", src);
		drive = build_truename(fpath, src, 0);
		if (drive < 0)
			return drive + 2;
		rc = (_AL == 0x39 ? dos_mkdir : dos_rmdir)(fpath, drive, 1);
		if (rc)
			return lfn_error(rc);
		break;
	case 0x3b: /* chdir */
	{
		char *d = LFN_string - (long)bios_f000 + (BIOSSEG << 4);
		Debug0((dbg_fd, "set directory to: %s\n", src));
		d_printf("LFN: chdir %s %zd\n", src, strlen(src));
		drive = build_posix_path(fpath, src, 0);
		if (drive < 0)
			return drive + 2;
		if (!find_file(fpath, &st, drive) || !S_ISDIR(st.st_mode))
			return lfn_error(PATH_NOT_FOUND);
		make_unmake_dos_mangled_path(d, fpath, drive, 1);
		d_printf("LFN: New CWD will be %s\n", d);
		call_dos_helper(0x3b);
		break;
	}
	case 0x41: /* remove file */
		drive = build_posix_path(fpath, src, _SI);
		if (drive < 0)
			return drive + 2;
		if (drives[drive].read_only)
			return lfn_error(ACCESS_DENIED);
		if (is_dos_device(fpath))
			return lfn_error(FILE_NOT_FOUND);
		if (_SI == 1)
			return wildcard_delete(fpath, drive);
		if (!find_file(fpath, &st, drive))
			return lfn_error(FILE_NOT_FOUND);
		d_printf("LFN: deleting %s\n", fpath);
		if (unlink(fpath) != 0)
			return lfn_error(FILE_NOT_FOUND);
		break;
	case 0x43: /* get/set file attributes */
		d_printf("LFN: attribute %s %d\n", src, _BL);
		drive = build_posix_path(fpath, src, 0);
		if (drive < 0)
			return drive + 2;
		if (drives[drive].read_only && (_BL < 8) && (_BL & 1))
			return lfn_error(ACCESS_DENIED);
		if (!find_file(fpath, &st, drive) || is_dos_device(fpath)) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			return lfn_error(FILE_NOT_FOUND);
		}
		utimbuf.actime = st.st_atime;
		utimbuf.modtime = st.st_mtime;
		switch (_BL) {
		case 0: /* retrieve attributes */
			_CX = get_dos_attr(fpath, st.st_mode,is_hidden(fpath));
			break;
		case 1: /* set attributes */
			if (set_dos_attr(fpath, st.st_mode, _CX) != 0)
				return lfn_error(ACCESS_DENIED);
			break;
		case 2: /* get physical size of uncompressed file */
			_DX = st.st_size >> 16;
			_AX = st.st_size & 0xffff;
			break; 
		case 3: /* set last write date/time */
			utimbuf.modtime = time_to_unix(_DI, _CX);
			if (dos_utime(fpath, &utimbuf) != 0)
				return lfn_error(ACCESS_DENIED);
			break;
		case 4: /* get last write date/time */
			time_to_dos(st.st_mtime, &_DI, &_CX);
			break;
		case 5: /* set last access date */
			utimbuf.actime = time_to_unix(_DI, _CX);
			if (dos_utime(fpath, &utimbuf) != 0)
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
			drive = sda_cur_drive(sda);
		else
			drive = _DL - 1;
		if (drive < 0 || drive >= MAX_DRIVE)
			return lfn_error(DISK_DRIVE_INVALID);
		if (!drives[drive].root)
			return 0;
			
		cwd = cds_current_path(drive_cds(drive));
		dest = SEGOFF2LINEAR(_DS, _SI);
		build_ufs_path(fpath, cwd, drive);
		d_printf("LFN: getcwd %s %s\n", cwd, fpath);
		find_file(fpath, &st, drive);
		d_printf("LFN: getcwd %s %s\n", cwd, fpath);
		d_printf("LFN: %p %d %#x %s\n", drive_cds(drive), drive, dest,
			 fpath+drives[drive].root_len);
		make_unmake_dos_mangled_path(fpath2, fpath, drive, 0);
		MEMCPY_2DOS(dest, fpath2 + 3, strlen(fpath2 + 3) + 1);
		break;
	case 0x4e: /* find first */
	{
		drive = build_posix_path(fpath, src, 1);
		if (drive < 0)
			return drive + 2;
		slash = strrchr(fpath, '/');
		d_printf("LFN: posix:%s\n", fpath);
		*slash++ = '\0';
		/* note: DJGPP doesn't like "0" as a directory handle */
		for (dirhandle = 1; dirhandle < MAX_OPEN_DIRS; dirhandle++)
			if (lfndirs[dirhandle] == NULL)
				break;
		if (dirhandle == MAX_OPEN_DIRS) {
			d_printf("LFN: too many dirs open\n");
			return lfn_error(NO_MORE_FILES);
		}
		dir = malloc(sizeof *dir);
		if (slash == fpath + 1)
			strcpy(dir->dirbase, "/");
		else
			strcpy(dir->dirbase, fpath);
		dir->dirattr = _CX;
		dir->drive = drive;
		dir->psp = sda_cur_psp(sda);
		name_ufs_to_dos(dir->pattern, slash);
		strupperDOS(dir->pattern);
		if (((_CX & (VOLUME_LABEL|DIRECTORY)) == VOLUME_LABEL) &&
		    (strcmp(slash, "*.*") == 0 || strcmp(slash, "*") ==  0)) {
			char lfn[260];
			dest = SEGOFF2LINEAR(_ES, _DI);
			MEMSET_DOS(dest, 0, 0x20);
			WRITE_BYTE(dest, VOLUME_LABEL);
			get_volume_label(NULL, NULL, lfn, drive);
			MEMCPY_2DOS(dest + 0x2c, lfn, strlen(lfn) + 1);
			WRITE_BYTE(dest + 0x130, 0);
			d_printf("LFN: get volume label: %s\n", lfn);
			lfndirs[dirhandle] = dir;
			dir->dir = NULL;
			_AX = dirhandle;
			_CX = 0;
			return 1;
		}

		/* XXX check for device (special dir entry) */
		if (!find_file(dir->dirbase, &st, drive) || is_dos_device(fpath)) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			free(dir);
			return lfn_error(NO_MORE_FILES);
		}
		dir->dir = dos_opendir(dir->dirbase);
		if (dir->dir == NULL) {
			Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
			free(dir);
			return lfn_error(NO_MORE_FILES);
		}
		d_printf("LFN: findfirst %s %s %s %x\n", slash, src, dir->dirbase, _CX);
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
			return 0;
		if (dir->dir == NULL)
			lfn_error(NO_MORE_FILES);
		do {
			de = dos_readdir(dir->dir);
			if (de == NULL) {
				dos_closedir(dir->dir);
				dir->dir = NULL;
				if (_AL == 0x4e) close_dirhandle(dirhandle);
				return lfn_error(NO_MORE_FILES);
			}
			d_printf("LFN: findnext %s %x\n", de->d_long_name, dir->dirattr);
		} while (!getfindnext(de, dir));
		if (_AL != 0x4e)
			_AX = 0x4f00 + dirhandle;
		else
			_AX = dirhandle;
		_CX = 0;
		break;
	}
	case 0x56: /* rename file */
	{
		int drive2, rc;
		const char *d = (const char *)SEGOFF2LINEAR(_ES, _DI);
		d_printf("LFN: rename to %s\n", d);
		drive = build_truename(fpath2, d, 0);
		if (drive < 0)
			return drive + 2;
		d_printf("LFN: rename from %s\n", src);
		drive2 = build_truename(fpath, src, 0);
		if (drive2 < 0)
			return drive2 + 2;
		if (drive != drive2)
			return lfn_error(NOT_SAME_DEV);
		rc = dos_rename(fpath, fpath2, drive, 1);
		if (rc)
			return lfn_error(rc);
		break;
	}
	case 0x60: /* truename */
	{
		int i;
		char filename[PATH_MAX];
	  
		src = (char *)SEGOFF2LINEAR(_DS, _SI);
		d_printf("LFN: truename %s, cl=%d\n", src, _CL);
		i = 0;
		if (src[0] && src[1] == ':') i = 2;
		for (; src[i]; i++) {
			if (!VALID_DOS_PCHAR(src + i) &&
			    strchr("\\/.",src[i]) == 0 &&
			    (_CL == 2 || strchr(" +,;=[]",src[i])==0)) {
				return lfn_error(FILE_NOT_FOUND);
			}
		}
		drive = build_truename(filename, src, !_CL);
		if (drive < 0)
			return drive + 2;

		d_printf("LFN: %s %s\n", fpath, drives[drive].root);

		if (_CL == 1 || _CL == 2) {
			build_ufs_path(fpath, filename, drive);
			find_file(fpath, &st, drive);
			make_unmake_dos_mangled_path(filename, fpath, drive, 2 - _CL);
		} else {
			strupperDOS(filename);
		}
		d_printf("LFN: %s %s\n", filename, drives[drive].root);
		MEMCPY_2DOS(dest, filename, strlen(filename) + 1);
		break;
	}
	case 0x6c: /* create/open */
	{
		char *d = LFN_string - (long)bios_f000 + (BIOSSEG << 4);
		src = (char *)SEGOFF2LINEAR(_DS, _SI);
		d_printf("LFN: open %s\n", src);
		drive = build_posix_path(fpath, src, 0);
		if (drive < 0)
			return drive + 2;
		if (is_dos_device(fpath)) {
			strcpy(d, strrchr(fpath, '/') + 1);
		} else {
			slash = strrchr(fpath, '/');
			strcpy(fpath2, slash);
			*slash = '\0';
			if (slash != fpath && !find_file(fpath, &st, drive))
				return lfn_error(PATH_NOT_FOUND);
			strcat(fpath, fpath2);
			if (!find_file(fpath, &st, drive) && (_DX & 0x10)) {
				int fd;
				if (drives[drive].read_only)
					return lfn_error(ACCESS_DENIED);
				fd = open(fpath, (O_RDWR | O_CREAT),
					  get_unix_attr(0664, _CL |
							ARCHIVE_NEEDED));
				if (fd < 0) {
					d_printf("LFN: creat problem: %o %s %s\n",
						 get_unix_attr(0644, _CX),
						 fpath, strerror(errno));
					return lfn_error(ACCESS_DENIED);
				}
				set_fat_attr(fd, _CL | ARCHIVE_NEEDED);
				d_printf("LFN: open: created %s\n", fpath);
				close(fd);
				_AL = 1; /* flags creation to DOS helper */
			} else {
				_AL = 0;
			}
			make_unmake_dos_mangled_path(d, fpath, drive, 1);
		}
		call_dos_helper(0x6c);
		break;
	}
	case 0xa0: /* get volume info */
		drive = build_posix_path(fpath, src, 0);
		if (drive < 0)
			return drive + 2;
		size = _CX;
		_AX = 0;
		_BX = 0x4002;
		_CX = 255;
		_DX = 260;
		if (size >= 4)
			MEMCPY_2DOS(dest, "MFS", 4);
		break;
	case 0xa1: /* findclose */
		d_printf("LFN: findclose %x\n", _BX);
		return close_dirhandle(_BX);
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
			unsigned long long wtime;
			wtime = unix_to_win_time(time_to_unix(_DX, _CX));
			WRITE_DWORD(dest, wtime);
			WRITE_DWORD(dest + 4, wtime >> 32);
		}
		break;
	case 0xa8: /* generate short filename */
	{
		src = (char *)SEGOFF2LINEAR(_DS, _SI);
		StrnCpy(fpath, src, sizeof(fpath) - 1);
		name_convert(fpath, MANGLE);
		strupperDOS(fpath);
		if (_DH == 0) {
			char d[8+3];
			extract_filename(fpath, d, d + 8);
			MEMCPY_2DOS(dest, fpath, 11);
		} else
			MEMCPY_2DOS(dest, fpath, strlen(fpath) + 1);
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

int mfs_lfn(void)
{
	int carry, ret;

	carry = isset_CF();
	ret = mfs_lfn_();
	/* preserve carry if we forward the LFN request */
	if (ret == 0 && carry)
		CARRY;
	return ret;
}
