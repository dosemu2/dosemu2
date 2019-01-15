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
#include "disks.h"
#include "utilities.h"
#include "dos2linux.h"

static int terminal_read(char *buf32, u_short size)
{
	char x[size+2];
	char *p = fgets(x, size+2, stdin);
	if (!p) {
		error("fgets() failed: %s\n", strerror(errno));
		return -1;
	}
	size = strlen(x);
	if (size && x[size - 1] == '\n') {
		size--;
		x[size] = '\0';
	}
	if (size)
		memcpy(buf32, x, size);
	return size;
}

static int (*printf_)(const char *, ...) FORMAT(printf, 1, 2) = printf;
static int (*read_string)(char *, u_short) = terminal_read;
int unix_e_welcome;

static int bios_read_string(char *buf, u_short len)
{
    int ret;
    set_IF();
    ret = com_biosread(buf, len);
    clear_IF();
    return ret;
}

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

static int create_symlink_ex(const char *path, int number, int special,
	const char *path2)
{
	char *drives_c = assemble_path(dosemu_image_dir_path, "drives/c");
	char *slashpos = drives_c + strlen(drives_c) - 2;
	static char symlink_txt[] =
		"Creating symbolic link for %s as %s\n";
	int err;

	slashpos[1] += number; /* make it 'd' if 'c' */
	printf_(symlink_txt, path, drives_c);
	if (special) {
		char *cmd;
		char *dst;

		asprintf(&dst, "%s.lnk", drives_c);
		err = (access(drives_c, F_OK) == 0 || access(dst, F_OK) == 0);
		if (err) {
			error("%s exists, not doing install\n", drives_c);
			free(dst);
			free(drives_c);
			return 0;
		}
		asprintf(&cmd, "echo -n '%s' >%s", path, dst);
		free(dst);
		system(cmd);
		free(cmd);
		if (path2) {
			free(drives_c);
			drives_c = expand_path(path2);
		}
	} else {
		err = symlink(path, drives_c);
		if (err) {
			error("%s exists, not doing install\n", drives_c);
			free(drives_c);
			return 0;
		}
	}
	if (config.hdisks <= number) {
		config.hdisks = number + 1;
		hdisktab[number].fdesc = -1;
	}
	free(hdisktab[number].dev_name);
	/* point C:/D: to $HOME/.dosemu/drives/c or d */
	hdisktab[number].dev_name = drives_c;
	hdisktab[number].type = DIR_TYPE;
	hdisktab[number].sectors = -1; 		// ask for re-setup
	return 1;
}

static int create_symlink(const char *path, int number)
{
	return create_symlink_ex(path, number, 0, NULL);
}

static char *dosreadline(void)
{
	char *line;
	int size;

	line = malloc(129);
	assert(line != NULL);

	size = read_string(line, 128);
	line[size] = '\0';
	return line;
}

static int install_dosemu_freedos(int choice)
{
	char *boot_dir_path, *system_str, *tmp, *sys_path;
	int ret;

	/*
	 * boot_dir_path = path to install to
	 * We copy the binaries from the directory dosemu_lib_dir
	 * to the users $HOME.
	 */
	if (choice == 3) {
		printf_("Please enter the name of the directory for your private "
			   "DOSEMU-FreeDOS files\n");

		boot_dir_path = dosreadline();
		if (boot_dir_path[0] == '/') {
			printf_("You gave an absolute path, "
				   "type ENTER to confirm or another path\n");

			tmp = dosreadline();
			if (tmp[0] == '\n')
				free(tmp);
			else {
				free(boot_dir_path);
				boot_dir_path = tmp;
			}
		}

		if (boot_dir_path[0] != '/') {
			ret = asprintf(&tmp, "%s/%s", getenv("PWD"), boot_dir_path);
			assert(ret != -1);
			free(boot_dir_path);
			boot_dir_path = tmp;
		}

		printf_("Installing to %s ...\n", boot_dir_path);
	}
	else
		boot_dir_path = assemble_path(dosemu_image_dir_path, "drive_c");

	ret = asprintf(&system_str, "mkdir -p %s/tmp", boot_dir_path);
	assert(ret != -1);

	if (system(system_str)) {
		printf_("  unable to create $BOOT_DIR_PATH, giving up\n");
		free(system_str);
		free(boot_dir_path);
		return 0;
	}
	free(system_str);

	if (choice != 2) {
		sys_path = assemble_path(dosemu_lib_dir_path, CMDS_SUFF);
		ret = asprintf(&system_str,
			"eval ln -s %s/fdconfig.sys "
			"\"%s\"",
			sys_path, boot_dir_path);
		free(sys_path);
		assert(ret != -1);
		if (system(system_str)) {
			printf_("Error: unable to copy startup files\n");
			free(system_str);
			free(boot_dir_path);
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
			printf_("Error: unable to copy startup files\n");
			free(system_str);
			free(boot_dir_path);
			return 0;
		}
		free(system_str);
	}
	free(boot_dir_path);
	ret = create_symlink_ex("${DOSEMU2_DRIVE_C}", 0, 1,
			dosemu_drive_c_path);
	unix_e_welcome = 1;
	return ret;
}

static char proprietary_notice[] =
"The DOSEMU part of the proprietary installation has been done.\n\n"
"Please make sure that you have a bootable DOS in '%s'.\n"
"Read the documentation for details on what it must contain.\n"
"The DOSEMU support commands are available within\n"
"%s/drive_z on drive D:. You might want to use different\n"
"config.sys and autoexec.bat files with your DOS. For example, you can try\n"
"to copy D:\\config.sys and D:\\autoemu.bat to C:\\, and adjust them.\n";

static int install_proprietary(char *proprietary, int warning)
{
	char x;
	int ok = create_symlink(proprietary, 0);
	if (!ok)
		return 0;
	if (!warning)
		return 1;
	printf_(proprietary_notice, proprietary, dosemu_lib_dir_path);
	printf_("\nPress ENTER to confirm, and boot DOSEMU, "
		"or [Ctrl-C] to abort\n");
	x = '\r';
	read_string(&x, 1);
	if (x == 3)
		leavedos(1);
	free(proprietary);
	return 1;
}

static int install_no_dosemu_freedos(void)
{
	char *p;
	printf_(
"\nDOSEMU-FreeDOS is not available to boot DOSEMU.\n"
"Please enter the name of a Linux directory which contains a bootable DOS, or\n"
"press [Ctrl-C] to abort for manual installation of FreeDOS or another DOS, or\n"
"press [ENTER] to quit if you suspect an error after manual installation.\n\n"
);
	p = dosreadline();
	if (p[0] == '\n')
		return install_dosemu_freedos(2);
	if (p[0] == '\3')
		leavedos(1);
	return install_proprietary(p, 1);
}

static int install_dos_(void)
{
	char x;
	int choice;

	if (config.install && config.install[0] == '\0') {
		/* no FreeDOS available: simple menu */
		return install_no_dosemu_freedos();
	}
	if (config.install[0]) {
		if (fddir_default && strcmp(config.install, fddir_default) == 0) {
			return install_dosemu_freedos(3);
		}
		return install_proprietary(strdup(config.install), 0);
	}

	if (config.quiet) {
		if (!fddir_default)
			leavedos(1);
		return install_dosemu_freedos(1);
	}

	printf_(
"\nPlease choose one of the following options:\n"
"1. Use a writable FreeDOS C: drive in ~/.dosemu/drive_c (recommended).\n"
"2. Use a read-only FreeDOS C: drive in %s/freedos.\n"
"3. Use a writable FreeDOS C: drive in another directory.\n"
"4. Use a different DOS than the provided DOSEMU-FreeDOS.\n"
"5. Exit this menu (completely manual setup).\n"
"[ENTER = the default option 1]\n", dosemu_lib_dir_path);
	x = '1';
	do {
		read_string(&x, 1);
		choice = (x == '\n' ? 1 : x - '0');
		switch (choice) {
		case 5:
			/* nothing to be done */
			return 0;
		case 2: {
			printf_("not implemented\n");
			leavedos(1);
		}
		case 4:
			printf_(
"Please enter the name of a directory which contains a bootable DOS\n");
			return install_proprietary(dosreadline(), 1);
		case 1:
		case 3:
			goto cont;
		default:
			continue;
		}
	} while (1);
cont:
	if (!fddir_default) {
		printf_("FreeDOS installation not found\n");
		leavedos(1);
	}
	return install_dosemu_freedos(choice);
}

void install_dos(void)
{
	int first_time;
	int symlink_created;
	int non_std = 0;
	char *dir_name =
		assemble_path(dosemu_image_dir_path, DOSEMU_DRIVES_DIR);

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
	printf_ = p_dos_str;
	read_string = bios_read_string;

	if (first_time)
		mkdir(dir_name, 0777);
	free(dir_name);

	symlink_created = 0;
	if (config.install && (!fddir_boot || config.install[0])) {
		symlink_created = install_dos_();
		non_std = 1;
	} else if (fddir_default || config.install) {
		if (fddir_boot) {
			symlink_created = install_dosemu_freedos(fddir_default ? 1 : 2);
		} else {
			error("fdpp not found, not doing install\n");
			return;
		}
	} else {
		error("comcom32 not found, not doing install\n");
		return;
	}
	if(symlink_created) {
		/* create symlink for D: too */
		create_symlink_ex("${DOSEMU2_DRIVE_D}", 1, 1,
				commands_path);
		if (!non_std) {
			create_symlink_ex("${DOSEMU2_DRIVE_E}", 2, 1, fddir_boot);
			create_symlink_ex("${DOSEMU2_DRIVE_F}", 3, 1, fddir_default);
		}
		disk_reset();
	}
	return;

exit_free:
	free(dir_name);
}
