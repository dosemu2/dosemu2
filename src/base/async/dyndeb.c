/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* dynamic debug handlers - by Tim Bird */
/* modified to support debug levels -- peak */
/* Rehash so we aren't changing the code all of the time. Eric Biederman */
#include "config.h"
#include <string.h>
#include "dosemu_debug.h"
#include "dosemu_config.h"
#include "init.h"
#include "int.h"
#include "port.h"


#ifndef NO_DEBUGPRINT_AT_ALL

int register_debug_class(int letter, void (*change_level)(int level), char *help_text)
{
	struct debug_class *class;
	if (letter >= DEBUG_CLASSES) {
		return -1;
	}
	if ((letter >= '0') && (letter <= '9')) {
		return -1;
	}
	class = &debug[letter];
	if (class->letter) {
		return -1;
	}
	class->letter = letter;
	class->change_level = change_level;
	class->help_text = help_text;
	class->level = 0;
	return 0;
}

int unregister_debug_class(int letter)
{
	struct debug_class *class;
	if (letter >= DEBUG_CLASSES) {
		return -1;
	}
	class = &debug[letter];
	if (!class->letter) {
		return -1;
	}
	memset(class, 0, sizeof(*class));
	return 0;
}

int set_debug_level(int letter, int level)
{
	struct debug_class *class;
	if (letter >= DEBUG_CLASSES) {
		return -1;
	}
	class = &debug[letter];
	if (!class->letter) {
		return -1;
	}
	if (class->level != level) {
		class->level = level;
		if (class->change_level) {
			class->change_level(level);
		}
	}
	return 0;
}

int debug_level(int letter)
{
	struct debug_class *class;
	if (letter >= DEBUG_CLASSES) {
		return -1;
	}
	class = &debug[letter];
	if (!class->letter) {
		return -1;
	}
	return class->level;
}


/*
 * DANG_BEGIN_FUNCTION parse_debugflags
 * 
 * arguments: 
 * s - string of options.
 * 
 * description: 
 * This part is fairly flexible...you specify the debugging
 * flags you wish with -D string.  The string consists of the following
 * characters: +   turns the following options on (initial state) -
 * turns the following options off a   turns all the options on/off,
 * depending on whether +/- is set 0-9 sets debug levels (0 is off, 9 is
 * most verbose) #   where # is a letter from the valid option list (see
 * docs), turns that option off/on depending on the +/- state.
 * 
 * Any option letter can occur in any place.  Even meaningless combinations,
 * such as "01-a-1+0vk" will be parsed without error, so be careful. Some
 * options are set by default, some are clear. This is subject to my whim.
 * You can ensure which are set by explicitly specifying.
 * 
 * DANG_END_FUNCTION
 */
int parse_debugflags(const char *s, unsigned char flag)
{
	char            c;
	int ret = 0;
	
	dbug_printf("debug flags: %s\n", s);
	while ((c = *(s++)))
		switch (c) {
		case '+':		/* begin options to turn on */
			if (!flag)
				flag = 1;
			break;
		case '-':		/* begin options to turn off */
			flag = 0;
			break;
		case '0'...'9':	/* set debug level, 0 is off, 9 is most
				 * verbose */
			flag = c - '0';
			break;
		default:
			ret = set_debug_level(c, flag);
			if (ret >= 0) {
				ret = 0;
			} else {
				fprintf(stderr, "Unknown debug-msg mask: %c\n\r", c);
				dbug_printf("Unknown debug-msg mask: %c\n", c);
				ret = 1;
			}
		}
	return ret;
}

int SetDebugFlagsHelper(char *debugStr)
{
	return parse_debugflags(debugStr, 0);
}

static char DebugFlag(int f)
{
	if (f == 0)
		return '-';
	else if (f >= 2 && f <= 9)
		return f + '0';
	else
		return '+';
}

int GetDebugFlagsHelper(char *debugStr, int print)
{
	int i;
	struct debug_class *class;
	
	if (print) dbug_printf("GetDebugFlagsHelper\n");
	if (print) dbug_printf("debugStr at %p\n", debugStr);
	i = 0;
	
	for(class = debug; class <= &debug[DEBUG_CLASSES-1]; class++) {
		if (!class->letter) {
			continue;
		}
		debugStr[i++] = DebugFlag(class->level); 
		debugStr[i++] = class->letter;
	}
	
	debugStr[i++] = '\0';
	if (print) dbug_printf("debugStr is %s\n", debugStr);
	
	return (0);
}

void print_debug_usage(FILE *stream)
{
	struct debug_class *class;
	int i;
	fprintf(stream, 
		"    -D set debug-msg mask to flags {+-}{0-9}{");
	for(class = debug; class <= &debug[DEBUG_CLASSES-1]; class++) {
		if (class->letter) {
			putc(class->letter, stream);
		}
	}
	fprintf(stream, "}\n");
	i = 0;
	for(class = debug; class <= &debug[DEBUG_CLASSES-1]; class++) {
		if (!class->letter) {
			continue;
		}
		if ((i & 1) == 0) {
			fprintf(stream, "      ");
		}
		fprintf(stream, " %c=%-33.33s",
			class->letter, class->help_text);
		if ((i & 1) == 1) {
			fprintf(stream, "\n");
		}
		i++;
	}
	if ((i & 1) == 1) {
		fprintf(stream, "\n");
	}
}

static void all_change_level(int level)
{
	int c;
	for (c = 0; c < DEBUG_CLASSES; c++) {
		if (c != 'a') {
			set_debug_level(c, level);
		}
	}
}
static void int21_change_level(int level)
{
	static int first = 1;
	if (first) {
		set_int21_revectored(level?1:0);
		first = 0;
	}
}
static void port_trace_change_level(int level)
{
	init_port_traceing();
}

static void config_change_level(int level)
{
	if (config_check_only && !level) {
		set_debug_level('c', 1);
	}
}

CONSTRUCTOR(static void init(void))
{
	register_debug_class('a', all_change_level, "Set all levels");
	register_debug_class('d', 0, "disk msgs");
	register_debug_class('R', 0, "disk READ");
	register_debug_class('W', 0, "disk WRITE");
	register_debug_class('D', int21_change_level, "dos int 21h");
	register_debug_class('C', 0, "CDROM");
	register_debug_class('v', 0, "video");
	register_debug_class('X', 0, "X support");       
	register_debug_class('k', 0, "keyboard");
	register_debug_class('i', 0, "i/o instructions (in/out)");
	register_debug_class('T', port_trace_change_level, "I/O trace");
	register_debug_class('s', 0, "serial");
	register_debug_class('m', 0, "mouse");
	register_debug_class('#', 0, "default int");
	register_debug_class('p', 0, "printer");
	register_debug_class('g', 0, "general messages");
	register_debug_class('c', config_change_level, "configuration");
	register_debug_class('w', 0, "warnings");
	register_debug_class('h', 0, "hardware");
	register_debug_class('I', 0, "IPC");
	register_debug_class('j', 0, "joystick");
	register_debug_class('E', 0, "EMS");
	register_debug_class('x', 0, "XMS");
	register_debug_class('M', 0, "DPMI");
	register_debug_class('n', 0, "IPX network");
	register_debug_class('P', 0, "Packet driver");
	register_debug_class('Q', 0, "Mapping driver");
	register_debug_class('r', 0, "PIC request");
	register_debug_class('S', 0, "SOUND");
	register_debug_class('A', 0, "ASPI");
	register_debug_class('Z', 0, "PCI");
#ifdef X86_EMULATOR
	register_debug_class('e', 0, "cpu-emu");
#endif
#ifdef TRACE_DPMI
	register_debug_class('t', 0, "dpmi trace");
#endif
};

#endif /* ! NO_DEBUGPRINT_AT_ALL */
