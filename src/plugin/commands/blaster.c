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
	    2, 'E', 0);

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
			config.mpu401_irq);
}

static void show_help(char *name)
{
	com_printf("%s - show current sound settings\n", name);
	com_printf("%s /E - set BLASTER and MIDI environment variables\n",
			name);
	com_printf("%s /H - this help\n", name);
}

int blaster_main(int argc, char **argv) {

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
			show_help(argv[0]);
			break;
		default:
			com_printf("Unknown option %s\n", argv[1]);
			return 1;
		}
	}

	return 0;
}
