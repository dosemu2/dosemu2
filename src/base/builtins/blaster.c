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

static void show_help(char *name)
{
	com_printf("%s \t\t - show current sound settings\n", name);
	com_printf("%s /E \t\t - set BLASTER and MIDI environment variables\n",
			name);
	com_printf("%s /S <mode> \t - set midi synth mode: gm or mt32\n",
			name);
	com_printf("%s /H \t\t - this help\n", name);
}

int sound_main(int argc, char **argv)
{
	if (!config.sound) {
		com_printf("Sound not enabled in config!\n");
		return 1;
	}

	if (argc == 1) {
		show_settings();
		return 0;
	}

	if (argv[1][0] == '/') {
		switch (toupper(argv[1][1])) {
		case 'E':
			blaster_setenv();
			break;
		case 'H':
		case '?':
			show_help(argv[0]);
			break;
		case 'S':
			if (argc < 3) {
				com_printf("/S requires parameter\n");
				return 1;
			}
			if (!midi_set_synth_type_from_string(argv[2])) {
				com_printf("%s mode unsupported\n", argv[2]);
				break;
			}
			break;
		default:
			com_printf("Unknown option %s\n", argv[1]);
			return 1;
		}
	}

	return 0;
}

/* for compatibility with dosemu-1 */
int blaster_main(int argc, char **argv)
{
	if (!config.sound) {
		com_printf("Sound not enabled in config!\n");
		return 1;
	}
	blaster_setenv();
	return 0;
}
