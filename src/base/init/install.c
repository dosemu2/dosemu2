/* 
 * (C) Copyright 1992, ..., 2006 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "emu.h"
#include "disks.h"
#include "utilities.h"
#include "dos2linux.h"

static int terminal_read(char *buf32, u_short size)
{
	char x[size+2];
	fgets(x, size+2, stdin);
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
static int symlink_created;

static void create_symlink(const char *path)
{
	char *drives_c = assemble_path(LOCALDIR, "drives/c", 0);
	char *slashpos = drives_c + strlen(drives_c) - 2;
	static char symlink_txt[] =
		"Creating symbolic link for bootdirectory as %s\n";

	printf_(symlink_txt, drives_c);
	*slashpos = '\0';
	mkdir(drives_c, 0777);
	*slashpos = '/';
	unlink(drives_c);
	symlink(path, drives_c);
	free(hdisktab[0].dev_name);
	/* point C: to $HOME/.dosemu/drives/c */
	hdisktab[0].dev_name = drives_c;
	hdisktab[0].type = DIR_TYPE;
	symlink_created = 1;
}

static char *dosreadline(void)
{
	char *line = malloc(129);
	int size = read_string(line, 128);
	line[size] = '\0';
	return line;
}

static void install_dosemu_freedos (int choice)
{
	char *boot_dir_path, *system_str;

	/*
	 * boot_dir_path = path to install to
	 * We copy the binaries from the directory DOSEMU_LIBDEFAULT
	 * to the users $HOME.
	 */
	if (choice == 3) {
		printf_("Please enter the name of the directory for your private "
			   "DOSEMU-FreeDOS files\n");
		boot_dir_path = dosreadline();
		if (boot_dir_path[0] == '/') {
			char *boot_dir_path2;
			printf_("You gave an absolute path, "
				   "type ENTER to confirm or another path\n");
			boot_dir_path2 = dosreadline();
			if (boot_dir_path2[0] == '\0')
				free(boot_dir_path2);
			else {
				free(boot_dir_path);
				boot_dir_path = boot_dir_path2;
			}
		} else {
			char *tmp;
			asprintf(&tmp, "%s/%s", getenv("PWD"), boot_dir_path);
			free(boot_dir_path);
			boot_dir_path = tmp;
		}
		printf_("Installing to %s ...\n", boot_dir_path);
	}
	else
		boot_dir_path = assemble_path(LOCALDIR, "drive_c", 0);
	asprintf(&system_str, "mkdir -p %s", boot_dir_path);
	if (system(system_str)) {
		printf_("  unable to create $BOOT_DIR_PATH, giving up\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}
	free(system_str);
	asprintf(&system_str,
		 "tar xzf "DOSEMULIB_DEFAULT"/dosemu-bin.tgz -C \"%s\"",
		 boot_dir_path);
	system(system_str);
	free(system_str);
	create_symlink(boot_dir_path);
	asprintf(&system_str,
		 "tar xzf "DOSEMULIB_DEFAULT"/dosemu-freedos-bin.tgz -C \"%s\"",
		 boot_dir_path);
	system(system_str);
	free(system_str);
	free(boot_dir_path);
}

static char proprietary_notice[] =
"The DOSEMU part of the proprietary installation has been done.\n\n"
"Please make sure that you have a bootable DOS in '%s'.\n"
"Read the documentation for details on what it must contain.\n"
"You should also install a symbolic link to or make a copy of the files in\n"
DOSEMULIB_DEFAULT"/commands\n"
"in order to make the DOSEMU support commands available within %s.\n";

static void install_proprietary(char *proprietary, int warning)
{
	char x;
	create_symlink(proprietary);
	if (!warning)
		return;
	printf_(proprietary_notice, proprietary, proprietary);
	printf_("\nPress ENTER to confirm, and boot DOSEMU, "
		"or [Ctrl-C] to abort\n");
	x = '\r';
	read_string(&x, 1);
	if (x == 3)
		leavedos(1);
	free(proprietary);
}

static void install_no_dosemu_freedos(const char *path)
{
	char *p;
	int specified = 1;
	if (path[0] == '\0') {
		specified = 0;
		printf_(
"\nDOSEMU-FreeDOS is not available to boot DOSEMU.\n"
"Please enter the name of a Linux directory which contains a bootable DOS, or\n"
"press [Ctrl-C] to abort for manual installation of FreeDOS or another DOS, or\n"
"press [ENTER] to quit if you suspect an error after manual installation.\n\n"
);
		p = dosreadline();
		if (path[0] == '\0')
			return;
		if (path[0] == 3)
			leavedos(1);
	} else
		p = strdup(path);
	install_proprietary(p, !specified);
}

static void install_dos_(void)
{
	char x;
	int choice;
	int first_time = 0;

	if (!config.install) {
		char *disclaimer_file_name =
		  assemble_path(LOCALDIR, "disclaimer", 0);
		first_time = !exists_file(disclaimer_file_name);
		free(disclaimer_file_name);
		if (!first_time)
			return;
		/* OK, first boot! */
	}
	if (config.hdiskboot != 1) {
		/* user wants to boot from a different drive! */
		if (first_time)
			return;
		printf_("You can only use -install or -i if you boot from C:.\n");
		printf_("Press [ENTER] to continue.\n");
		read_string(&x, 1);
		return;
	}
	if (!exists_file(DOSEMULIB_DEFAULT"/freedos/kernel.sys")) {
		/* no FreeDOS available: simple menu */
		if (config.install)
			install_no_dosemu_freedos(config.install);
		return;
	}
	if (config.install) {
		if (strcmp(config.install, DOSEMULIB_DEFAULT"/freedos") == 0) {
			install_dosemu_freedos(3);
			return;
		}
		if (config.install[0] != '\0') {
			install_proprietary(strdup(config.install), 0);
			return;
		}
	}
	printf_(
"\nPlease choose one of the following options:\n"
"1. Use a writable FreeDOS C: drive in ~/.dosemu/drive_c (recommended).\n"
"2. Use a read-only FreeDOS C: drive in "DOSEMULIB_DEFAULT"/freedos.\n"
"3. Use a writable FreeDOS C: drive in another directory.\n"
"4. Use a different DOS than the provided DOSEMU-FreeDOS.\n"
"5. Exit this menu (completely manual setup).\n"
"[ENTER = the default option 1]\n");
	x = '5';
	read_string(&x, 1);
	choice = x - '0';
	if (choice == 5) {
		/* nothing to be done */
		return;
	}
	if (choice == 2) {
		install_proprietary(strdup(DOSEMULIB_DEFAULT"/freedos"), 0);
		return;
	}
	if (choice == 4) {
		printf_(
"Please enter the name of a directory which contains a bootable DOS\n");
		install_proprietary(dosreadline(), 1);
		return;
	}
	install_dosemu_freedos(choice);
}

void install_dos(int post_boot)
{
	if (post_boot) {
		printf_ = p_dos_str;
		read_string = com_biosread;
	}
	symlink_created = 0;
	install_dos_();
	do_liability_disclaimer_prompt(post_boot);
	if(post_boot && symlink_created)
		disk_reset();
}
