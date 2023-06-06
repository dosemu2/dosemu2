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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: fdpp boot protocol
 *
 * Author: Stas Sergeev
 */
#include <stdint.h>
#include "emu.h"
#include "int.h"
#include "disks.h"
#include <fdpp/bprm.h>
#if BPRM_MIN_VER != 10
#error wrong bprm version
#endif
/* currently supported version */
#define BPRM_SUPP_VER 10
#include "boot.h"

int fdpp_boot(far_t *plt, int plt_len, const void *krnl, int len,
        uint16_t seg, int khigh,
        uint16_t heap_seg, int heap, int hhigh, unsigned char *boot_sec,
        uint16_t bpseg)
{
    int i;
    struct _bprm bprm = {};
    struct _bprm_xtra *xtra = MK_FP32(bpseg, 0);
    uint16_t ofs = 0x0000;
    dosaddr_t loadaddress = SEGOFF2LINEAR(seg, ofs);
    uint16_t env_seg = FDPP_BS_SEG + (FDPP_BS_OFF >> 4) + 0x20;  // stack+bs
    char *env = SEG2UNIX(env_seg);
    int env_len = 0;
    int warn_legacy_conf = 0;

    bprm.BprmLen = sizeof(bprm);
    bprm.BprmVersion = BPRM_SUPP_VER;
    bprm.HeapSize = heap;
    bprm.HeapSeg = heap_seg;
    bprm.XtraSeg = bpseg;
#if BPRM_VER >= 9
    bprm.PredMask |= (!config.dumb_video << 0);
#endif
    if (khigh)
	bprm.Flags |= FDPP_FL_KERNEL_HIGH;
    if (hhigh)
	bprm.Flags |= FDPP_FL_HEAP_HIGH;
    if (hhigh == 2)
	bprm.Flags |= FDPP_FL_HEAP_HMA;
    bprm.InitEnvSeg = env_seg;

    bprm.DriveMask = config.drives_mask;

    if (config.drive_c_num) {
	LO(bx) = config.drive_c_num;
	env_len += sprintf(env + env_len, "USERDRV=%c",
		(config.drive_c_num & 0x7f) + 'C');
	env_len++;
	xtra->BootDrvNum = (config.drive_c_num & 0x7f) + 2;
    } else {
	LO(bx) = 0x80;
	xtra->BootDrvNum = 2;
    }
    FOR_EACH_HDISK(i, {
	if (disk_root_contains(&hdisktab[i], CONF4_IDX)) {
	    bprm.CfgDrive = hdisktab[i].drive_num + hdisktab[i].log_offs;
	    break;
	}
	if (HDISK_NUM(i) == 2 && disk_root_contains(&hdisktab[i], CONF_IDX))
	    warn_legacy_conf = 1;
    });

    FOR_EACH_HDISK(i, {
	if (disk_root_contains(&hdisktab[i], CMD_IDX)) {
	    char drv = HDISK_NUM(i) + 'A';
	    uint8_t drv_num = hdisktab[i].drive_num;
	    fatfs_t *f1 = get_fat_fs_by_drive(drv_num);
	    struct sys_dsc *sf1 = fatfs_get_sfiles(f1);

	    bprm.ShellDrive = drv_num + hdisktab[i].log_offs;
	    if (sf1[CMD_IDX].flags & FLG_COMCOM32)
		dbug_printf("booting with comcom32\n");
	    env_len += sprintf(env + env_len, "SHELLDRV=%c", drv +
		    hdisktab[i].log_offs);
	    env_len++;
	    break;
	}
    });
#if 0
    if (!bprm.ShellDrive)
	return -1;
#endif

    FOR_EACH_HDISK(i, {
	if (disk_root_contains(&hdisktab[i], DEMU_IDX)) {
	    char drv = HDISK_NUM(i) + 'A';
	    bprm.DeviceDrive = hdisktab[i].drive_num + hdisktab[i].log_offs;
	    env_len += sprintf(env + env_len, "DOSEMUDRV=%c", drv +
		    hdisktab[i].log_offs);
	    env_len++;
	    break;
	}
    });
    if (!bprm.DeviceDrive)
	return -1;

    FOR_EACH_HDISK(i, {
	if (disk_root_contains(&hdisktab[i], AUT2_IDX)) {
	    char drv = HDISK_NUM(i) + 'A';
	    uint8_t drv_num = hdisktab[i].drive_num;
	    fatfs_t *f1 = get_fat_fs_by_drive(drv_num);
	    struct sys_dsc *sf1 = fatfs_get_sfiles(f1);

	    env_len += sprintf(env + env_len, "FDPP_AUTOEXEC=%c:\\%s", drv +
		    hdisktab[i].log_offs, sf1[AUT2_IDX].name);
	    env_len++;
	    break;
	}
    });

    env_len += sprintf(env + env_len, "#0 :SWITCHES=/F/T%s",
	    config.dos_trace ? "/Y" : ""
    );
    env_len++;
    if (config.country) {
	env_len += sprintf(env + env_len, "#1 :COUNTRY=%i,%i",
		config.country, atoi(config.internal_cset + 2));
	env_len++;
    }
    env_len += sprintf(env + env_len, "#2 =command.com /e:%s /k "
	    "%%FDPP_AUTOEXEC%%",
	    config.dos_cmd ? "512" : "384 /p"
    );
    env_len++;

    if (fddir_default) {
	struct disk *dsk = hdisk_find_by_path(fddir_default);
	if (dsk) {
	    char drv = (dsk->drive_num & 0x7f) + 'C';
	    env_len += sprintf(env + env_len, "FREEDOSDRV=%c", drv +
		    dsk->log_offs);
	    env_len++;
	}
    }

    if (xbat_dir) {
	struct disk *dsk = hdisk_find_by_path(xbat_dir);
	if (dsk) {
	    char drv = (dsk->drive_num & 0x7f) + 'C';
	    env_len += sprintf(env + env_len, "XBATDRV=%c", drv +
		    dsk->log_offs);
	    env_len++;
	}
    }

    env[env_len++] = '\0'; // second terminator
    env[env_len++] = '\0'; // third terminator (can be \1 for cmdline)
    env[env_len++] = '\0'; // third terminator is a word, not byte

    memcpy(boot_sec + 0x03, "FDPP 1.6", 8);
    memcpy(boot_sec + FDPP_PLT_OFFSET, plt, sizeof(plt) * plt_len);
    *(uint16_t *)(boot_sec + FDPP_BPRM_VER_OFFSET) = BPRM_VER;
    memcpy(boot_sec + FDPP_BPRM_OFFSET, &bprm, sizeof(bprm));

    SREG(ds)  = loadaddress >> 4;
    SREG(es)  = loadaddress >> 4;
    SREG(ss)  = FDPP_BS_SEG;
    LWORD(esp) = FDPP_BS_OFF;  /* temp stack */
    LWORD(ebp) = FDPP_BS_OFF;
    SREG(cs)  = seg;
    LWORD(eip) = ofs;

    int_try_disable_revect();
    /* try disable int hooks as well */
    if (config.int_hooks == -1)
	config.int_hooks = config.force_revect;
    add_syscom_user(&xtra->BootDrvNum);

    if (warn_legacy_conf) {
	error("@Compatibility warning: CONFIG.SYS found on drive C: ");
	error("@is not used by fdpp.\n");
	error("@\tUse C:\\FDPPCONF.SYS to override or C:\\USERHOOK.SYS ");
	error("@to extend\n\tthe default boot-up config file.\n");
	error("@\tYou can also put KERNEL.SYS to drive C: ");
	error("@to override fdpp entirely.\n");
    }
    return 0;
}

int fdpp_boot_xtra_space(void)
{
    return sizeof(struct _bprm_xtra);
}
