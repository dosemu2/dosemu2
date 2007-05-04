/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 ******** OLD_DMA_CODE COPYRIGHT
 * dma.c--dma controller emulater--Joel N. Weber II
 * Copyright (C) 1995  Joel N. Weber II
 * See the file README.dma in this directory for more information
 ********
 *
 * DANG_BEGIN_MODULE dma.c
 * This is the DMA controller emulation. It is fairly complete.
 *
 * maintainer:
 * Alistair MacDonald <alistair@slitesys.demon.co.uk>
 * Stas Sergeev <stsp@users.sourceforge.net>
 *
 * Previous maintainers:
 * David Brauman <crisk@netvision.net.il>
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_REMARK
 * **** WARNING ****
 * This Code _HAS_ changed.
 * DANG_END_REMARK
 *
 * modified 11/05/95 by Michael Beck
 *  some minor bugs fixed in dma_trans() and other places (Old Code - AM)
 *
 * CHANGELOG
 * 23/2/97 - AM - Finally removed the old DMA code, and re-organised the
 *          code to make it easier to read now it has been indented. Added
 *	    in the fixes from David.
 * 17/3/97 - AM - Added in support for the handler to reject the completion.
 * 	    which allows the SB driver to work better, and better handling
 *	    when files "disappear" (EBADF) in some functions.
 *         - AM - Added in the Auto-Init code.
 * 4/7/97  - AM - Merging in Michael Karcher's code (and amending)
 * END_CHANGELOG
 */

#include "emu.h"		/* for h_printf */

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "dma.h"
#include "pic.h"
#include "port.h"

#include <unistd.h>

/* #define EXCESSIVE_DEBUG 1 */

typedef struct {
  Bit8u lsb;
  Bit8u msb;
  Bit8u underflow;
} msblsb_t;

typedef union {
  Bit8u data[3];
  msblsb_t bits;
  /* AM - Removed 'value' since this makes endian-ness assumptions */
} multi_t;

typedef struct {
  int mask;			/* Whether channel is masked */
  int dack;			/* DACK */
  int run;			/* Running */
  int eop;			/* End Of Process */
  long int size;		/* Preferred Transfer Size */
  size_t(*read_handler) (void *, size_t);	/* Handler Function */
  size_t(*write_handler) (void *, size_t);	/* Handler Function */
  void(*DACK_handler) (void);	/* Handler Function */
  void(*EOP_handler) (void);	/* Handler Function */

  multi_t address;		/* Backup address - for Auto-init modes */
  multi_t length;		/* Backup length - for Auto-init modes */
  long int set_size;		/* Backup transfer size - for Auto-init modes */
} internal_t;

typedef struct {
  multi_t address[4];		/* Address for channel */
  multi_t length[4];		/* Length of transfer */
  Bit8u status;			/* Status of Controller */
  Bit8u ch_config[4];		/* Configuration of channel */
  Bit8u ch_mode[4];		/* Mode of the channel */
  Bit8u page[4];		/* Page address for channel */
  int ff;			/* High/Low order bit Flip/Flop */
  int current_channel;		/* Which is being referred to */

/* Emulation Specific */
  internal_t i[4];		/* Internals */
} dma_t;

static dma_t dma[2];		/* DMA controllers */

/*
 * DANG_BEGIN_REMARK
 *
 * The Emulated DMA channels are provided by using files and writes.
 * This means that they are easy to track.
 * It might cause problems when attempting to interface to the REAL DMA
 * controller. (Necessary to talk to hardware which uses DMA.)
 *
 * Note that DMA controller 2 uses word granular addressing and controller 1
 * uses byte granular address ... this simplifies the code !
 *
 * DANG_END_REMARK
 */

#define DMA1  0
#define DMA2  1

inline void dma_toggle_ff(int dma_c);
inline void dma_write_mask(int dma_c, Bit8u value);
inline void dma_write_count(int dma_c, int channel, Bit8u value);
inline void dma_write_addr(int dma_c, int channel, Bit8u value);
inline Bit8u dma_read_count(int dma_c, int channel);
inline Bit8u dma_read_addr(int dma_c, int channel);
inline Bit32u create_addr(Bit8u page, multi_t address, int dma_c);

inline Bit32s get_value(multi_t data);
inline void add_value(multi_t * data, Bit16u value);
inline void sub_value(multi_t * data, Bit16u value);
inline void set_value(multi_t * data, Bit16u value);
inline int has_underflow(multi_t data);
static inline void dma_handle_TC(int controller, int channel);

inline int get_ch(int controller, int channel);
inline int get_mask(int ch);

static inline void activate_channel(int controller, int channel);
static inline void deactivate_channel(int controller, int channel);

/* PUBLIC CODE */

void dma_assert_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Asserting DACK on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].dack = 1;
}

void dma_assert_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Asserting DREQ on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;
  mask = 1 << (ch + 4);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: ... mask = %d, status = %d\n", mask,
	   dma[controller].i[ch].mask);
#endif				/* EXCESSIVE_DEBUG */

  /* 
   * Must assert DREQ before activating the channel, otherwise it ignores the
   * the request - AM
   */

  dma[controller].status |= mask;

  if (!dma[controller].i[ch].mask)
    activate_channel(controller, ch);

}

void dma_drop_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Dropping DACK on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].dack = 0;
}

void dma_drop_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Dropping DREQ on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;
  mask = 1 << (ch + 4);

  is_dma &= ~get_mask(channel);
  dma[controller].status &= ~mask;
}

void dma_assert_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Asserting EOP on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].eop = 1;
}

void dma_drop_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Dropping EOP on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].eop = 0;
}

int dma_test_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Testing DACK on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  return dma[controller].i[ch].dack;
}

int dma_test_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Testing DREQ on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch + 4);

  return dma[controller].status & mask;
}

int dma_test_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Testing EOP on channel %d\n", channel);
#endif				/* EXCESSIVE_DEBUG */

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;

  return dma[controller].i[ch].eop;
}




/* PRIVATE CODE */


/*
 * This removes any Endian-ness assumptions
 */

inline Bit32s get_value(multi_t data)
{
  Bit16u tmp = ((Bit16u) data.bits.msb << 8) | data.bits.lsb;
  return (data.bits.underflow ? -~tmp - 1 : tmp);
}

inline void add_value(multi_t * data, Bit16u value)
{
  Bit16u tmp;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif				/* EXCESSIVE_DEBUG */

  tmp = get_value(*data);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Adding %u to %u gives ", value, tmp);
#endif				/* EXCESSIVE_DEBUG */

  set_value(data, tmp + value);
  if (get_value(*data) < tmp)
    data->bits.underflow = 1;
  else
    data->bits.underflow = 0;

#ifdef EXCESSIVE_DEBUG
  h_printf("%u ", get_value(*data));
  if (data->bits.underflow)
    h_printf("(overflow)");
  h_printf("\n");
  h_printf("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif				/* EXCESSIVE_DEBUG */
}

inline void sub_value(multi_t * data, Bit16u value)
{
  Bit16u tmp;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif				/* EXCESSIVE_DEBUG */

  tmp = get_value(*data);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Subtracting %u from %u gives ", value, tmp);
#endif				/* EXCESSIVE_DEBUG */

  set_value(data, tmp - value);
  if (tmp < value)
    data->bits.underflow = 1;
  else
    data->bits.underflow = 0;

#ifdef EXCESSIVE_DEBUG
  h_printf("%u ", get_value(*data));
  if (data->bits.underflow)
    h_printf("(underflow)");
  h_printf("\n");
  h_printf("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif				/* EXCESSIVE_DEBUG */
}

inline void set_value(multi_t * data, Bit16u value)
{
  data->bits.msb = (Bit8u) (value >> 8);
  data->bits.lsb = (Bit8u) value & 0xFF;
  data->bits.underflow = 0;
}

inline int has_underflow(multi_t data)
{
  return data.bits.underflow;
}

/*
 * This ensures that the controller will be called to service this channel.
 */
static inline void activate_channel(int controller, int channel)
{
  int mask, ch;

  ch = get_ch(controller, channel);

  dma[controller].i[channel].mask = 0;
  /* 
   * We only _actually_ activate the channel if DREQ is set. It's irrelevant
   * otherwise (from an idea by Michael Karcher) - AM
   */

  if (dma_test_DREQ(ch)) {
    mask = get_mask(ch);

    is_dma |= mask;
#ifdef EXCESSIVE_DEBUG
    h_printf("DMA: Channel %d activated. DMA indicator is %d\n", ch,
	     is_dma);
#endif				/* EXCESSIVE_DEBUG */
  }
}

/* 
 * This ensures that the controller is not called for this channel.
 */
static inline void deactivate_channel(int controller, int channel)
{
  int mask, ch;

  dma[controller].i[channel].mask = 1;
  ch = get_ch(controller, channel);
  mask = get_mask(ch);

  is_dma &= ~mask;

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Channel %d deactivated. DMA indicator is %d\n", ch,
	   is_dma);
#endif				/* EXCESSIVE_DEBUG */
}

inline Bit32u create_addr(Bit8u page, multi_t address, int dma_c)
{
  /* Ben Davis modified this so it really actually DOES work. :)
   * Tested with SB16 emulation.
   */

  Bit32u offset;

  offset = ((Bit16u) address.bits.msb << 8) + address.bits.lsb;

  if (dma_c == DMA2)
    offset <<= 1;

  /* Note: my 'PC Intern' book strongly implies that the '& 0xFFFF'
   * operation should not be done. However, several programs don't
   * work unless I do this operation. Such is life. -- BD
   */

  /* Great. FT2 works (sorta) if I don't do & 0xFFFF. ITSB16.MMX
   * works if I DO do & 0xFFFF. GIMME A BREAK!
   * ... very well. Bochs uses |, so we shall too. Seems to work.
   * Now I know why my old QB+ASM SB16 code sometimes made evil
   * noises :( -- BD
   */
  return (((Bit32u) page & 0xF) << 16) | offset;
}

inline void dma_toggle_ff(int dma_c)
{
  dma[dma_c].ff = !dma[dma_c].ff;
}


inline Bit8u dma_read_addr(int dma_c, int channel)
{
  Bit8u r;

  r = dma[dma_c].address[channel].data[dma[dma_c].ff];
  h_printf("DMA: Read %u from Channel %d Address (Byte %d)\n", r,
	   (dma_c * 4) + channel, dma[dma_c].ff);

  dma_toggle_ff(dma_c);

  return r;
}

inline Bit8u dma_read_count(int dma_c, int channel)
{
  Bit8u r;

  r = dma[dma_c].length[channel].data[dma[dma_c].ff];
  h_printf("DMA: Read %u from Channel %d Length (Byte %d)\n", r,
	   (dma_c * 4) + channel, dma[dma_c].ff);

  dma_toggle_ff(dma_c);

  return r;
}

static void dma_recover_values(int controller, int channel)
{
  h_printf("DMA: Recovering settings for Auto-Init transfer on %d, %d\n",
	   controller, channel);
  /* Recover the values from the private store */
  dma[controller].address[channel].data[0]
      = dma[controller].i[channel].address.data[0];
  dma[controller].address[channel].data[1]
      = dma[controller].i[channel].address.data[1];
  dma[controller].address[channel].bits.underflow = 0;
  dma[controller].length[channel].data[0]
      = dma[controller].i[channel].length.data[0];
  dma[controller].length[channel].data[1]
      = dma[controller].i[channel].length.data[1];
  dma[controller].length[channel].bits.underflow = 0;

  dma[controller].i[channel].size = dma[controller].i[channel].set_size;
}

#if 0
/* 
 * Obselete? 
 * Might make a useful public function if converted to just take a single
 * parameter - AM 
 */
static Bit16u length_transferred(int controller, int channel)
{
  return (get_value(dma[controller].i[channel].length)
	  - get_value(dma[controller].length[channel]));
}
#endif

static Bit8u dma_io_read(ioport_t port)
{
  Bit8u r;

  r = 0;

  switch (port) {
  case DMA_ADDR_0:
    r = dma_read_addr(DMA1, 0);
    break;
  case DMA_ADDR_1:
    r = dma_read_addr(DMA1, 1);
    break;
  case DMA_ADDR_2:
    r = dma_read_addr(DMA1, 2);
    break;
  case DMA_ADDR_3:
    r = dma_read_addr(DMA1, 3);
    break;
  case DMA_ADDR_4:
    r = dma_read_addr(DMA2, 0);
    break;
  case DMA_ADDR_5:
    r = dma_read_addr(DMA2, 1);
    break;
  case DMA_ADDR_6:
    r = dma_read_addr(DMA2, 2);
    break;
  case DMA_ADDR_7:
    r = dma_read_addr(DMA2, 3);
    break;

  case DMA_CNT_0:
    r = dma_read_count(DMA1, 0);
    break;
  case DMA_CNT_1:
    r = dma_read_count(DMA1, 1);
    break;
  case DMA_CNT_2:
    r = dma_read_count(DMA1, 2);
    break;
  case DMA_CNT_3:
    r = dma_read_count(DMA1, 3);
    break;
  case DMA_CNT_4:
    r = dma_read_count(DMA2, 0);
    break;
  case DMA_CNT_5:
    r = dma_read_count(DMA2, 1);
    break;
  case DMA_CNT_6:
    r = dma_read_count(DMA2, 2);
    break;
  case DMA_CNT_7:
    r = dma_read_count(DMA2, 3);
    break;

  case DMA_PAGE_0:
    r = dma[DMA1].page[0];
    h_printf("DMA: Read %u from Channel 0 Page\n", r);
    break;
  case DMA_PAGE_1:
    r = dma[DMA1].page[1];
    h_printf("DMA: Read %u from Channel 1 Page\n", r);
    break;
  case DMA_PAGE_2:
    r = dma[DMA1].page[2];
    h_printf("DMA: Read %u from Channel 2 Page\n", r);
    break;
  case DMA_PAGE_3:
    r = dma[DMA1].page[3];
    h_printf("DMA: Read %u from Channel 3 Page\n", r);
    break;
  case DMA_PAGE_4:
    r = dma[DMA2].page[0];
    h_printf("DMA: Read %u from Channel 4 Page\n", r);
    break;
  case DMA_PAGE_5:
    r = dma[DMA2].page[1];
    h_printf("DMA: Read %u from Channel 5 Page\n", r);
    break;
  case DMA_PAGE_6:
    r = dma[DMA2].page[2];
    h_printf("DMA: Read %u from Channel 6 Page\n", r);
    break;
  case DMA_PAGE_7:
    r = dma[DMA2].page[3];
    h_printf("DMA: Read %u from Channel 7 Page\n", r);
    break;

  case DMA1_STAT_REG:
    r = dma[DMA1].status;
    h_printf("DMA: Read %u from DMA1 Status\n", r);
    dma[DMA1].status &= 0xf0;	/* clear status bits */
    break;
  case DMA2_STAT_REG:
    r = dma[DMA2].status;
    h_printf("DMA: Read %u from DMA2 Status\n", r);
    dma[DMA2].status &= 0xf0;	/* clear status bits */
    break;

  default:
    h_printf("DMA: Unhandled Read on 0x%04x\n", (Bit16u) port);
    r = 0xff;
  }

  return r;
}


inline void dma_write_addr(int dma_c, int channel, Bit8u value)
{
  dma[dma_c].address[channel].data[dma[dma_c].ff] = value;
  dma[dma_c].i[channel].address.data[dma[dma_c].ff] = value;	/* autoinit */
  dma[dma_c].address[channel].bits.underflow = 0;
  dma[dma_c].i[channel].address.bits.underflow = 0;

  h_printf("DMA: Wrote 0x%x into Channel %d Address (Byte %d)\n", value,
	   (dma_c * 4) + channel, dma[dma_c].ff);

  dma_toggle_ff(dma_c);
}

inline void dma_write_count(int dma_c, int channel, Bit8u value)
{
  dma[dma_c].length[channel].data[dma[dma_c].ff] = value;
  dma[dma_c].i[channel].length.data[dma[dma_c].ff] = value;	/* autoinit */
  dma[dma_c].length[channel].bits.underflow = 0;
  dma[dma_c].i[channel].length.bits.underflow = 0;

  h_printf("DMA: Wrote 0x%x into Channel %d Length (Byte %d)\n", value,
	   (dma_c * 4) + channel, dma[dma_c].ff);
  if (dma[dma_c].ff) {
    h_printf("DMA: Block size set to %d\n",
	     dma_get_block_size(get_ch(dma_c, channel)));
  }

  dma_toggle_ff(dma_c);
}

inline int dma_get_block_size(int channel)
{
  int controller, ch;

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;
  return (get_value(dma[controller].i[ch].length) + 1);
}

int dma_units_left(int channel) /* units are bytes or words */
{
  int controller, ch;

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;
  return get_value(dma[controller].length[ch]) + 1;
}

inline int dma_get_transfer_size(int channel)
{
  int controller, ch;

  controller = (channel & 4) >> 2;
  ch = channel & DMA_CH_SELECT;
  /* trick, actual transfer size is .size, not .set_size */
  return dma[controller].i[ch].set_size;
}

inline void dma_write_mask(int dma_c, Bit8u value)
{
  if (value & DMA_SELECT_BIT) {
    dma[dma_c].current_channel = (int) value & 3;

    deactivate_channel(dma_c, dma[dma_c].current_channel);	/* - Karcher */

    h_printf("DMA: Channel %u selected.\n",
	     (dma_c * 4) + dma[dma_c].current_channel);
    /* Clear the COMPLETE flag for this channel */
    dma[dma_c].status &= ~(1 << (value & 3));
  } else {
    h_printf("DMA: Channel %u deselected.\n", (dma_c * 4) + (value & 3));

    activate_channel(dma_c, value & 3);
    dma[dma_c].current_channel = -1;

    dma_run();
  }
}




static void dma_io_write(ioport_t port, Bit8u value)
{
  Bit8u ch;

  switch (port) {
  case DMA_ADDR_0:
    dma_write_addr(DMA1, 0, value);
    break;
  case DMA_ADDR_1:
    dma_write_addr(DMA1, 1, value);
    break;
  case DMA_ADDR_2:
    dma_write_addr(DMA1, 2, value);
    break;
  case DMA_ADDR_3:
    dma_write_addr(DMA1, 3, value);
    break;
  case DMA_ADDR_4:
    dma_write_addr(DMA2, 0, value);
    h_printf("DMA: That last write was a little dubious ....\n");
    break;
  case DMA_ADDR_5:
    dma_write_addr(DMA2, 1, value);
    break;
  case DMA_ADDR_6:
    dma_write_addr(DMA2, 2, value);
    break;
  case DMA_ADDR_7:
    dma_write_addr(DMA2, 3, value);
    break;
  case DMA_CNT_0:
    dma_write_count(DMA1, 0, value);
    break;
  case DMA_CNT_1:
    dma_write_count(DMA1, 1, value);
    break;
  case DMA_CNT_2:
    dma_write_count(DMA1, 2, value);
    break;
  case DMA_CNT_3:
    dma_write_count(DMA1, 3, value);
    break;
  case DMA_CNT_4:
    dma_write_count(DMA2, 0, value);
    break;
  case DMA_CNT_5:
    dma_write_count(DMA2, 1, value);
    break;
  case DMA_CNT_6:
    dma_write_count(DMA2, 2, value);
    break;
  case DMA_CNT_7:
    dma_write_count(DMA2, 3, value);
    break;
  case DMA_PAGE_0:
    dma[DMA1].page[0] = value;
    h_printf("DMA: Write %u to Channel0 (Page)\n", value);
    break;
  case DMA_PAGE_1:
    dma[DMA1].page[1] = value;
    h_printf("DMA: Write %u to Channel1 (Page)\n", value);
    break;
  case DMA_PAGE_2:
    dma[DMA1].page[2] = value;
    h_printf("DMA: Write %u to Channel2 (Page)\n", value);
    break;
  case DMA_PAGE_3:
    dma[DMA1].page[3] = value;
    h_printf("DMA: Write %u to Channel3 (Page)\n", value);
    break;
  case DMA_PAGE_4:
    dma[DMA2].page[0] = value;
    h_printf("DMA: Write %u to Channel4 (Page)\n", value);
    break;
  case DMA_PAGE_5:
    dma[DMA2].page[1] = value;
    h_printf("DMA: Write %u to Channel5 (Page)\n", value);
    break;
  case DMA_PAGE_6:
    dma[DMA2].page[2] = value;
    h_printf("DMA: Write %u to Channel6 (Page)\n", value);
    break;
  case DMA_PAGE_7:
    dma[DMA2].page[3] = value;
    h_printf("DMA: Write %u to Channel7 (Page)\n", value);
    break;

  case DMA1_MASK_REG:
    dma_write_mask(DMA1, value);
    break;
  case DMA2_MASK_REG:
    dma_write_mask(DMA2, value);
    break;

  case DMA1_MODE_REG:
    ch = value & DMA_CH_SELECT;
    dma[DMA1].ch_mode[ch] = value - ch;
    h_printf("DMA: Write 0x%x to Channel %u mode\n", value - ch, ch);
    break;
  case DMA2_MODE_REG:
    ch = value & DMA_CH_SELECT;
    dma[DMA2].ch_mode[ch] = value - ch;
    h_printf("DMA: Write 0x%x to Channel %u mode\n", value - ch, ch + 4);
    break;

  case DMA1_CMD_REG:
    dma[DMA1].ch_config[dma[DMA1].current_channel] = value;;
    h_printf("DMA: Write 0x%x to DMA1 Command\n", value);
    break;
  case DMA2_CMD_REG:
    dma[DMA2].ch_config[dma[DMA2].current_channel] = value;;
    h_printf("DMA: Write 0x%x to DMA2 Command\n", value);
    break;

  case DMA1_CLEAR_FF_REG:
    h_printf("DMA: Clearing DMA1 Output FF\n");
    /* Kernel implies this, then ignores it */
    /* dma[DMA1].ff = value & 1; */
    dma[DMA1].ff = 0;
    break;
  case DMA2_CLEAR_FF_REG:
    h_printf("DMA: Clearing DMA2 Output FF\n");
    /* Kernel implies this, then ignores it */
    /* dma[DMA2].ff = value & 1; */
    dma[DMA2].ff = 0;
    break;

  default:
    h_printf("DMA: Unhandled Write on 0x%04x\n", (Bit16u) port);
  }
}



void dma_install_handler(int ch, size_t(*read_handler)(void *, size_t),
    size_t(*write_handler)(void *, size_t), void(*DACK_handler)(void),
    void(*EOP_handler)(void))
{
  int channel, dma_c;

  h_printf("DMA: Installing DMA Handler on channel %d\n", ch);

  dma_c = (ch & 4) >> 2;
  channel = ch & DMA_CH_SELECT;

  dma[dma_c].i[channel].read_handler = read_handler;
  dma[dma_c].i[channel].write_handler = write_handler;
  dma[dma_c].i[channel].DACK_handler = DACK_handler;
  dma[dma_c].i[channel].EOP_handler = EOP_handler;
}

inline int get_ch(int controller, int channel)
{
  return (controller << 2) + channel;
}

inline int get_mask(int ch)
{
  return (1 << ch);
}

static inline void dma_handle_TC(int controller, int channel)
{
  h_printf("DMA: Transfer is complete\n");
  if (dma[controller].ch_mode[channel] & DMA_AUTO_INIT) {
    /* Auto-Init means re-start, so we re-start not stop */
    dma_recover_values(controller, channel);
  } else {
    dma_set_transfer_size(get_ch(controller, channel), 0);
    dma[controller].status |= (1 << channel);
    deactivate_channel(controller, channel);
    dma_drop_DACK(get_ch(controller, channel));
  }
  if (dma[controller].i[channel].EOP_handler)
    dma[controller].i[channel].EOP_handler();
}

static inline size_t dma_do_read(int controller, int channel, void *ptr,
				 size_t size)
{
  if (dma[controller].i[channel].write_handler)
    return dma[controller].i[channel].write_handler(ptr, size);
  else {
    h_printf
	("DMA: ERROR: write handler not installed, can't do transfer!\n");
    return 0;
  }
}

static inline size_t dma_do_write(int controller, int channel, void *ptr,
				  size_t size)
{
  if (dma[controller].i[channel].read_handler)
    return dma[controller].i[channel].read_handler(ptr, size);
  else {
    h_printf
	("DMA: ERROR: read handler not installed, can't do transfer!\n");
    return 0;
  }
}

static void dma_process_demand_mode_write(int controller, int channel)
{
  uintptr_t target_addr;
  int amount_done = 0;
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  target_addr = create_addr(dma[controller].page[channel],
			    dma[controller].address[channel], controller);
#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Demand Mode Write run (%d) target: %u\n", ch,
	   target_addr);
#endif				/* EXCESSIVE_DEBUG */

  if (dma_test_DREQ(ch)) {
    dma[controller].i[channel].run = 1;	/* TRIVIAL */

    dma_drop_DREQ(ch);

    if (dma_units_left(ch) < dma_get_transfer_size(ch)) {
      dma[controller].i[channel].size = dma_units_left(ch);
    }

    amount_done = dma_do_write(controller, channel, (void *) target_addr,
			       dma[controller].i[channel].
			       size << controller);

    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
      sub_value(&dma[controller].address[channel], amount_done);
    } else {
      add_value(&dma[controller].address[channel], amount_done);
    }

    sub_value(&dma[controller].length[channel], amount_done);
  } else
    dma[controller].i[channel].run = 0;

  if (has_underflow(dma[controller].length[channel])) {
    /* Transfer is complete */
    dma_handle_TC(controller, channel);
  }
}

static void dma_process_single_mode_write(int controller, int channel)
{
  uintptr_t target_addr;
  int amount_done = 0;
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  target_addr = create_addr(dma[controller].page[channel],
			    dma[controller].address[channel], controller);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: Single Mode Write run (%d) target: %u\n", ch,
	   target_addr);
#endif				/* EXCESSIVE_DEBUG */

  if (dma_test_DREQ(ch)) {
    dma[controller].i[channel].run = 1;

    dma_drop_DREQ(ch);

    if (dma_units_left(ch) < dma_get_transfer_size(ch)) {
      dma[controller].i[channel].size = dma_units_left(ch);
    }

    amount_done = dma_do_write(controller, channel, (void *) target_addr,
			       dma[controller].i[channel].
			       size << controller);

    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
      sub_value(&dma[controller].address[channel], amount_done);
    } else {
      add_value(&dma[controller].address[channel], amount_done);
    }

    sub_value(&dma[controller].length[channel], amount_done);

    dma[controller].i[channel].run = 0;
  }

  if (has_underflow(dma[controller].length[channel])) {
    /* Transfer is complete */
    dma_handle_TC(controller, channel);
  }
}


static void dma_process_block_mode_write(int controller, int channel)
{
  uintptr_t target_addr;
  int amount_done = 0;
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  target_addr = create_addr(dma[controller].page[channel],
			    dma[controller].address[channel], controller);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: block Mode Write run (%d) target: %u\n", ch, target_addr);
#endif				/* EXCESSIVE_DEBUG */

  if ((!dma[controller].i[channel].run) && (dma_test_DREQ(ch)))
    dma[controller].i[channel].run = 1;	/* Important ! */

  if (dma[controller].i[channel].run) {

    dma_drop_DREQ(ch);

    if (dma_units_left(ch) < dma_get_transfer_size(ch)) {
      dma[controller].i[channel].size = dma_units_left(ch);
    }

    amount_done = dma_do_write(controller, channel, (void *) target_addr,
			       dma[controller].i[channel].
			       size << controller);

    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
      sub_value(&dma[controller].address[channel], amount_done);
    } else {
      add_value(&dma[controller].address[channel], amount_done);
    }

    sub_value(&dma[controller].length[channel], amount_done);
  }

  if (has_underflow(dma[controller].length[channel])) {
    /* Transfer is complete */
    dma_handle_TC(controller, channel);
  }
}


/* 
 * Cascade mode Writes are not supported - Unimportant
 */
static void dma_process_cascade_mode_write(int controller, int channel)
{
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  h_printf("DMA: Attempt to use unsupported CASCADE mode\n");

  deactivate_channel(controller, channel);
  /*    is_dma &= ~mask; */
}


static void dma_process_demand_mode_read(int controller, int channel)
{
  uintptr_t target_addr;
  int amount_done = 0;
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  target_addr = create_addr(dma[controller].page[channel],
			    dma[controller].address[channel], controller);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: demand Mode read run (%d) target: %u\n", ch, target_addr);
#endif				/* EXCESSIVE_DEBUG */

  if (dma_test_DREQ(ch)) {
    dma[controller].i[channel].run = 1;	/* TRIVIAL */

    dma_drop_DREQ(ch);

    if (dma_units_left(ch) < dma_get_transfer_size(ch)) {
      dma[controller].i[channel].size = dma_units_left(ch);
    }

    amount_done = dma_do_read(controller, channel, (void *) target_addr,
			      dma[controller].i[channel].
			      size << controller);

    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
      sub_value(&dma[controller].address[channel], amount_done);
    } else {
      add_value(&dma[controller].address[channel], amount_done);
    }

    sub_value(&dma[controller].length[channel], amount_done);
  } else
    dma[controller].i[channel].run = 0;

  if (has_underflow(dma[controller].length[channel])) {
    /* Transfer is complete */
    dma_handle_TC(controller, channel);
  }
}


static void dma_process_single_mode_read(int controller, int channel)
{
  uintptr_t target_addr;
  size_t amount_done = 0;
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  target_addr = create_addr(dma[controller].page[channel],
			    dma[controller].address[channel], controller);

  h_printf("DMA: Single Mode Read - length %d (%ld)\n",
	   dma_units_left(ch), dma[controller].i[channel].size);

  if (dma_test_DREQ(ch)) {
    dma[controller].i[channel].run = 1;

    dma_drop_DREQ(ch);

    if (dma_units_left(ch) < dma_get_transfer_size(ch)) {
      dma[controller].i[channel].size = dma_units_left(ch);
    }

    amount_done = dma_do_read(controller, channel, (void *) target_addr,
			      dma[controller].i[channel].
			      size << controller);

    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
      sub_value(&dma[controller].address[channel], (Bit16u) amount_done);
    } else {
      add_value(&dma[controller].address[channel], amount_done);
    }

    sub_value(&dma[controller].length[channel], amount_done);

    dma[controller].i[channel].run = 0;
  }

  if (has_underflow(dma[controller].length[channel])) {
    /* Transfer is complete */
    dma_handle_TC(controller, channel);
  }

  h_printf("DMA: [crisk] DMA single read end trace\n");
}



static void dma_process_block_mode_read(int controller, int channel)
{
  uintptr_t target_addr;
  int amount_done = 0;
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  target_addr = create_addr(dma[controller].page[channel],
			    dma[controller].address[channel], controller);

#ifdef EXCESSIVE_DEBUG
  h_printf("DMA: block Mode read run (%d) target: %u\n", ch, target_addr);
#endif				/* EXCESSIVE_DEBUG */

  if ((!dma[controller].i[channel].run) && (dma_test_DREQ(ch)))
    dma[controller].i[channel].run = 1;	/* Essential ! */

  if (dma[controller].i[channel].run) {

    dma_drop_DREQ(ch);

    if (dma_units_left(ch) < dma_get_transfer_size(ch)) {
      dma[controller].i[channel].size = dma_units_left(ch);
    }

    amount_done = dma_do_read(controller, channel, (void *) target_addr,
			      dma[controller].i[channel].
			      size << controller);

    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
      sub_value(&dma[controller].address[channel], amount_done);
    } else {
      add_value(&dma[controller].address[channel], amount_done);
    }

    sub_value(&dma[controller].length[channel], amount_done);
  }

  if (has_underflow(dma[controller].length[channel])) {
    /* Transfer is complete */
    dma_handle_TC(controller, channel);
  }
}


/* 
 * DANG_FIXTHIS: Cascade Mode Reads are not supported 
 */
static void dma_process_cascade_mode_read(int controller, int channel)
{
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  h_printf("DMA: Attempt to use unsupported CASCADE mode\n");

  /*is_dma &= ~mask; */
  deactivate_channel(controller, channel);
}

/* 
 * DANG_FIXTHIS: The Verify Mode is not supported 
 */
static void dma_process_verify_mode(int controller, int channel)
{
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  h_printf
      ("DMA: Attempt to use unsupported VERIFY direction by controller %d, channel %d\n",
       controller + 1, channel);

  deactivate_channel(controller, channel);
  /*is_dma &= ~mask; */
}


/* 
 * DANG_FIXTHIS: The Invalid Mode is not supported (!) 
 */
static void dma_process_invalid_mode(int controller, int channel)
{
  int ch;
  /*      int mask; */

  ch = get_ch(controller, channel);
  /*      mask = get_mask (ch); */

  h_printf
      ("DMA: Attempt to use INVALID direction by controller %d, channel %d\n",
       controller + 1, channel);

  deactivate_channel(controller, channel);
  /*is_dma &= ~mask; */
}



static int dma_process_channel(int controller, int channel)
{
  h_printf("DMA: processing controller %d, channel %d\n",
	   controller + 1, channel);

  dma_assert_DACK(get_ch(controller, channel));
  if (dma[controller].i[channel].DACK_handler)
    dma[controller].i[channel].DACK_handler();

  /* 
   * Telling the DMA controller to READ means that you 
   * want to read from the address, so we actually need 
   * to write !
   */

  switch (dma[controller].ch_mode[channel] & DMA_DIR_MASK) {
  case DMA_WRITE:
    switch (dma[controller].ch_mode[channel] & DMA_MODE_MASK) {
    case DMA_DEMAND_MODE:
      dma_process_demand_mode_write(controller, channel);
      break;

    case DMA_SINGLE_MODE:
      dma_process_single_mode_write(controller, channel);
      break;

    case DMA_BLOCK_MODE:
      dma_process_block_mode_write(controller, channel);
      break;

    case DMA_CASCADE_MODE:
      dma_process_cascade_mode_write(controller, channel);
      break;
    };
    break;

  case DMA_READ:

    switch (dma[controller].ch_mode[channel] & DMA_MODE_MASK) {
    case DMA_DEMAND_MODE:
      dma_process_demand_mode_read(controller, channel);
      break;

    case DMA_SINGLE_MODE:
      dma_process_single_mode_read(controller, channel);
      break;

    case DMA_BLOCK_MODE:
      dma_process_block_mode_read(controller, channel);
      break;

    case DMA_CASCADE_MODE:
      dma_process_cascade_mode_read(controller, channel);
      break;
    };
    break;

  case DMA_VERIFY:
    dma_process_verify_mode(controller, channel);
    break;

  case DMA_INVALID:
    dma_process_invalid_mode(controller, channel);
    break;

  };

  return 0;
}

/* Karcher */
void dma_set_transfer_size(int ch, long int size)
{
  int channel, dma_c;

  h_printf
      ("DMA: Changing DMA block-size on channel %d [%ld bytes/call]\n",
       ch, size);

  dma_c = (ch & 4) >> 2;
  channel = ch & DMA_CH_SELECT;

  dma[dma_c].i[channel].size = size;
  dma[dma_c].i[channel].set_size = size;	/* For auto-init - AM */
}


/*
 * This is surrounds the brains of the operation ....
 */

void dma_controller(void)
{
  Bit8u test;
  int controller, channel;

  for (test = 1, controller = DMA1, channel = 0; test > 0 && test <= 128;
       test = test << 1, channel++) {
    if (channel == 4) {
      controller = DMA2;
      channel = 0;
    }
#ifdef EXCESSIVE_DEBUG
    h_printf("DMA: controller: (%d) run - %s, current - %s\n",
	     (controller * 4) + channel,
	     (is_dma & test) ? "true" : "false",
	     (dma[controller].current_channel ==
	      channel) ? "true" : "false");
#endif				/* EXCESSIVE_DEBUG */

    /* Process the channel only if it has been deselected */
    if ((is_dma & test)
	&& (dma[controller].current_channel != channel)) {
      /* Time to process this DMA channel */
      dma_process_channel(controller, channel);
    }
  }
}

void dma_init(void)
{
  int i, j;
  emu_iodev_t io_device;

  if (config.sound == 2) {
    dma_new_init();
    return;
  }

  /* 8237 DMA controller */
  io_device.read_portb = dma_io_read;
  io_device.write_portb = dma_io_write;
  io_device.read_portw = NULL;
  io_device.write_portw = NULL;
  io_device.read_portd = NULL;
  io_device.write_portd = NULL;
  io_device.irq = EMU_NO_IRQ;
  io_device.fd = -1;

  /*
   * XT Controller 
   */
  io_device.start_addr = 0x0000;
  io_device.end_addr = 0x000F;
  io_device.handler_name = "DMA - XT Controller";
  port_register_handler(io_device, 0);

  /*
   * Page Registers (XT)
   */
  io_device.start_addr = 0x0081;
  io_device.end_addr = 0x0087;
  io_device.handler_name = "DMA - XT Pages";
  port_register_handler(io_device, 0);

  /*
   * AT Controller
   */
  io_device.start_addr = 0x00C0;
  io_device.end_addr = 0x00DE;
  io_device.handler_name = "DMA - AT Controller";
  port_register_handler(io_device, 0);

  /*
   * Page Registers (AT)
   */
  io_device.start_addr = 0x0089;
  io_device.end_addr = 0x008F;
  io_device.handler_name = "DMA - AT Pages";
  port_register_handler(io_device, 0);

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 4; j++) {
      set_value(&dma[i].address[j], 0);
      set_value(&dma[i].length[j], 0);
      dma[i].ch_config[j] = 0;
      dma[i].page[j] = 0;

      dma[i].i[j].read_handler = NULL;
      dma[i].i[j].write_handler = NULL;

      dma[i].i[j].size = MAX_DMA_TRANSFERSIZE;
      dma[i].i[j].set_size = MAX_DMA_TRANSFERSIZE;

      dma[i].i[j].run = 0;
    }
    dma[i].status = 0;
    dma[i].ff = 0;
    dma[i].current_channel = -1;
  }

  is_dma = 0;

  h_printf("DMA: DMA Controller initialized - 8 & 16 bit modes\n");
}


void dma_reset(void)
{
  int i, j;
  if (config.sound == 2) {
    dma_new_reset();
    return;
  }
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 4; j++) {
      set_value(&dma[i].address[j], 0);
      set_value(&dma[i].length[j], 0);
      dma[i].ch_config[j] = 0;
      dma[i].page[j] = 0;

      dma[i].i[j].size = MAX_DMA_TRANSFERSIZE;
      dma[i].i[j].set_size = MAX_DMA_TRANSFERSIZE;

      dma[i].i[j].mask = 1;

      dma[i].i[j].run = 0;
    }
    dma[i].status = 0;
    dma[i].ff = 0;
    dma[i].current_channel = -1;
  }

  is_dma = 0;

  h_printf("DMA: DMA Controller Reset - 8 & 16 bit modes\n");

}
