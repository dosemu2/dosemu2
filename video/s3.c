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
 * Using this software could be dangerous for You, your hardware or
 * anything near your hardware.
 */

#define S3_C


#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "emu.h"
#include "port.h"
#include "video.h"
#include "vga.h"
#include "s3.h"

static int s3_type = 0;
static int s3_memsize = 0;
int s3_8514_base = 0;

#define BASE_8514_1	0x2e8
#define BASE_8514_2	0x148

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

#define S + 256

static short s3_crt_regs[0x60] = {
/*	 0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
/* 0 */	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/* 1 */	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/* 2 */	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/* 3 */	-1,  4, -1, -1,  5,  6, -1, -1,  0,  1,  7, -1,  8, -1, -1, -1,
/* 4 */	 9, 10, 11, 12, -1, 13, 14, 15, 16, 17, -1, -1, 18, 19, 20, 21,
/* 5 */	24 S,25 S, -1,26 S, -1, 22, -1, -1, 23,27 S,28 S, -1, -1, -1,29 S, -1 };

#undef S

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

	xregs[2] = port_in(0x102);
	port_out(1, 0x102);
	iopl(3);
	xregs16[0] = port_in_w(0x8000 | s3_8514_base);	/* CUR_Y */
	xregs16[1] = port_in_w(0x8400 | s3_8514_base);	/* CUR_X */
	xregs16[2] = port_in_w(0x8800 | s3_8514_base);
	xregs16[3] = port_in_w(0x8c00 | s3_8514_base);
	iopl(0);
	port_out(xregs[2], 0x102);

}

static void s3_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
        DEB(fprintf(stderr, "s3_restore_ext_regs(): start.\n");)

        /*
         * Enable extensions
         */
        out_crt(0x38, 0x48);
        out_crt(0x39, 0xa5);

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

	port_out(1, 0x102);
	iopl(3);
	port_out_w(xregs16[0], 0x8000 | s3_8514_base);
	port_out_w(xregs16[1], 0x8400 | s3_8514_base);
	port_out_w(xregs16[2], 0x8800 | s3_8514_base);
	port_out_w(xregs16[3], 0x8c00 | s3_8514_base);
	iopl(0);
	port_out(xregs[2], 0x102);
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
	int x_reg;

	switch(port)
	{
	case CRT_DC:
	case CRT_DM:
		if (dosemu_regs.regs[CRTI] < 0x60 &&
				(x_reg = s3_crt_regs[dosemu_regs.regs[CRTI]]) != -1)
		{
			/*
			 * some registers are available only on `advanced'
			 * s3 cards.
			 */
			if ((x_reg & 0xff) && !s3_type)
				break;
			x_reg &= 0xff;
			v_printf("S3: Read on CRT_D 0x%02x got 0x%02x\n",
					dosemu_regs.regs[CRTI],
					dosemu_regs.xregs[x_reg]);
			return dosemu_regs.xregs[x_reg];
		}
		break;
	}
	v_printf("S3: Bad Read on port 0x%04x\n", port);
	return 0;
}

static void s3_ext_video_port_out(u_char value, int port)
{
	int x_reg;

	switch(port & 0x02ff)
	{
	case CRT_DC:
	case CRT_DM:
		if (dosemu_regs.regs[CRTI] < 0x60 &&
				(x_reg = s3_crt_regs[dosemu_regs.regs[CRTI]]) != -1)
		{
			/*
			 * some registers are available only on `advanced'
			 * s3 cards.
			 */
			if ((x_reg & 0xff) && !s3_type)
				break;
			x_reg &= 0xff;
			v_printf("S3: Write on CRT_D 0x%02x of 0x%02x\n",
					dosemu_regs.regs[CRTI], value);
			dosemu_regs.xregs[x_reg] = value;
			return;
		}
		break;
	case BASE_8514_1:
	case BASE_8514_2:
		break;
		switch(port & 0xfc00)
		{
			case 0x8000:	*((unsigned char *) (dosemu_regs.xregs16)) = value;
					v_printf("S3: Write on 8514 reg 0x%04x of 0x%02x\n",
							port, value);
					break;
			case 0x8400:	*((unsigned char *) (&dosemu_regs.xregs16[1])) = value;
					v_printf("S3: Write on 8514 reg 0x%04x of 0x%02x\n",
							port, value);
					break;
			case 0x8800:	*((unsigned char *) (&dosemu_regs.xregs16[2])) = value;
					v_printf("S3: Write on 8514 reg 0x%04x of 0x%02x\n",
							port, value);
					break;
			case 0x8c00:	*((unsigned char *) (&dosemu_regs.xregs16[3])) = value;
					v_printf("S3: Write on 8514 reg 0x%04x of 0x%02x\n",
							port, value);
					break;
		}
		break;
	case BASE_8514_1 + 1:
	case BASE_8514_2 + 1:
		break;
	}
	v_printf("S3: Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

void vga_init_s3(void)
{
	int mode, mode2, chip;
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

	mode2 = in_crt(0x39);
        out_crt(0x39, 0xa5);
	if (in_crt(0x43) & 0x10)
		s3_8514_base = BASE_8514_2;
	else
		s3_8514_base = BASE_8514_1;
	out_crt(0x39, mode2);

	if (in_crt(0x36) & 0x20)
		s3_memsize = 512;
	else
	{
		if (s3_type) switch((in_crt(0x36) & 0xc0) >> 6)
		{
			case 0:	s3_memsize = 4096; break;
			case 1: s3_memsize = 3072; break;
			case 2: s3_memsize = 2048; break;
			case 3: s3_memsize = 1024; break;
		}
		else
			s3_memsize = 1024;
	}

	fprintf(stderr, "S3 base address: 0x%x\n", s3_8514_base);
	fprintf(stderr, "S3 memory size : %d kbyte\n", s3_memsize);

	out_crt(0x38, mode);
}

#undef S3_C
