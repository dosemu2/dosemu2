#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "translate/unicode_symbols.h"
#include "translate/keysym_attributes.h"

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
"#include \"translate/keysym_attributes.h\"\n"
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
	case DKY_PAD_0:
	case DKY_PAD_1:
	case DKY_PAD_2:
	case DKY_PAD_3:
	case DKY_PAD_4:
	case DKY_PAD_5:
	case DKY_PAD_6:
	case DKY_PAD_7:
	case DKY_PAD_8:
	case DKY_PAD_9:
		attrib = KEYSYM_NUMBER;
		break;

	case DKY_PAD_DECIMAL:
	case DKY_PAD_SLASH:
	case DKY_PAD_AST:
	case DKY_PAD_MINUS:
	case DKY_PAD_PLUS:
	case DKY_PAD_ENTER:
	case DKY_PAD_HOME:
	case DKY_PAD_UP:
	case DKY_PAD_PGUP:
	case DKY_PAD_LEFT:
	case DKY_PAD_CENTER:
	case DKY_PAD_RIGHT:
	case DKY_PAD_END:
	case DKY_PAD_DOWN:
	case DKY_PAD_PGDN:
	case DKY_PAD_INS:
	case DKY_PAD_DEL:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

		/* function_keys */
	case DKY_F1:
	case DKY_F2:
	case DKY_F3:
	case DKY_F4:
	case DKY_F5:
	case DKY_F6:
	case DKY_F7:
	case DKY_F8:
	case DKY_F9:
	case DKY_F10:
	case DKY_F11:
	case DKY_F12:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* cursor block */
	case DKY_INS:
	case DKY_DEL:
	case DKY_HOME:
	case DKY_END:
	case DKY_PGUP:
	case DKY_PGDN:
	case DKY_UP:
	case DKY_DOWN:
	case DKY_LEFT:
	case DKY_RIGHT:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* shift keys */
	case DKY_L_ALT:
	case DKY_R_ALT:
	case DKY_L_CTRL:
	case DKY_R_CTRL:
	case DKY_L_SHIFT:
	case DKY_R_SHIFT:
	case DKY_NUM:
	case DKY_SCROLL:
	case DKY_CAPS:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* special keys */
	case DKY_PRTSCR:
	case DKY_PAUSE:
	case DKY_SYSRQ:
	case DKY_BREAK:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* more */
	case DKY_PAD_SEPARATOR:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* alt */
	case DKY_ALT_A:
	case DKY_ALT_B:
	case DKY_ALT_C:
	case DKY_ALT_D:
	case DKY_ALT_E:
	case DKY_ALT_F:
	case DKY_ALT_G:
	case DKY_ALT_H:
	case DKY_ALT_I:
	case DKY_ALT_J:
	case DKY_ALT_K:
	case DKY_ALT_L:
	case DKY_ALT_M:
	case DKY_ALT_N:
	case DKY_ALT_O:
	case DKY_ALT_P:
	case DKY_ALT_Q:
	case DKY_ALT_R:
	case DKY_ALT_S:
	case DKY_ALT_T:
	case DKY_ALT_U:
	case DKY_ALT_V:
	case DKY_ALT_W:
	case DKY_ALT_X:
	case DKY_ALT_Y:
	case DKY_ALT_Z:
		attrib = KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* more */
	case DKY_LEFT_TAB:
		attrib =  KEYSYM_EXTERNAL_FUNCTION;
		break;

	/* dead keys */
	case DKY_DEAD_ALT:
	case DKY_DEAD_CTRL:
	case DKY_DEAD_SHIFT:
	case DKY_DEAD_ALTGR:
	case DKY_DEAD_DKY_PAD:
		attrib = KEYSYM_DEAD;
		break;

	/* dosemu special functions */
	case DKY_DOSEMU_HELP:
	case DKY_DOSEMU_REDRAW:
	case DKY_DOSEMU_SUSPEND:
	case DKY_DOSEMU_RESET:
	case DKY_DOSEMU_MONO:
	case DKY_DOSEMU_PAN_UP:
	case DKY_DOSEMU_PAN_DOWN:
	case DKY_DOSEMU_PAN_LEFT:
	case DKY_DOSEMU_PAN_RIGHT:
	case DKY_DOSEMU_REBOOT:
	case DKY_DOSEMU_EXIT:
	case DKY_DOSEMU_VT_1:
	case DKY_DOSEMU_VT_2:
	case DKY_DOSEMU_VT_3:
	case DKY_DOSEMU_VT_4:
	case DKY_DOSEMU_VT_5:
	case DKY_DOSEMU_VT_6:
	case DKY_DOSEMU_VT_7:
	case DKY_DOSEMU_VT_8:
	case DKY_DOSEMU_VT_9:
	case DKY_DOSEMU_VT_10:
	case DKY_DOSEMU_VT_11:
	case DKY_DOSEMU_VT_12:
	case DKY_DOSEMU_VT_NEXT:
	case DKY_DOSEMU_VT_PREV:
	case DKY_MOUSE_UP:
	case DKY_MOUSE_DOWN:
	case DKY_MOUSE_LEFT:
	case DKY_MOUSE_RIGHT:
	case DKY_MOUSE_UP_AND_LEFT:
	case DKY_MOUSE_UP_AND_RIGHT:
	case DKY_MOUSE_DOWN_AND_LEFT:
	case DKY_MOUSE_DOWN_AND_RIGHT:
	case DKY_MOUSE_BUTTON_LEFT:
	case DKY_MOUSE_BUTTON_MIDDLE:
	case DKY_MOUSE_BUTTON_RIGHT:
	case DKY_MOUSE_GRAB:
	case DKY_DOSEMU_X86EMU_DEBUG:
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
