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

#ifndef __DMAREGS_H__
#define __DMAREGS_H__

/* 8237 DMA controllers */
#define IO_DMA1_BASE	0x00	/* 8 bit slave DMA, channels 0..3 */
#define IO_DMA2_BASE	0xC0	/* 16 bit master DMA, ch 4(=slave input)..7 */

/* DMA controller registers */
#define DMA1_CMD_REG		0x08	/* command register (w) */
#define DMA1_STAT_REG		0x08	/* status register (r) */
#define DMA1_REQ_REG            0x09    /* request register (w) */
#define DMA1_MASK_REG		0x0A	/* single-channel mask (w) */
#define DMA1_MODE_REG		0x0B	/* mode register (w) */
#define DMA1_CLEAR_FF_REG	0x0C	/* clear pointer flip-flop (w) */
#define DMA1_TEMP_REG           0x0D    /* Temporary Register (r) */
#define DMA1_RESET_REG		0x0D	/* Master Clear (w) */
#define DMA1_CLR_MASK_REG       0x0E    /* Clear Mask */
#define DMA1_MASK_ALL_REG       0x0F    /* all-channels mask (w) */

#define DMA2_CMD_REG		0xD0	/* command register (w) */
#define DMA2_STAT_REG		0xD0	/* status register (r) */
#define DMA2_REQ_REG            0xD2    /* request register (w) */
#define DMA2_MASK_REG		0xD4	/* single-channel mask (w) */
#define DMA2_MODE_REG		0xD6	/* mode register (w) */
#define DMA2_CLEAR_FF_REG	0xD8	/* clear pointer flip-flop (w) */
#define DMA2_TEMP_REG           0xDA    /* Temporary Register (r) */
#define DMA2_RESET_REG		0xDA	/* Master Clear (w) */
#define DMA2_CLR_MASK_REG       0xDC    /* Clear Mask */
#define DMA2_MASK_ALL_REG       0xDE    /* all-channels mask (w) */

#define DMA1_ADDR_1              0x00    /* DMA address registers */
#define DMA1_ADDR_2              0x02
#define DMA1_ADDR_3              0x04
#define DMA1_ADDR_4              0x06
#define DMA2_ADDR_1              0xC0
#define DMA2_ADDR_2              0xC4
#define DMA2_ADDR_3              0xC8
#define DMA2_ADDR_4              0xCC

#define DMA1_CNT_1               0x01    /* DMA count registers */
#define DMA1_CNT_2               0x03
#define DMA1_CNT_3               0x05
#define DMA1_CNT_4               0x07
#define DMA2_CNT_1               0xC2
#define DMA2_CNT_2               0xC6
#define DMA2_CNT_3               0xCA
#define DMA2_CNT_4               0xCE

#define DMA1_PAGE_1              0x87    /* DMA page registers */
#define DMA1_PAGE_2              0x83
#define DMA1_PAGE_3              0x81
#define DMA1_PAGE_4              0x82
#define DMA2_PAGE_1              0x8F
#define DMA2_PAGE_2              0x8B
#define DMA2_PAGE_3              0x89
#define DMA2_PAGE_4              0x8A

#define DMA_TRANSFER_MODE(m) ((m >> 4) & 3)
enum TRMODE { DEMAND, SINGLE, BLOCK, CASCADE };

#define DMA_TRANSFER_OP(m) (m & 3)
enum TROP { VERIFY, WRITE, READ, INVALID };

#define DMA_ADDR_DEC(m) ((m >> 3) & 1)

#define DMA_AUTOINIT(m) ((m >> 2) & 1)

#endif
