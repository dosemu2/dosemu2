/*
 *  linux/fs/msdos/dir.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  MS-DOS directory handling functions
 *
 *  -----------------------------------------------------------------------
 *  Patched version for dosemu by Hans Lermen <lermen@elserv.ffm.fgan.de>
 *
 *  Well, I would have preferred not to patch this.
 *  However, starting with Linux-1.3.4 there will be no HIDDEN or SYSTEM
 *  attributed filenames delivered. This of course makes it dosemu impossible
 *  to show the DOS-side what it expects to see.
 *
 *  So, we remove this '|ATTR_SYS|ATTR_HIDDEN' and can live happy again...  
 */

#include "kversion.h"
#if 0
#define KERNEL_VERSION 1003036 /* last verified kernel version */
#endif

#if (KERNEL_VERSION  >= 1003004) && defined(REPAIR_ODD_MSDOS_FS)

#if 0
#ifdef MODULE
#include <linux/module.h>
#endif
#endif

#include <asm/segment.h>

#include <linux/fs.h>
#include <linux/msdos_fs.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/string.h>

#include "msbuffer.h"


#define PRINTK(X)

static int msdos_dir_read(struct inode * inode,struct file * filp, char * buf,int count)
{
	return -EISDIR;
}

struct file_operations _TRANSIENT_msdos_dir_operations = {
	NULL,			/* lseek - default */
	msdos_dir_read,		/* read */
	NULL,			/* write - bad */
	msdos_readdir,		/* readdir */
	NULL,			/* select - default */
	NULL,			/* ioctl - default */
	NULL,			/* mmap */
	NULL,			/* no special open code */
	NULL,			/* no special release code */
	file_fsync		/* fsync */
};

#if 0
struct inode_operations msdos_dir_inode_operations = {
	&msdos_dir_operations,	/* default directory file-ops */
	msdos_create,		/* create */
	msdos_lookup,		/* lookup */
	NULL,			/* link */
	msdos_unlink,		/* unlink */
	NULL,			/* symlink */
	msdos_mkdir,		/* mkdir */
	msdos_rmdir,		/* rmdir */
	NULL,			/* mknod */
	msdos_rename,		/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	msdos_bmap,		/* bmap */
	NULL,			/* truncate */
	NULL			/* permission */
};
#endif

int msdos_readdir(
	struct inode *inode,
	struct file *filp,
	void *dirent,
	filldir_t filldir)
{
	struct super_block *sb = inode->i_sb;
	int ino,i,i2,last;
	char c;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;
	unsigned long oldpos = filp->f_pos;

	if (!inode || !S_ISDIR(inode->i_mode))
		return -EBADF;
/* Fake . and .. for the root directory. */
	if (inode->i_ino == MSDOS_ROOT_INO) {
		while (oldpos < 2) {
			if (filldir(dirent, "..", oldpos+1, oldpos, MSDOS_ROOT_INO) < 0)
				return 0;
			oldpos++;
			filp->f_pos++;
		}
		if (oldpos == 2)
			filp->f_pos = 0;
	}
	if (filp->f_pos & (sizeof(struct msdos_dir_entry)-1))
		return -ENOENT;
	bh = NULL;
	while ((ino = msdos_get_entry(inode,&filp->f_pos,&bh,&de)) > -1) {
		if (!IS_FREE(de->name)
#if 0 /* this is what makes us pain */
			&& !(de->attr & (ATTR_VOLUME|ATTR_SYS|ATTR_HIDDEN))) {
#else
			&& !(de->attr & ATTR_VOLUME)) {
#endif
			char bufname[12];
			char *ptname = bufname;
			for (i = last = 0; i < 8; i++) {
				if (!(c = de->name[i])) break;
				if (c >= 'A' && c <= 'Z') c += 32;
				if (c != ' ')
					last = i+1;
				ptname[i] = c;
			}
			i = last;
			ptname[i] = '.';
			i++;
			for (i2 = 0; i2 < 3; i2++) {
				if (!(c = de->ext[i2])) break;
				if (c >= 'A' && c <= 'Z') c += 32;
				if (c != ' ')
					last = i+1;
				ptname[i] = c;
				i++;
			}
			if ((i = last) != 0) {
				if (!strcmp(de->name,MSDOS_DOT))
					ino = inode->i_ino;
				else if (!strcmp(de->name,MSDOS_DOTDOT))
					ino = msdos_parent_ino(inode,0);
				if (filldir(dirent, bufname, i, oldpos, ino) < 0) {
					filp->f_pos = oldpos;
					break;
				}
			}
		}
		oldpos = filp->f_pos;
	}
	if (bh) brelse(bh);
	return 0;
}

#endif /* KERNEL_VERSION >= 1003004 */
