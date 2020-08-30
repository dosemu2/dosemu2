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
#if BPRM_VER != 3
#error wrong bprm version
#endif
#include "boot.h"

int fdpp_boot(far_t plt)
{
    int i;
    struct _bprm bprm = {};
    uint16_t bprm_seg = 0x1fe0 + 0x7c0 + 0x20;  // stack+bs
    uint16_t seg = 0x0060;
    uint16_t ofs = 0x0000;
    dosaddr_t loadaddress = SEGOFF2LINEAR(seg, ofs);
    uint16_t env_seg = bprm_seg + 8;
    char *env = SEG2UNIX(env_seg);
    int env_len = 0;
    int warn_legacy_conf = 0;

    bprm.PltSeg = plt.segment;
    bprm.PltOff = plt.offset;
    bprm.InitEnvSeg = env_seg;
    LWORD(eax) = bprm_seg;
    HI(bx) = BPRM_VER;

    bprm.DriveMask = config.drives_mask;

    if (config.drive_c_num) {
	LO(bx) = config.drive_c_num;
	env_len += sprintf(env + env_len, "USERDRV=%c",
		(config.drive_c_num & 0x7f) + 'C');
	env_len++;
    } else {
	LO(bx) = 0x80;
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

    env_len += sprintf(env + env_len, "#0 :SWITCHES=/F%s",
	    config.dos_trace ? "/Y" : "");
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

    env[env_len++] = '\0'; // second terminator
    env[env_len++] = '\0'; // third terminator (can be \1 for cmdline)
    MEMCPY_2DOS(SEGOFF2LINEAR(bprm_seg, 0), &bprm, sizeof(bprm));

    SREG(ds)  = loadaddress >> 4;
    SREG(es)  = loadaddress >> 4;
    SREG(ss)  = 0x1FE0;
    LWORD(esp) = 0x7c00;  /* temp stack */
    LWORD(ebp) = 0x7C00;
    SREG(cs)  = seg;
    LWORD(eip) = ofs;

    int_try_disable_revect();
    /* try disable int hooks as well */
    if (config.int_hooks == -1)
	config.int_hooks = config.force_revect;

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
