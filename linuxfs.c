#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <time.h>
#include <sys/time.h>
#include "emu.h"

const char prefix[] = PHANTOMDIR;
char cwd[80] = PHANTOMDIR;
int dos_open_modes[3] = {O_RDONLY, O_WRONLY, O_RDWR};
char findpath[80];
char findname[13];
int dos_files[20] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; 

int dos2unix(char *uni, char *dos)
{
	char drive;

	if (strstr(dos, "Phantom.") != dos)
		return -1;
	dos += 8;
	drive = *dos++;
	if (*dos++ != ':') return -1;
	strcpy(uni, prefix);
	uni += strlen(prefix);
	for (;;) {
		switch (*dos) {
			case '\\':
				*uni++ = '/';
				break;
			case '\0':
				*uni = '\0';
				if (*--uni == '.') *uni = '\0';
				return 0;
			default :
				*uni++ = tolower(*dos);
		}
		dos++;
	}
}

void dos_time(time_t utime, short *time, short *date)
{
	struct tm *tp;

	tp = localtime(&utime);
	/*
	d_printf("%d.%d.%d   %d:%02d\n", tp->tm_mday, tp->tm_mon+1, tp->tm_year,
		tp->tm_hour, tp->tm_min);
	*/
	*time = (tp->tm_sec >>1) | (tp->tm_min <<5) | (tp->tm_hour <<11);
	*date = tp->tm_mday | ((tp->tm_mon +1) <<5) | ((tp->tm_year -80) <<9);
}
struct fentry {
	char fname[12];
	long fsize;
	short fdate, ftime;
	char fattr;
};

int dir_entry(int first, int s_mode, struct fentry *fptr)
{
	char name[80], *cp, *np;
	int c, mode;
	static DIR *dirp;
	struct dirent *dp;
	struct stat stbuf;

	d_printf("DIR %x, %s\n", s_mode, findname);

	if (s_mode == 0x08) return 18; /* no name */
	if (first) {
		if (dirp != NULL) (void)closedir(dirp);
		if ((dirp = opendir(findpath)) == NULL) return 3;
	}
	for (;;) {
		if ((dp = readdir(dirp)) == NULL) { /* no more entries */
			(void)closedir(dirp);
			dirp = NULL;
			return 18;
		}
		d_printf("dir: %s\n", dp->d_name);
		np = dp->d_name;
		cp = findname;
		while (*cp) {
			if (*cp == '?') {
 				if (*np != '\0' && *np != '.') np++;
			} else if (*cp == *np) np++;
			else if (*cp != '.' || *np != '\0') break;
			cp++;
		}
		if (*np || *cp) continue;
		if (*--np == '.') continue;
		strcpy(name, findpath);
		strcat(name, "/");
		strcat(name, dp->d_name);
		d_printf("matched %s\n", name);
		if (stat(name, &stbuf) < 0) continue;
		/* matched */
		if ((s_mode & 0x10) == 0 && S_ISDIR(stbuf.st_mode)) continue;
		break;
	}
	mode = 0;
	if (S_ISDIR(stbuf.st_mode)) mode = 0x10;
	np = dp->d_name;
	if (fptr) {
		cp = fptr->fname;
		memset(cp, ' ', 12);
		cp[11] = '\0';
		for (c=0; c<11; c++) {
			if (*np == '\0') break;
			if (*np == '.' && c < 9) c = 7, np++;
			else cp[c] = toupper(*np++);
		}
		fptr->fsize = stbuf.st_size;
		dos_time(stbuf.st_mtime, &fptr->ftime, &fptr->fdate);
		fptr->fattr = mode;
	}
	return 0; /* OK */
}

int delete(char *delpath)
{
	char name[80], *cp, *np;
	char delname[13];
	int c = 2; /* file not found */
	DIR *dirp;
	struct dirent *dp;
	struct stat stbuf;


	cp = delpath + strlen(delpath);
	while (*--cp != '/');
	*cp++ = '\0';
	strcpy(delname, cp);
	d_printf("DEL <%s><%s>\n", delpath, delname);
	if ((dirp = opendir(delpath)) == NULL) return 3;
	while ((dp = readdir(dirp)) != NULL) { 
		/* d_printf("dir: %s\n", dp->d_name); */
		np = dp->d_name;
		cp = delname;
		while (*cp) {
			if (*cp == '?') {
 				if (*np != '\0' && *np != '.') np++;
			} else if (*cp == *np) np++;
			else if (*cp != '.' || *np != '\0') break;
			cp++;
		}
		if (*np || *cp) continue;
		if (*--np == '.') continue;
		strcpy(name, delpath);
		strcat(name, "/");
		strcat(name, dp->d_name);
		if (stat(name, &stbuf) < 0) {
			c = 5;
			break;
		}
		if (!S_ISREG(stbuf.st_mode)) continue;
		/* matched */
		d_printf("deleting %s\n", name);
		if (unlink(name) == 0) c = 0;
		else {
			c = 5;
			break;
		}
	}
	(void)closedir(dirp);
	return c;
}

int ext_fs(int nr, char *p1, char *p2, int c)
{
	char name[80], name2[80], *cp;
	struct stat stbuf;
	struct statfs fsbuf;
	int r, m, a, f;

	d_printf("EXT FS:\n");
	switch(nr) {
		case 1: /* CD */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("CD %s --- %s\n", p1, name);
			if (stat(name, &stbuf) < 0 || !S_ISDIR(stbuf.st_mode))
				return 3;
			strcpy(cwd, name);
			return 0;
		case 2: /* MD */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("MD %s --- %s\n", p1, name);
			if (mkdir(name, 0755) < 0) {
				return (errno == EPERM) ? 5 : 3;
			}
			return 0;
		case 3: /* RD */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("RD %s --- %s\n", p1, name);
			if (stat(name, &stbuf) < 0 || !S_ISDIR(stbuf.st_mode)) {
				return 3;
			}
			d_printf("stat\n");
			if (rmdir(name) < 0) {
				return (errno == EBUSY) ? 16 : 5;
				break;
			}
			return 0;
		case 4: /* GET FIRST */
			if (dos2unix(findpath, p1)) return 0xf;
			cp = findpath + strlen(findpath);
			while (*--cp != '/');
			*cp++ = '\0';
			strcpy(findname, cp);
			d_printf("GET FIRST %s  %d --- %s, %s (%x)\n", p1, c, findpath, findname, p2-(char *)0);
			return dir_entry(1, c, (struct fentry *)p2);
		case 5: /* GET NEXT */
			d_printf("GET NEXT  %d --- %s, %s\n", c, findpath, findname);
			return dir_entry(0, c, (struct fentry *)p2);
		case 6: /* OPEN */
		case 7: /* CREAT */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("%s %d --- %s, %s\n", (nr==7) ? "CREAT" : "OPEN",
				c, p1, name);
			for (f=19; f>= 0; f--) if (dos_files[f] < 0) break;
			if (f < 0) return 4;
			m = c & 0xff;
			if (m > 2) return 0xc;
			dos_files[f] = open(name, dos_open_modes[m] | 
					((nr==7) ? O_CREAT | O_TRUNC : 0), 0644);
			if (dos_files[f] < 0) return 2;
			if (fstat(dos_files[f], &stbuf) < 0) return 2;
			if (S_ISDIR(stbuf.st_mode)) return 5;
			if (nr == 6) {
				d_printf("SZ=%d\n", stbuf.st_size);
				*((int *)p2)++ = stbuf.st_size;
			} 
			*(short *)p2 = (short)f;
			d_printf("__%d\n", f);
			return 0;
		case 8: /* SPECIAL OPEN */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("SPECIAL OPEN %x --- %s, %s\n", c, p1, name);
			r = (stat(name, &stbuf) == 0 && S_ISREG(stbuf.st_mode));
			d_printf("r= %d\n", r);
			if (((c & 0xf00) == 0 && r) || ((c & 0xf000) == 0 && !r))
				return 5;
			m = c & 0x3;
			if (m > 2) return 0xc;
			for (f=19; f>= 0; f--) if (dos_files[f] < 0) break;
			if (f < 0) return 4;
			if (r) a = (c & 0x200) ? O_TRUNC : 0;
			else a = O_CREAT;
			a |= dos_open_modes[m];
			dos_files[f] = open(name, a, 0644);
			if (dos_files[f] < 0) return 5;
			if (fstat(dos_files[f], &stbuf) < 0) return 2;
			if (S_ISDIR(stbuf.st_mode)) return 5;
			r = (a & (O_CREAT | O_TRUNC)) ? -1 : stbuf.st_size;
			*((int *)p2) = r;
			*(short *)(p2 + 4) = (short)f;
			d_printf("__%d, SZ=%d\n", f, r);
			return 0;
		case 9: /* CLOSE */
			d_printf("CLOSE (%d) \n", c);
			if (c >= 0 && c < 20)
				dos_files[c] = -1;
			return 0;
		case 10: /* READ */
		case 11: /* WRITE */
			d_printf("%s (%d) sz=%d\n", (nr == 10) ? "READ" : "WRITE",
						 c, *(unsigned short *)p2);
			d_printf("%04x:%04x\n", _regs.fs, _regs.edi);
			if (c < 0 || c >= 20 || dos_files[c] < 0) r = 0;
			else {
				a = *(int *)(p2+2);
				d_printf("... %d - (%d)  ", a, lseek(dos_files[c], 0, SEEK_CUR));
				if (a != lseek(dos_files[c], a, SEEK_SET))
					d_printf("SEEK OUT OF FILE\n");
				if (nr == 10)
					r = read(dos_files[c], p1, *(unsigned short *)p2);
				else
					r = write(dos_files[c], p1, *(unsigned short *)p2);
				d_printf("res:%d\n", r);
				if (r < 0) r = 0;
			}
			*(unsigned short *)p2 = (unsigned short)r;
			return 0;
		case 12: /* SEEK */
			d_printf("SEEK (%d) %d\n", c, *(int *)p1);
			if (c < 0 || c >= 20 || dos_files[c] < 0) return 6;
			r = lseek(dos_files[c], *(off_t *)p1, SEEK_END);
			d_printf("pos = %d\n", r);
			if (r < 0) return 5;
			*(int *)p1 = r;
			return 0;
		case 13: /* DELETE */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("DELETE %s --- %s\n", p1, name);
			return delete(name);
		case 14: /* RENAME */
			if (dos2unix(name, p1)) return 0xf;
			if (dos2unix(name2, p2)) return 0xf;
			d_printf("RENAME %s --- %s TO %s\n", p1, name, p2);
			if (rename(name, name2) < 0)
				return 5;
			return 0;
		case 15: /* SPACE */
			d_printf("SPACE %s\n", cwd);
			if (statfs(cwd, &fsbuf) >= 0) {
				d_printf("f_bsize %d, f_blocks %d, f_bfree %d, f_bavail %d\n",
				fsbuf.f_bsize, fsbuf.f_blocks, fsbuf.f_bfree, fsbuf.f_bavail);
				r = fsbuf.f_bsize * fsbuf.f_bavail / 1024;
				if (r > 0xffff) r = 0xffff;
			} else r = 0;
			*(unsigned short *)p1 = r;
			return 0;
		case 16: /* SETATT */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("SETATT %s --- %s TO %x\n", p1, name, c);
			return 0;
		case 17: /* GETATT */
			if (dos2unix(name, p1)) return 0xf;
			d_printf("GETATT %s --- %s\n", p1, name);
			if (stat(name, &stbuf) < 0)
				return 2;
			if (S_ISDIR(stbuf.st_mode)) m = 0x10; else m = 0;
			d_printf(">>%x\n", m);
			*(unsigned short *)p2 = m;
			return 0;
		default:
			d_printf("FN_%d not implemented\n", nr);
			show_regs();
			error = 1;
	}
	return -1;
}
