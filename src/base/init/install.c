/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include "emu.h"
#include "dosemu_config.h"
#include "utilities.h"
#include "dos2linux.h"

int unix_e_welcome;

void show_welcome_screen(void)
{
	/* called from unix -e */
	do_liability_disclaimer_prompt(0);
	p_dos_str(
"Continue to confirm, or enter EXITEMU or press Ctrl-Alt-PgDn to exit DOSEMU.\n\n");	p_dos_str(
	"Your DOS drives are set up as follows:\n"
	" A: floppy drive (if it exists)\n"
	" C: points to the Linux directory ~/.dosemu/drive_c\n"
	" D: points to the read-only DOSEMU commands directory\n"
	" E: points to FreeDOS boot files\n"
	" F: points to FreeDOS installation\n"
	" G: points to your CD-ROM drive, if it is mounted at /media/cdrom\n"
	"Use the lredir2 DOSEMU command to adjust these settings, or edit\n"
        "/etc/dosemu/dosemu.conf, ~/.dosemu/.dosemurc\n\n"
	"To re-install a DOS, exit and then restart DOSEMU using dosemu -i.\n"
	"Enter HELP for more information on DOS and DOSEMU commands.\n");
	p_dos_str(
"Keys: Ctrl-Alt-... F=fullscreen, K=grab keyboard, Home=grab mouse, Del=reboot\n"
"      Ctrl-Alt-PgDn=exit; Ctrl-^: use special keys on terminals\n");
}

static int create_drive_c(char *boot_dir_path)
{
	char *system_str, *sys_path;
	int ret;

	ret = asprintf(&system_str, "mkdir -p %s/tmp", boot_dir_path);
	assert(ret != -1);

	if (system(system_str)) {
		error("  unable to create $BOOT_DIR_PATH, giving up\n");
		free(system_str);
		return 0;
	}
	free(system_str);

	sys_path = assemble_path(dosemu_lib_dir_path, CMDS_SUFF);
	ret = asprintf(&system_str,
			"eval ln -s %s/fdconfig.sys "
			"\"%s\"",
			sys_path, boot_dir_path);
	free(sys_path);
	assert(ret != -1);
	if (system(system_str)) {
		error("Error: unable to symlink fdconfig.sys\n");
		free(system_str);
		return 0;
	}
	free(system_str);
	/* symlink command.com in case someone hits Shift or F5 */
	ret = asprintf(&system_str,
			"eval ln -s %s/command.com "
			"\"%s\"",
			fddir_default, boot_dir_path);
	assert(ret != -1);
	if (system(system_str)) {
		error("Error: unable to symlink command.com\n");
		free(system_str);
		return 0;
	}
	free(system_str);
	return 1;
}

void install_dos(void)
{
	int first_time;
	char *dir_name =
		assemble_path(dosemu_image_dir_path, "drive_c");

	first_time = !exists_dir(dir_name);
	if (!config.install && !first_time && config.hdisks)
		goto exit_free;
	if (!config.default_drives)
		goto exit_free;
	if (config.hdiskboot != -1) {
		error("$_bootdrive is altered, not doing install\n");
		goto exit_free;
	}
	if (config.emusys) {
		error("$_emusys must be disabled before installing DOS\n");
//		leavedos(24);
	}

	if (first_time) {
		mkdir(dir_name, 0777);
		create_drive_c(dir_name);
		unix_e_welcome = 1;
	}
	free(dir_name);
	return;

exit_free:
	free(dir_name);
}
