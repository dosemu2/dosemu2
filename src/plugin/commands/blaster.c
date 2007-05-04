/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <string.h>
#include "builtins.h"
#include "msetenv.h"

#include "blaster.h"

#include "emu.h"
#include "sound.h"

int blaster_main(int argc, char **argv) {
	
	if (config.sound) {
		char tmpbuf[30];
		char blaster[255];

		com_printf("Sound is enabled.\n");

		com_printf("SB DSP at 0x%x-0x%x, IRQ=%d, DMA8=%d", config.sb_base,
				config.sb_base+15, config.sb_irq, config.sb_dma);
		if (config.sb_hdma) {
			com_printf(", DMA16=%d", config.sb_hdma);
		}
		com_printf("\n");

		if (config.mpu401_base) {
			com_printf("MPU-401 interface at 0x%x-0x%x\n",
					config.mpu401_base, config.mpu401_base+1);
		}

		snprintf(blaster, sizeof(blaster), "A%x I%d D%d H%d", config.sb_base,
				config.sb_irq, config.sb_dma,
				config.sb_hdma ? : config.sb_dma);

		if (config.mpu401_base) {
			snprintf(tmpbuf, sizeof(tmpbuf), " P%x",
					config.mpu401_base);
			strncat(blaster, tmpbuf, 10);
		}

		strncat(blaster, " T6", 10); /* SB16 */

		if (msetenv("BLASTER", blaster) == -1) {
			com_printf("Environment too small for BLASTER! "
			    "(needed %zu bytes)\n", strlen(blaster));
		}

		snprintf(blaster, sizeof(blaster), "SYNTH:%d MAP:%c MODE:%d",
		    config.mpu401_base ? 2 : 1, 'E', 0);

		if (msetenv("MIDI", blaster) == -1) {
			com_printf("Environment too small for MIDI! (needed %zu bytes)\n", strlen(blaster));
		}

		com_printf("\n");
	}
	else {
		com_printf("Sound not enabled in config!\n");
	}
	return 0;
}

