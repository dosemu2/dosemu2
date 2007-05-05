/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
static char *dosemu_lib_dir;
int unix_e_welcome;

void show_welcome_screen(void)
{
	/* called from unix -e */
	do_liability_disclaimer_prompt(1, 0);
	p_dos_str(
"Continue to confirm, or enter EXITEMU or press Ctrl-Alt-PgDn to exit DOSEMU.\n\n");	p_dos_str(
	"Your DOS drives are set up as follows:\n"
	" A: floppy drive (if it exists)\n"
	" C: points to the Linux directory ~/.dosemu/drive_c\n"
	" D: points to your Linux home directory\n"
	" E: points to your CD-ROM drive, if it is mounted at /media/cdrom\n"
	" Z: points to the read-only DOSEMU and FreeDOS commands directory\n"
	"Use the LREDIR DOSEMU command to adjust these settings, or edit\n"
        "/etc/dosemu/dosemu.conf, ~/.dosemurc, c:\\config.sys, or c:\\autoexec.bat,\n"
	"or change the symbolic links in ~/.dosemu/drives.\n\n"
	"To re-install a DOS, exit and then restart DOSEMU using dosemu -i.\n"
	"Enter HELP for more information on DOS and DOSEMU commands.\n");
	p_dos_str(
"Keys: Ctrl-Alt-... F=fullscreen, K=grab keyboard, Home=grab mouse, Del=reboot\n"
"      Ctrl-Alt-PgDn=exit; Ctrl-^: use special keys on terminals\n");
}

static void create_symlink(const char *path, int number)
{
	char *drives_c = assemble_path(LOCALDIR, "drives/c", 0);
	char *slashpos = drives_c + strlen(drives_c) - 2;
	static char symlink_txt[] =
		"Creating symbolic link for %s as %s\n";

	printf_(symlink_txt, path, drives_c);
	*slashpos = '\0';
	slashpos[1] += number; /* make it 'd' if 'c' */
	mkdir(drives_c, 0777);
	*slashpos = '/';
	unlink(drives_c);
	symlink(path, drives_c);
	free(hdisktab[number].dev_name);
	/* point C:/D: to $HOME/.dosemu/drives/c or d */
	hdisktab[number].dev_name = drives_c;
	hdisktab[number].type = DIR_TYPE;
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
	 * We copy the binaries from the directory dosemu_lib_dir
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
	asprintf(&system_str, "mkdir -p %s/tmp", boot_dir_path);
	if (system(system_str)) {
		printf_("  unable to create $BOOT_DIR_PATH, giving up\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}
	free(system_str);
	asprintf(&system_str,
		 "cp -a %s/drive_z/config.sys "
		 "%s/drive_z/autoexec.bat \"%s\"",
		 dosemu_lib_dir, dosemu_lib_dir, boot_dir_path);
	system(system_str);
	free(system_str);
	create_symlink(boot_dir_path, 0);
	free(boot_dir_path);
	unix_e_welcome = 1;
}

static char proprietary_notice[] =
"The DOSEMU part of the proprietary installation has been done.\n\n"
"Please make sure that you have a bootable DOS in '%s'.\n"
"Read the documentation for details on what it must contain.\n"
"The DOSEMU support commands are available within\n"
"%s/drive_z on drive D:. You might want to use different\n"
"config.sys and autoexec.bat files with your DOS. For example, you can try\n"
"to copy D:\\config.emu and D:\\autoemu.bat to C:\\, adjust them, and use the\n"
"$_emusys option in ~/.dosemurc.\n";

static void install_proprietary(char *proprietary, int warning)
{
	char x;
	create_symlink(proprietary, 0);
	if (!warning)
		return;
	printf_(proprietary_notice, proprietary, dosemu_lib_dir);
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
		if (p[0] == '\0')
			return;
		if (p[0] == 3)
			leavedos(1);
	} else
		p = strdup(path);
	install_proprietary(p, !specified);
}

static int first_boot_time(void)
{
	int first_time;
	char *disclaimer_file_name =
		assemble_path(LOCALDIR, "disclaimer", 0);
	first_time = !exists_file(disclaimer_file_name);
	free(disclaimer_file_name);
	return first_time;
}

static void install_dos_(char *kernelsyspath)
{
	char x;
	int choice;

	if (config.hdiskboot != 1) {
		/* user wants to boot from a different drive! */
		if (!config.install)
			return;
		printf_("You can only use -install or -i if you boot from C:.\n");
		printf_("Press [ENTER] to continue.\n");
		read_string(&x, 1);
		return;
	}
	if (!exists_file(kernelsyspath)) {
		/* no FreeDOS available: simple menu */
		if (config.install)
			install_no_dosemu_freedos(config.install);
		return;
	}
	if (config.install) {
		char *freedos = assemble_path(dosemu_lib_dir, "freedos", 0);
		char *drive_z = assemble_path(dosemu_lib_dir, "drive_z", 0);
		if (strcmp(config.install, freedos) == 0 ||
		    strcmp(config.install, drive_z) == 0) {
			install_dosemu_freedos(3);
			free(freedos);
			free(drive_z);
			return;
		}
		free(freedos);
		free(drive_z);
		if (config.install[0] != '\0') {
			install_proprietary(strdup(config.install), 0);
			return;
		}
	}
	printf_(
"\nPlease choose one of the following options:\n"
"1. Use a writable FreeDOS C: drive in ~/.dosemu/drive_c (recommended).\n"
"2. Use a read-only FreeDOS C: drive in %s/freedos.\n"
"3. Use a writable FreeDOS C: drive in another directory.\n"
"4. Use a different DOS than the provided DOSEMU-FreeDOS.\n"
"5. Exit this menu (completely manual setup).\n"
"[ENTER = the default option 1]\n", dosemu_lib_dir);
	x = '1';
	read_string(&x, 1);
	choice = x - '0';
	if (choice == 5) {
		/* nothing to be done */
		return;
	}
	if (choice == 2) {
		char *freedos = assemble_path(dosemu_lib_dir, "freedos", 0);
		install_proprietary(freedos, 0);
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
	char *kernelsyspath;
	int first_time;
	first_time = first_boot_time();
	if (!config.install && !first_time)
		return;
	if (post_boot) {
		printf_ = p_dos_str;
		read_string = com_biosread;
	}
	symlink_created = 0;
	dosemu_lib_dir = getenv("DOSEMU_LIB_DIR");
	if (!dosemu_lib_dir) dosemu_lib_dir = "";
	kernelsyspath = assemble_path(dosemu_lib_dir, "drive_z/kernel.sys", 0);
	if (config.hdiskboot != 1 ||
	    config.install ||
	    !exists_file(kernelsyspath)) {
		if (first_time) {
			do_liability_disclaimer_prompt(post_boot, 1);
			if (!config.X)
				printf_(
"DOSEMU will run on _this_ terminal.\n"
"To exit you need to execute \'exitemu\' from within DOS,\n"
"because <Ctrl>-C and \'exit\' won\'t work!\n");
		}
		install_dos_(kernelsyspath);
	} else 
		install_dosemu_freedos(1);
	free(kernelsyspath);
	if(symlink_created) {
		/* create symlink for D: too */
		char *commands_path = 
			assemble_path(dosemu_lib_dir, "drive_z", 0);
		create_symlink(commands_path, 1);
		free(commands_path);
		if(post_boot)
			disk_reset();
	}
}
