/*
 * s3.c
 *
 * This file contains support for VGA-cards with an S3 chip.
 *
 * (C) 1994 Christoph Niemann
 *          niemann@swt.uni-bochum.de
 *
 * The programming information was taken from the file vgadoc3.zip
 * by Finn Thoegersen and from the german computer magazine c't,
 * nov. 1992, p 242.
 */

/*
 * This is ALPHA-quality software.
 *
 * Using this software could be dangerous for You, you hardware or
 * anything near your hardware.
 */

#define S3_C


#include <stdio.h>

#include "config.h"
#include "emu.h"
#include "video.h"

extern int CRT_I, CRT_D;

void (*save_ext_regs) ();
void (*restore_ext_regs) ();
void (*set_bank_read) (u_char bank);
void (*set_bank_write) (u_char bank);
void (*ext_video_port_out) (u_char value, int port);
u_char(*ext_video_port_in) (int port);

static int s3_type = 0;


#define DEB(x) x

static void out_crt(const int index, const int value)
{
	port_out(index, CRT_I);
	port_out(value, CRT_D);
}

static int in_crt(const int index)
{
	port_out(index, CRT_I);
	return (port_in(CRT_D) & 0xff);
}

static void s3_save_ext_regs(u_char xregs[], u_short xregs16[])
{
	DEB(fprintf(stderr, "s3_save_ext_regs(): start.\n");)

	/*
	 * Save status of the extensions enable/disable regs
	 */
	xregs[0] = in_crt(0x38);
	xregs[1] = in_crt(0x39);
	/*
	 * Enable extensions
	 */
	out_crt(0x38, 0x48);
	out_crt(0x39, 0xa5);

	xregs[2] = in_crt(0x0e);	/* hardware cursor fg color */
	xregs[3] = in_crt(0x0f);	/* hardware cursor bg color */

	xregs[4] = in_crt(0x31);	/* display start */

	xregs[5] = in_crt(0x34);
	xregs[6] = in_crt(0x35);	/* active bank number */

	xregs[7] = in_crt(0x3a);

	xregs[8] = in_crt(0x3c);

	xregs[9] = in_crt(0x40);
	xregs[10] = in_crt(0x41);
	xregs[11] = in_crt(0x42);
	xregs[12] = in_crt(0x43);

	xregs[13] = in_crt(0x45);
	xregs[14] = in_crt(0x46);
	xregs[15] = in_crt(0x47);
	xregs[16] = in_crt(0x48);
	xregs[17] = in_crt(0x49);

	xregs[18] = in_crt(0x4c);
	xregs[19] = in_crt(0x4d);
	xregs[20] = in_crt(0x4e);
	xregs[21] = in_crt(0x4f);

	xregs[22] = in_crt(0x55);
	xregs[23] = in_crt(0x58);

	if (s3_type)
	{
		/*
		 * I have an S3-911, so this part is untested
		 */
		DEB(fprintf(stderr,"s3_save_ext_regs(): saving additional regs\n");)
		xregs[24] = in_crt(0x50);
		xregs[25] = in_crt(0x51);

		xregs[26] = in_crt(0x53);

		xregs[27] = in_crt(0x59);
		xregs[28] = in_crt(0x5a);

		xregs[29] = in_crt(0x5e);
	}

	/*
	 * restore the status of the extensions
	 */

	out_crt(0x38, xregs[0]);
	out_crt(0x39, xregs[1]);
}

static void s3_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
        DEB(fprintf(stderr, "s3_restore_ext_regs(): start.\n");)

        /*
         * Enable extensions
         */
        out_crt(0x38, 0x48);
        out_crt(0x39, 0xa5);

	out_crt(0x0e, xregs[2]);
	out_crt(0x0f, xregs[3]);

	out_crt(0x31, xregs[4]);

	out_crt(0x34, xregs[5]);
	out_crt(0x35, xregs[6]);

	out_crt(0x3a, xregs[7]);

	out_crt(0x3c, xregs[8]);

	out_crt(0x40, xregs[9]);
	out_crt(0x41, xregs[10]);
	out_crt(0x42, xregs[11]);
	out_crt(0x43, xregs[12]);

	out_crt(0x45, xregs[13]);
	out_crt(0x46, xregs[14]);
	out_crt(0x47, xregs[15]);
	out_crt(0x48, xregs[16]);
	out_crt(0x49, xregs[17]);

	out_crt(0x4c, xregs[18]);
	out_crt(0x4d, xregs[19]);
	out_crt(0x4e, xregs[20]);
	out_crt(0x4f, xregs[21]);

	out_crt(0x55, xregs[22]);
	out_crt(0x58, xregs[23]);

	if (s3_type)
	{
		/*
		 * I have an S3-911, so this part is untested
		 */
		DEB(fprintf(stderr,"s3_restore_ext_regs(): restoring additional regs\n");)
		out_crt(0x50,xregs[24]);
		out_crt(0x51,xregs[25]);

		out_crt(0x53,xregs[26]);

		out_crt(0x59,xregs[27]);
		out_crt(0x5a,xregs[28]);

		out_crt(0x5e,xregs[29]);
	}

	/*
	 * restore the status of the extensions
	 */
	out_crt(0x38, xregs[0]);
	out_crt(0x39, xregs[1]);
}

static void s3_set_bank(u_char bank)
{
	/*
	 * For S3 chips the memory bank is always the same for
	 * reading and writing-
	 */
	int mode = in_crt(0x38);
	int old_reg;

	DEB(fprintf(stderr, "s3_set_bank(%d)\n", bank);)

	out_crt(0x38, 0x48);
	old_reg = in_crt(0x35);
	out_crt(0x35, (bank & 0x0f) + (old_reg & 0xf0));
	if ((bank & 0xf0) && s3_type)
	{
		/*
		 * This is untested (I don't have > 1 MB VRAM)
		 */
		int mode2 = in_crt(0x39);
		out_crt(0x39, 0xa5);
		old_reg = in_crt(0x51);
		out_crt(0x51, (bank & 0xf0) >> 2);
		out_crt(0x39, mode2);
	}
	out_crt(0x38, mode);
}

static u_char s3_ext_video_port_in(int port)
{
	return (port_in(port) & 0xff);
}

static void s3_ext_video_port_out(u_char value, int port)
{
	port_out(value, port);
}

void vga_init_s3(void)
{
	int mode, chip;
	char *name = NULL;
	save_ext_regs = s3_save_ext_regs;
	restore_ext_regs = s3_restore_ext_regs;
	set_bank_read = s3_set_bank;
	set_bank_write = s3_set_bank;
	ext_video_port_in = s3_ext_video_port_in;
	ext_video_port_out = s3_ext_video_port_out;

	mode = in_crt(0x38);
	out_crt(0x38, 0x48);
	chip = in_crt(0x30);
	out_crt(0x38, mode);

	switch(chip)
	{
		case 0x81:	name = "911";				break;
		case 0x82:	name = "911A/924";			break;
		case 0x90:	name = "928C"; s3_type = 1;		break;
		case 0x91:	name = "928D"; s3_type = 1;		break;
		case 0x94:
		case 0x95:	name = "928E"; s3_type = 1;		break;
		case 0xA0:	name = "801/805A/B"; s3_type = 1;	break;
		case 0xA2:
		case 0xA3:
		case 0xA4:	name = "801/805C"; s3_type = 1;		break;
		case 0xA5:	name = "801/805D"; s3_type = 1;		break;
		case 0xB0:	name = "928PCI"; s3_type = 1;		break;
		default:	fprintf(stderr, "Unknown S3-chip, type = 0x%xd\n", chip);
	}

	if (name)
		fprintf(stderr, "S3 chip 86c%s detected.\n", name);
}

#undef S3_C
