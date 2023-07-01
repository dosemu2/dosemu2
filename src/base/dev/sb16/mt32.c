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
 * Purpose: MT32 emulation.
 *
 * Author: @stsp
 *
 */
#include "pic.h"
#include "dosemu_debug.h"
#include "sound/midi.h"
#include "iodev.h"
#include "emu.h"
#include "softmpu/export.h"
#include "mpu401.h"

struct mt32state_s {
    struct mpu401_s *mpu;
    unsigned int irq_active:1;
};

static struct mt32state_s mt32;

static void mpu_activate_irq(struct mpu401_s *mpu)
{
    if (mt32.irq_active) {
	S_printf("MT32: Warning: Interrupt already active!\n");
	return;
    }
    S_printf("MT32: Activating irq %d\n", config.mpu401_irq_mt32);
    mt32.irq_active = 1;
    pic_request(config.mpu401_irq_mt32);
}

static void mpu_deactivate_irq(struct mpu401_s *mpu)
{
    S_printf("MT32: Deactivating irq %d\n", config.mpu401_irq_mt32);
    if (!mt32.irq_active) {
	S_printf("MT32: Warning: Interrupt not active!\n");
	return;
    }
    mt32.irq_active = 0;
    pic_untrigger(config.mpu401_irq_mt32);
}

static void mpu_run_irq(struct mpu401_s *mpu)
{
    if (!mt32.irq_active)
	return;
    S_printf("MT32: Run irq %d\n", config.mpu401_irq_mt32);
    pic_untrigger(config.mpu401_irq_mt32);
    pic_request(config.mpu401_irq_mt32);
}

static void mpu_write_midi(struct mpu401_s *mpu, uint8_t data)
{
    if (mpu401_is_uart(mpu))
	midi_write(data, ST_ANY);
    else
	MPU401_WriteData(data);
}

static void mpu_cmd_hook(struct mpu401_s *mpu, uint8_t cmd)
{
    MPU401_WriteCommand(cmd);
}

static void mpu_read_hook(struct mpu401_s *mpu, uint8_t data)
{
    MPU401_ReadData(data);
}

static struct mpu401_ops mops = {
    .activate_irq = mpu_activate_irq,
    .deactivate_irq = mpu_deactivate_irq,
    .run_irq = mpu_run_irq,
    .write_midi = mpu_write_midi,
    .cmd_hook = mpu_cmd_hook,
    .read_hook = mpu_read_hook,
    .name = "MT32 MPU401"
};

void mt32_init(void)
{
    if (config.mpu401_irq_mt32 == 2) {
	error("irq2 for mt32 not supported, using irq 9\n");
	config.mpu401_irq_mt32 = 9;
    }
    mt32.irq_active = 0;
    mt32.mpu = mpu401_init(config.mpu401_base_mt32, &mops);
    MPU401_Init();
    S_printf("MT32: Initialisation completed\n");
}

void mt32_reset(void)
{
    mpu401_reset(mt32.mpu); // this also deactivates irq
}

void mt32_done(void)
{
    mpu401_done(mt32.mpu);
    MPU401_Done();
}

void QueueByte(Bit8u data)
{
    mpu401_put_midi_in_byte(mt32.mpu, data);
}

void ClrQueue(void)
{
    mpu401_clear_midi_in_fifo(mt32.mpu);
}
