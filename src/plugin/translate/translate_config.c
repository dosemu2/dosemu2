#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include "dosemu_debug.h"
#include "translate.h"
#include "dosemu_charset.h"
#include "dosemu_config.h"
#include "init.h"
#include "emu.h"
#include "video.h"
#include "utilities.h"

struct translate_config_t trconfig; /* Intialized to nulls */

static void config_translate_scrub(void)
{
    char *charset;
    k_printf("config.term_charset=%d\n", config.term_charset);
    /* set the character sets used base upon config.term_charset */
    setlocale (LC_ALL, "");
    if (!trconfig.unix_charset) {
	charset = strdup(nl_langinfo(CODESET));
	strlower(charset);
	if (memcmp(charset, "iso-", 4) == 0)
	    memmove(charset + 3, charset + 4, strlen(charset + 3));
	trconfig.unix_charset = lookup_charset(charset);
	free(charset);
	if (!trconfig.unix_charset)
	    trconfig.unix_charset = lookup_charset("iso8859-1");
    }     
    if (!trconfig.paste_charset)
	trconfig.paste_charset = trconfig.unix_charset;
    if (!trconfig.keyb_charset)
	trconfig.keyb_charset = trconfig.unix_charset;
    switch (config.term_charset) {
    case CHARSET_FULLIBM:
	error("WARNING: 'charset fullibm' doesn't work.  Use 'charset ibm' instead.\n");
	/* fallthrough */
    case CHARSET_IBM:
	if (!trconfig.video_mem_charset) 
		trconfig.video_mem_charset = lookup_charset("cp437");
	if (!trconfig.keyb_config_charset)
		trconfig.keyb_config_charset = 
			get_terminal_charset(lookup_charset("cp437"));
	if (!trconfig.output_charset)
		trconfig.output_charset = 
			get_terminal_charset(lookup_charset("cp437"));
	if (!trconfig.dos_charset)
		trconfig.dos_charset = lookup_charset("cp437");
	break;
    case CHARSET_LATIN1:
	if (!trconfig.video_mem_charset) 
		trconfig.video_mem_charset = lookup_charset("cp850");
	if (!trconfig.keyb_config_charset)
		trconfig.keyb_config_charset = 
			get_terminal_charset(lookup_charset("cp850"));
	if (!trconfig.output_charset)
		trconfig.output_charset = lookup_charset("iso8859-1");
	if (!trconfig.dos_charset)
		trconfig.dos_charset = lookup_charset("cp850");
	break;
    case CHARSET_LATIN2:
	if (!trconfig.video_mem_charset) 
		trconfig.video_mem_charset = lookup_charset("cp852");
	if (!trconfig.keyb_config_charset)
		trconfig.keyb_config_charset = 
			get_terminal_charset(lookup_charset("cp852"));
	if (!trconfig.output_charset)
		trconfig.output_charset = lookup_charset("iso8859-2");
	if (!trconfig.dos_charset)
		trconfig.dos_charset = lookup_charset("cp852");
	break;
    case CHARSET_LATIN:
	k_printf("charset_latin\n");
	if (!trconfig.video_mem_charset) 
		trconfig.video_mem_charset = lookup_charset("cp437");
	if (!trconfig.keyb_config_charset)
		trconfig.keyb_config_charset = 
			get_terminal_charset(lookup_charset("cp437"));
	if (!trconfig.output_charset)
		trconfig.output_charset = lookup_charset("iso8859-1");
	if (!trconfig.dos_charset)
		trconfig.dos_charset = lookup_charset("cp437");
	break;
    case CHARSET_KOI8:
	if (!trconfig.video_mem_charset) 
		trconfig.video_mem_charset = lookup_charset("cp866");
	if (!trconfig.keyb_config_charset)
		trconfig.keyb_config_charset = 
			get_terminal_charset(lookup_charset("cp866"));
	if (!trconfig.output_charset)
		trconfig.output_charset = lookup_charset("koi8-r");
	if (!trconfig.dos_charset)
		trconfig.dos_charset = lookup_charset("cp866");
	break;
    default:
	if (!trconfig.video_mem_charset) 
		trconfig.video_mem_charset = lookup_charset("cp437");
	if (!trconfig.keyb_config_charset)
		trconfig.keyb_config_charset = 
			get_terminal_charset(lookup_charset("cp437"));
	if (!trconfig.output_charset)
		trconfig.output_charset = lookup_charset("default");
	if (!trconfig.dos_charset)
		trconfig.dos_charset = lookup_charset("cp437");
	break;
    }
    v_printf("video_mem_charset=%s\n", 
	    trconfig.video_mem_charset?(trconfig.video_mem_charset->names)[0]:"<NULL>");
    k_printf("keyb_config_charset=%s\n", 
	    trconfig.keyb_charset?trconfig.keyb_config_charset->names[0]:"<NULL>");
    v_printf("output_charset=%s\n", 
	    trconfig.output_charset?trconfig.output_charset->names[0]:"<NULL>");
    k_printf("paste_charset=%s\n", 
	    trconfig.paste_charset?trconfig.paste_charset->names[0]:"<NULL>");
    k_printf("keyb_charset=%s\n", 
	    trconfig.keyb_charset?trconfig.keyb_charset->names[0]:"<NULL>");
    d_printf("unix_charset=%s\n", 
	    trconfig.unix_charset?trconfig.unix_charset->names[0]:"<NULL>");
    d_printf("dos_charset=%s\n", 
	    trconfig.dos_charset?trconfig.dos_charset->names[0]:"<NULL>");
}

CONSTRUCTOR(static void init(void))
{
	register_config_scrub(config_translate_scrub);
}
