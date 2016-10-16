/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

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
static int symlink_created;
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
	do_liability_disclaimer_prompt(1, 0);
	p_dos_str(
"Continue to confirm, or enter EXITEMU or press Ctrl-Alt-PgDn to exit DOSEMU.\n\n");	p_dos_str(
	"Your DOS drives are set up as follows:\n"
	" A: floppy drive (if it exists)\n"
	" C: points to the Linux directory ~/.dosemu/drive_c\n"
	" D: points to FreeDOS installation\n"
	" E: points to the read-only DOSEMU commands directory\n"
	" F: points to your CD-ROM drive, if it is mounted at /media/cdrom\n"
	"Use the lredir2 DOSEMU command to adjust these settings, or edit\n"
        "/etc/dosemu/dosemu.conf, ~/.dosemurc, c:\\fdconfig.sys, or c:\\autoexec.bat.\n\n"
	"To re-install a DOS, exit and then restart DOSEMU using dosemu -i.\n"
	"Enter HELP for more information on DOS and DOSEMU commands.\n");
	p_dos_str(
"Keys: Ctrl-Alt-... F=fullscreen, K=grab keyboard, Home=grab mouse, Del=reboot\n"
"      Ctrl-Alt-PgDn=exit; Ctrl-^: use special keys on terminals\n");
}

static void create_symlink_ex(const char *path, int number, int special,
	const char *path2)
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
	if (special) {
		char *cmd;
		asprintf(&cmd, "echo -n '%s' >%s.lnk", path, drives_c);
		system(cmd);
		free(cmd);
		free(drives_c);
		drives_c = strdup(path2);
	} else {
		symlink(path, drives_c);
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
	symlink_created = 1;
}

static void create_symlink(const char *path, int number)
{
	create_symlink_ex(path, number, 0, NULL);
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

static void install_dosemu_freedos (int choice)
{
	char *boot_dir_path, *system_str, *tmp, *sys_path;
	int ret;

	/*
	 * boot_dir_path = path to install to
	 * We copy the binaries from the directory dosemu_lib_dir
	 * to the users $HOME.
	 */
	boot_dir_path = assemble_path(LOCALDIR, "drive_c", 0);

	ret = asprintf(&system_str, "mkdir -p %s/tmp", boot_dir_path);
	assert(ret != -1);

	if (system(system_str)) {
		printf_("  unable to create $BOOT_DIR_PATH, giving up\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}
	free(system_str);

	sys_path = assemble_path(dosemu_lib_dir_path, CMDS_SUFF, 0);
	ret = asprintf(&system_str,
			"cp -p %s/fdconfig.sys "
			"%s/autoexec.bat \"%s\"",
			sys_path, sys_path, boot_dir_path);
	free(sys_path);
	assert(ret != -1);
	if (system(system_str)) {
		printf_("Error: unable to copy startup files\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}
	free(system_str);
	/* symlink command.com in case someone hits Shift or F5 */
	ret = asprintf(&system_str,
			"ln -s ../drives/d/command.com "
			"../drives/d/kernel.sys "
			"\"%s\"",
			boot_dir_path);
	assert(ret != -1);
	if (system(system_str)) {
		printf_("Error: unable to copy startup files\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}
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
"to copy D:\\config.sys and D:\\autoemu.bat to C:\\, and adjust them.\n";

static void install_proprietary(char *proprietary, int warning)
{
	char x;
	create_symlink(proprietary, 0);
	if (!warning)
		return;
	printf_(proprietary_notice, proprietary, dosemu_lib_dir_path);
	printf_("\nPress ENTER to confirm, and boot DOSEMU, "
		"or [Ctrl-C] to abort\n");
	x = '\r';
	read_string(&x, 1);
	if (x == 3)
		leavedos(1);
	free(proprietary);
}

static void call_installdos(void)
{
	char *boot_dir_path, *dos_dir_path, *system_str, *sys_path, *configDotSys, *autoexecDotBat, *output, *install_source_dirname, *chosen_dos;
	char *dos_flavours[255];
	char x;
	int ret, dos_count = 0, choice = 0;
	FILE *fp;
	boot_dir_path = assemble_path(LOCALDIR, "drive_c", 0);
	dos_dir_path  = assemble_path(LOCALDIR, "drive_d", 0);
	configDotSys = malloc(8 + 1 + 3 + 1);
	autoexecDotBat = malloc(8 + 1 + 3 + 1);
	output = malloc(80 + 1);

	printf_("Please choose which DOS flavour to install:\n");
	fp = popen("loaddosinstall -l", "r");
	while (fgets(output, 80 + 1, fp) != NULL) {
		char *short_name = malloc(20);
		char *name = malloc(255);
		sscanf(output, "%s %[^\n]", short_name, name);
		dos_flavours[dos_count] = short_name;
		printf_("%d. %s\n", ++dos_count, name);
	}
	ret = pclose(fp);
	assert(ret != -1);

	do {
		read_string(&x, 1);
		choice = x - '0';
	} while (choice > dos_count || choice < 1);

	chosen_dos = dos_flavours[choice - 1];

	ret = asprintf(&system_str, "%s/install/%s", DOSEMUHDIMAGE_DEFAULT, chosen_dos);
	assert(ret != -1);
	DIR *dir = opendir(system_str);
	if (dir != NULL) {
		install_source_dirname = malloc(255);
		strcpy(install_source_dirname, system_str);
	} else {
		char template[] = "/tmp/dosemu.XXXXXX";
		install_source_dirname = mkdtemp(template);

		ret = asprintf(&system_str, "%s/downloaddos -d %s %s", DOSEMULIB_DEFAULT, chosen_dos, install_source_dirname);
		assert(ret != 1);
		fp = popen(system_str, "r");

		while (fgets(output, 80 + 1, fp) != NULL) {
			printf_("%s\n", output);
		}
		ret = pclose(fp);
		assert(ret != -1);
	}

	ret = mkdir(boot_dir_path, 0777);
	assert(ret == 0);
	ret = asprintf(&system_str, "%s/installdos %s %s", DOSEMULIB_DEFAULT, install_source_dirname, dos_dir_path);
	assert(ret != 1);

	fp = popen(system_str, "r");

	while (fgets(output, 80 + 1, fp) != NULL) {
		printf_("%s\n", output);
	}

	ret = pclose(fp);
	assert(ret != -1);

	create_symlink(boot_dir_path, 0);
	create_symlink(dos_dir_path, 1);

	sys_path = assemble_path(dosemu_lib_dir_path, CMDS_SUFF, 0);
	if (strncmp("freedos", chosen_dos, 7) == 0) {
		strcpy(configDotSys, "fdconfig.sys");
		strcpy(autoexecDotBat, "autoexec.bat");
	}
	if (strncmp("msdos", chosen_dos, 5) == 0) {
		strcpy(configDotSys, "config.sys");
		strcpy(autoexecDotBat, "autoemu.bat");
	}
	ret = asprintf(&system_str,
			"cp -p %s/%s "
			"%s/%s \"%s\"",
			sys_path, configDotSys, sys_path, autoexecDotBat, boot_dir_path);
	free(sys_path);
	assert(ret != -1);
	if (system(system_str)) {
		printf_("Error: unable to copy startup files\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}
	if (strncmp("freedos", chosen_dos, 7) == 0) {
			/* symlink command.com in case someone hits Shift or F5 */
			ret = asprintf(&system_str,
					"ln -s ../drives/d/command.com "
					"../drives/d/kernel.sys "
					"\"%s\"",
					boot_dir_path);
	}
	if (strncmp("msdos", chosen_dos, 5) == 0) {
			ret = asprintf(&system_str,
					"ln -s ../drives/d/command.com "
					"../drives/d/io.sys ../drives/d/msdos.sys "
					"\"%s\"",
					boot_dir_path);
	}
	assert(ret != -1);
	if (system(system_str)) {
		printf_("Error: unable to copy startup files\n");
		free(system_str);
		free(boot_dir_path);
		return;
	}

	char *commands_path = "${DOSEMU_COMMANDS_DIR}";
	create_symlink_ex(commands_path, 2, 1,
			getenv("DOSEMU_COMMANDS_DIR"));

	// skip symlink creation which is triggered in install_dos when this is 1
	symlink_created = 0;

	free(dos_dir_path);
	free(boot_dir_path);
	free(system_str);
}

static void install_no_dosemu_freedos(void)
{
	char x;
	int choice;
	printf_(
"\nPlease choose one of the following options:\n"
"1. Automatic installation.\n"
"2. Manual installation.\n"
"3. Exit this menu (completely manual setup).\n"
"[ENTER = the default option 1]\n");
	x = '1';
	do {
		read_string(&x, 1);
		choice = x - '0';
		switch (choice) {
		case 3:
			/* nothing to be done */
			return;
		case 2:
			printf_(
"Please enter the name of a directory which contains a bootable DOS\n");
			install_proprietary(dosreadline(), 1);
			return;
		case 1:
			call_installdos();
			return;
		default:
			continue;
		}
	} while (1);
}

static int first_boot_time(void)
{
	int first_time;
	char *dir_name =
		assemble_path(LOCALDIR, "drives", 0);
	first_time = !exists_dir(dir_name);
	free(dir_name);
	return first_time;
}

static int disclaimer_shown(void)
{
	int shown;
	char *disclaimer_file_name =
		assemble_path(LOCALDIR, "disclaimer", 0);
	shown = exists_file(disclaimer_file_name);
	free(disclaimer_file_name);
	return shown;
}

void install_dos(int post_boot)
{
	char *kernelsyspath;
	int first_time;
	char x;
	int choice;

	if (!disclaimer_shown())
		do_liability_disclaimer_prompt(post_boot, !config.quiet);
	first_time = first_boot_time();
	if (!config.install && !first_time &&
			(config.hdisks || config.bootdisk))
		return;
	if (config.emusys) {
		error("$_emusys must be disabled before installing DOS\n");
//		leavedos(24);
	}
	if (post_boot) {
		printf_ = p_dos_str;
		read_string = bios_read_string;
	}

	symlink_created = 0;
	kernelsyspath = assemble_path(fddir_default, "kernel.sys", 0);
	if (config.hdiskboot != 2 ||
	    config.install ||
	    !exists_file(kernelsyspath)) {
		install_no_dosemu_freedos();
	} else {
		printf_(
"\nPlease choose one of the following options:\n"
"1. Symlink existing FreeDOS installation from %s.\n"
"2. Install another DOS.\n"
"[ENTER = the default option 1]\n", fddir_default);
		x = '1';
		do {
			read_string(&x, 1);
			choice = (x == '\n' ? 1 : x - '0');
			switch (choice) {
			case 2: {
				install_no_dosemu_freedos();
				goto cont;
			}
			case 1: {
				install_dosemu_freedos(1);
				goto cont;
			}
			default:
				continue;
			}
		} while (1);
	}
cont:
	free(kernelsyspath);
	if(symlink_created) {
		/* create symlink for D: too */
		char *commands_path = "${DOSEMU_COMMANDS_DIR}";
		create_symlink(fddir_default, 1);
		create_symlink_ex(commands_path, 2, 1,
				getenv("DOSEMU_COMMANDS_DIR"));
	}
	if(post_boot)
		disk_reset();
}
