/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Authors: Ryan C. Underwood, Stas Sergeev
 */

#include <string.h>
#include <ctype.h>
#include "builtins.h"
#include "msetenv.h"

#include "blaster.h"

#include "emu.h"
#include "sound.h"
#include "sound/midi.h"

static const char *smode[] = { "gm", "gs", "mt32" };

static int get_mode_num(void)
{
    switch (midi_get_synth_type()) {
    case ST_GM:
	return 0;
/*    case ST_SC:
	return 1;*/
    case ST_MT32:
	return 2;
    default:
	return 0;
    }
}

static void blaster_setenv(void)
{
	char blaster[255];

	snprintf(blaster, sizeof(blaster), "A%x I%d D%d H%d P%x",
			config.sb_base,
			config.sb_irq, config.sb_dma,
			config.sb_hdma ? : config.sb_dma,
			config.mpu401_base);


	strncat(blaster, " T6", 10); /* SB16 */

	com_printf("BLASTER=%s\n", blaster);
	if (msetenv("BLASTER", blaster) == -1) {
		com_printf("Environment too small for BLASTER! "
		    "(needed %zu bytes)\n", strlen(blaster));
	}

	snprintf(blaster, sizeof(blaster), "SYNTH:%d MAP:%c MODE:%d",
	    2, 'E', get_mode_num());

	com_printf("MIDI=%s\n", blaster);
	if (msetenv("MIDI", blaster) == -1) {
		com_printf("Environment too small for MIDI! (needed %zu bytes)\n", strlen(blaster));
	}
}

static void show_settings(void)
{
	com_printf("SB at 0x%x-0x%x, IRQ=%d, DMA8=%d", config.sb_base,
			config.sb_base+15, config.sb_irq, config.sb_dma);
	if (config.sb_hdma) {
		com_printf(", DMA16=%d", config.sb_hdma);
	}

	com_printf(". MPU-401 at 0x%x-0x%x, IRQ=%d.\n",
			config.mpu401_base, config.mpu401_base+1,
			get_mpu401_irq_num());
	com_printf("MIDI synth mode is %s\n", smode[get_mode_num()]);
}

static void show_help(void)
{
	const char *name = "emusound";
	com_printf("%s -c\t\t - show current sound settings\n", name);
	com_printf("%s -e\t\t - set BLASTER and MIDI environment variables\n",
			name);
	com_printf("%s -s <mode> \t - set midi synth mode: gm or mt32\n",
			name);
	com_printf("%s -h \t\t - this help\n", name);
}

int emusound_main(int argc, char **argv)
{
	int c;

	if (!config.sound) {
		com_printf("Sound not enabled in config!\n");
		return 1;
	}

	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "/?"))) {
		show_help();
		com_printf("\nCurrent settings:\n");
		show_settings();
		return 0;
	}

	optind = 0;
	while ((c = getopt(argc, argv, "cehs:")) != -1) {
	    switch (c) {
		case 'c':
			show_settings();
			break;
		case 'e':
			blaster_setenv();
			break;
		case 'h':
			show_help();
			break;
		case 's':
			if (!midi_set_synth_type_from_string(optarg)) {
				com_printf("%s mode unsupported\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		default:
			com_printf("Unknown option\n");
			return EXIT_FAILURE;
		}
	}

	return 0;
}
