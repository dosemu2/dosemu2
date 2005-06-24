#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "unicode_symbols.h"
#include "keyboard.h"
#include "keysym_attributes.h"

#undef NUM_KEYSYMS
#define NUM_KEYSYMS 0x10000

struct print_state
{
	FILE *out;
	int line;
	int chars;
};
static void start_line(struct print_state *state, int max) 
{
	if (state->chars > max) {
		fprintf(state->out, "\n");
		state->line++;
		state->chars = 0;
	}
}
static void print_header(struct print_state *state)
{
	start_line(state, 0);
	fprintf(state->out,
"#include \"keysym_attributes.h\"\n"
"\n"
"/* automatically generated do not touch */\n "
"unsigned char keysym_attributes[] = \n"
"{\n"
		);
	state->line += 4;
	state->chars = 0;
}

static void print_trailer(struct print_state *state)
{
	start_line(state, 0);
	fprintf(state->out, 
"};\n"
		);
	
}

static void print_entry(struct print_state *state, int letter, int attrib)
{
	int amount;
#if 0
	fprintf(state->out, "/* 0x%04X */ 0x%02X,\n", letter, attrib);
	state->line++;
	state->chars = 0;
#else
	start_line(state, 75);
	amount = fprintf(state->out, "%d,", attrib);
	if (amount > 0) {
		state->chars += amount;
	}
#endif
}

static void dump_attributes(FILE *out, unsigned char *attributes)
{
	int i;
	struct print_state state[1];
	state->out = out;
	state->line = 1;
	state->chars = 0;
	print_header(state);
	for(i = 0; i < NUM_KEYSYMS; i++) {
		print_entry(state, i, attributes[i]);
	}
	print_trailer(state);
}

struct entry {
	unsigned long range_start;
	unsigned long range_end;
	char type[3];
};

static unsigned char compute_attribute(char *type)
{
	unsigned char attrib = KEYSYM_UNKNOWN;
	
	if (type[0] == 'M') {
		attrib = KEYSYM_COMBINING;
	}
	if (type[0] == 'N') {
		attrib = KEYSYM_NUMBER;
	}
	if (type[0] == 'Z') {
		attrib = KEYSYM_SPACE;
	}
	if (type[0] == 'C') {
		attrib = KEYSYM_CONTROL;
	}
	if (type[0] == 'L') {
		attrib = KEYSYM_LETTER;
	}
	if (type[0] == 'P') {
		attrib = KEYSYM_PUNCTUATION;
	}
	if (type[0] == 'S') {
		attrib = KEYSYM_SYMBOL;
	}
	return attrib;
}

static void store_attributes(struct entry *entry, unsigned char *attributes)
{
	int i;
	unsigned char attrib = compute_attribute(entry->type);
	for(i = entry->range_start; (i < NUM_KEYSYMS) && (i <= entry->range_end); i++) {
		attributes[i] = attrib;
	}
}

static int compute_missing_attribute(int letter)
{
	int attrib = KEYSYM_UNKNOWN;
	switch(letter) {
		/* keypad */
	case KEY_PAD_0:
	case KEY_PAD_1:
	case KEY_PAD_2:
	case KEY_PAD_3:
	case KEY_PAD_4:
	case KEY_PAD_5:
	case KEY_PAD_6:
	case KEY_PAD_7:
	case KEY_PAD_8:
	case KEY_PAD_9:
		attrib = KEYSYM_NUMBER;
		break;

	case KEY_PAD_DECIMAL:
	case KEY_PAD_SLASH:
	case KEY_PAD_AST:
	case KEY_PAD_MINUS:
	case KEY_PAD_PLUS:
	case KEY_PAD_ENTER:
	case KEY_PAD_HOME:
	case KEY_PAD_UP:
	case KEY_PAD_PGUP:
	case KEY_PAD_LEFT:
	case KEY_PAD_CENTER:
	case KEY_PAD_RIGHT:
	case KEY_PAD_END:
	case KEY_PAD_DOWN:
	case KEY_PAD_PGDN:
	case KEY_PAD_INS:
	case KEY_PAD_DEL:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

		/* function_keys */
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* cursor block */
	case KEY_INS:
	case KEY_DEL:
	case KEY_HOME:
	case KEY_END:
	case KEY_PGUP:
	case KEY_PGDN:
	case KEY_UP:
	case KEY_DOWN:
	case KEY_LEFT:
	case KEY_RIGHT:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* shift keys */
	case KEY_L_ALT:
	case KEY_R_ALT:
	case KEY_L_CTRL:
	case KEY_R_CTRL:
	case KEY_L_SHIFT:
	case KEY_R_SHIFT:
	case KEY_NUM:
	case KEY_SCROLL:
	case KEY_CAPS:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* special keys */
	case KEY_PRTSCR:
	case KEY_PAUSE:
	case KEY_SYSRQ:
	case KEY_BREAK:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* more */
	case KEY_PAD_SEPARATOR:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* alt */
	case KEY_ALT_A:
	case KEY_ALT_B:
	case KEY_ALT_C:
	case KEY_ALT_D:
	case KEY_ALT_E:
	case KEY_ALT_F:
	case KEY_ALT_G:
	case KEY_ALT_H:
	case KEY_ALT_I:
	case KEY_ALT_J:
	case KEY_ALT_K:
	case KEY_ALT_L:
	case KEY_ALT_M:
	case KEY_ALT_N:
	case KEY_ALT_O:
	case KEY_ALT_P:
	case KEY_ALT_Q:
	case KEY_ALT_R:
	case KEY_ALT_S:
	case KEY_ALT_T:
	case KEY_ALT_U:
	case KEY_ALT_V:
	case KEY_ALT_W:
	case KEY_ALT_X:
	case KEY_ALT_Y:
	case KEY_ALT_Z:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* more */
	case KEY_LEFT_TAB:
		attrib =  KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* dead keys */
	case KEY_DEAD_ALT:
	case KEY_DEAD_CTRL:
	case KEY_DEAD_SHIFT:
	case KEY_DEAD_ALTGR:
	case KEY_DEAD_KEY_PAD:
		attrib = KEYSYM_DEAD;
		break;
		
	/* dosemu special functions */
	case KEY_DOSEMU_HELP:
	case KEY_DOSEMU_REDRAW:
	case KEY_DOSEMU_SUSPEND:
	case KEY_DOSEMU_RESET:
	case KEY_DOSEMU_MONO:
	case KEY_DOSEMU_PAN_UP:
	case KEY_DOSEMU_PAN_DOWN:
	case KEY_DOSEMU_PAN_LEFT:
	case KEY_DOSEMU_PAN_RIGHT:
	case KEY_DOSEMU_REBOOT:
	case KEY_DOSEMU_EXIT:
	case KEY_DOSEMU_VT_1:
	case KEY_DOSEMU_VT_2:
	case KEY_DOSEMU_VT_3:
	case KEY_DOSEMU_VT_4:
	case KEY_DOSEMU_VT_5:
	case KEY_DOSEMU_VT_6:
	case KEY_DOSEMU_VT_7:
	case KEY_DOSEMU_VT_8:
	case KEY_DOSEMU_VT_9:
	case KEY_DOSEMU_VT_10:
	case KEY_DOSEMU_VT_11:
	case KEY_DOSEMU_VT_12:
	case KEY_DOSEMU_VT_NEXT:
	case KEY_DOSEMU_VT_PREV:
	case KEY_MOUSE_UP:
	case KEY_MOUSE_DOWN:
	case KEY_MOUSE_LEFT:
	case KEY_MOUSE_RIGHT:
	case KEY_MOUSE_UP_AND_LEFT:
	case KEY_MOUSE_UP_AND_RIGHT:
	case KEY_MOUSE_DOWN_AND_LEFT:
	case KEY_MOUSE_DOWN_AND_RIGHT:
	case KEY_MOUSE_BUTTON_LEFT:
	case KEY_MOUSE_BUTTON_MIDDLE:
	case KEY_MOUSE_BUTTON_RIGHT:
	case KEY_MOUSE_GRAB:
	case KEY_DOSEMU_X86EMU_DEBUG:
		attrib = KEYSYM_INTERNAL_FUNCTION;
		break;
	}
	return attrib;
}

static int read_next_entry(FILE *in, struct entry *entry)
{
	char number_str1[5];
	char number_str2[5];
	char type_str[3];
	char data[2+1+4+1+4+1+100];
	char *result;
	do {
		result = fgets(data, sizeof(data), in);
		if (result == 0) {
			return -1;
		}
	} while(data[0] == '#');
	memcpy(type_str, data+0, 2);
	type_str[2] = '\0';
	memcpy(number_str1, data+3, 4);
	number_str1[4] = '\0';
	if (data[7] == '-') {
		memcpy(number_str2, data+8, 4);
		number_str2[4] = '\0';
	} else {
		strcpy(number_str2, number_str1);
	}
	entry->range_start = strtoul(number_str1, NULL, 16);
	entry->range_end = strtoul(number_str2, NULL, 16);
	strcpy(entry->type, type_str);

	return 0;
}

static void init_keysyms(unsigned char *attributes)
{
	int i;
	for(i = 0; i < NUM_KEYSYMS; i++) {
		attributes[i] = compute_missing_attribute(i);
	}
}

static void read_file(FILE *in, unsigned char *attributes)
{
	struct entry entry[1];
	while(read_next_entry(in, entry) == 0) {
		store_attributes(entry, attributes);
	}
}

static void process_file(FILE *in, FILE *out)
{
	static unsigned char attributes[NUM_KEYSYMS];
	init_keysyms(attributes);
	read_file(in, attributes);
	dump_attributes(out, attributes);
}

int main(int argc, char**argv)
{
	if (argc != 1) {
		fprintf(stderr, "usage %s < infilename > outfile \n",
			argv[0]);
		exit(1);
	}
	process_file(stdin, stdout);
	return 0;
}
