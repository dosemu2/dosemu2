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
#include "redirect.h"
#include "utilities.h"
#include "dos2linux.h"

static void create_symlink(const char *path, int post_boot)
{
	char *drives_c = assemble_path(LOCALDIR, "drives/c", 0);
	char *slashpos = drives_c + strlen(drives_c) - 2;
	char *lredir_path;
	static char symlink_txt[] =
		"Creating symbolic link for bootdirectory as %s\n";

	if (post_boot)
		com_printf(symlink_txt, drives_c);
	else
		printf(symlink_txt, drives_c);
	*slashpos = '\0';
	mkdir(drives_c, 0777);
	*slashpos = '/';
	unlink(drives_c);
	symlink(path, drives_c);
	/* point C: to $HOME/.dosemu/drives/c */
	asprintf(&lredir_path, "\\\\LINUX\\FS%s", drives_c);
	free(drives_c);
	if (post_boot) {
		CancelDiskRedirection(2);
		RedirectDisk(2, lredir_path, 0);
	}
	free(lredir_path);
}

static char *dosreadline(void)
{
	char *cr, *line = malloc(128);

	com_dosread(0, line, 128);
	cr = strchr(line, '\r');
	if (cr) *cr = '\0';
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
		com_printf("Please enter the name of the directory for your private "
			   "DOSEMU-FreeDOS files\n");
		boot_dir_path = dosreadline();
		if (boot_dir_path[0] == '/') {
			char *boot_dir_path2;
			com_printf("You gave an absolute path, "
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
		com_printf("Installing to %s ...\n", boot_dir_path);
	}
	else
		boot_dir_path = assemble_path(LOCALDIR, "drive_c", 0);
	asprintf(&system_str, "mkdir -p %s", boot_dir_path);
	if (system(system_str)) {
		com_printf("  unable to create $BOOT_DIR_PATH, giving up\n");
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
	create_symlink(boot_dir_path, 1);
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

static void install_proprietary(char *proprietary)
{
	char x[2];
	create_symlink(proprietary, 1);
	com_printf(proprietary_notice, proprietary, proprietary);
	com_printf("Type ENTER to confirm, and reboot DOSEMU\n");
	com_dosread(0, x, 2);
	free(proprietary);
	dos_ctrl_alt_del();
}

static void install_dos_from_terminal(void)
{
	int specified = 1;
	if (config.install[0] == 0 || !strcmp(config.install, "/dev/null")) {
		size_t n;
		char *nl;
		specified = 0;
		printf(
"\nDOSEMU-FreeDOS is not available to boot DOSEMU.\n"
"Please enter the name of a Linux directory which contains a bootable DOS, or\n"
"press [Ctrl-C] to abort for manual installation of FreeDOS or another DOS, or\n"
"press [ENTER] to quit if you suspect an error after manual installation.\n\n"
);
		config.install = NULL;
		getline(&config.install, &n, stdin);
		if (!config.install || n == 0)
			return;
		nl = strchr(config.install, '\n');
		if (nl) *nl = '\0';
		if (config.install[0] == '\0')
			return;
	}
	create_symlink(config.install, 0);
	printf(proprietary_notice, config.install, config.install);
	if (!specified) {
		printf("\nPress ENTER to confirm, and boot DOSEMU, "
		       "or [Ctrl-C] to abort\n");
		getchar();
	}
}

void install_dos(int post_boot)
{
	char x[3];
	int choice;

	do_liability_disclaimer_prompt(post_boot);
	if (!config.install)
		return;
	if (!post_boot) {
		install_dos_from_terminal();
		return;
	}
	if (strcmp(config.install, "/dev/null") == 0) {
		/* first time menu, check if we booted from FreeDOS */
		char *resource;
		int ro_flag;
		if (config.hdiskboot != 1 ||
		    GetRedirectionRoot(2, &resource, &ro_flag))
			return;
		if (ro_flag ||
		    strcmp(resource,
			   ALTERNATE_ETC"/drives/c/") != 0 ||
		    !exists_file(ALTERNATE_ETC"/drives/c/kernel.sys"))
			return;
		/* OK, first boot! */
		config.install = "";
	}
	if (strcmp(config.install, DOSEMULIB_DEFAULT"/freedos") == 0) {
		install_dosemu_freedos(3);
		return;
	}
	if (config.install[0] != '\0') {
		install_proprietary(strdup(config.install));
		return;
	}
	com_printf(
"\nPlease choose one of the following options:\n"
"1. Use a writable FreeDOS C: drive in ~/.dosemu/drive_c (recommended).\n"
"2. Use a read-only FreeDOS C: drive in "DOSEMULIB_DEFAULT"/freedos.\n"
"3. Use a writable FreeDOS C: drive in another directory.\n"
"4. Use a different DOS than the provided DOSEMU-FreeDOS.\n"
"5. Exit this menu (completely manual setup).\n"
"[ENTER = the default option 1]\n");
	com_dosread(0, x, 3);
	choice = x[0] - '0';
	if (choice == 2 || choice == 5) {
		/* nothing to be done */
		return;
	}
	if (choice == 4) {
		com_printf(
"Please enter the name of a directory which contains a bootable DOS\n");
		install_proprietary(dosreadline());
		return;
	}
	install_dosemu_freedos(choice);
}
