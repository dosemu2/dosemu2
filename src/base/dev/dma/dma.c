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
 * END_CHANGELOG
 */

#include "emu.h" /* for h_printf */

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "dma.h"
#include "pic.h"
#include "port.h"

#include <unistd.h>

/* AM - Inverted order of msb/lsb as CRISK found they were inverted. */
typedef struct {
  Bit8u msb;
  Bit8u lsb;
} msblsb_t;

typedef union {
  Bit8u data[2];
  msblsb_t bits;
  /* AM - Removed 'value' since this makes endian-ness assumptions */
} multi_t;

typedef struct {
  int     wfd;           /* Write part of pipe */
  int     rfd;           /* Read part of pipe */
  int     dack;          /* DACK */
  int     run;           /* Running */
  int     eop;           /* End Of Process */
  int     size;          /* Preferred Transfer Size */
  int     (* handler)(int);  /* Handler Function */
} internal_t;

typedef struct {
  multi_t address[4];    /* Address for channel */
  multi_t length[4];     /* Length of transfer */
  Bit8u   status;        /* Status of Controller */
  Bit8u   ch_config[4];  /* Configuration of channel */
  Bit8u   ch_mode[4];    /* Mode of the channel */
  Bit8u   page[4];       /* Page address for channel */
  int     ff;            /* High/Low order bit Flip/Flop */
  int current_channel;	 /* Which is being referred to */

/* Emulation Specific */
  internal_t i[4];       /* Internals */
} dma_t;

static  dma_t dma[2];    /* DMA controllers */

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
inline void dma_write_mask (int dma_c, Bit8u value);
inline void dma_write_count (int dma_c, int channel, Bit8u value);
inline void dma_write_addr (int dma_c, int channel, Bit8u value);
inline Bit8u dma_read_count (int dma_c, int channel);
inline Bit8u dma_read_addr (int dma_c, int channel);
inline Bit32u create_addr(Bit8u page, multi_t address, int dma_c);

inline Bit16u get_value ( multi_t data );
inline void add_value ( multi_t *data, Bit16u value );
inline void sub_value ( multi_t *data, Bit16u value );
inline void set_value ( multi_t *data, Bit16u value );

inline int get_ch (int controller, int channel);
inline int get_mask (int ch);

size_t dma_do_read(int controller, int channel, Bit32u target_addr);
size_t dma_do_write(int controller, int channel, Bit32u target_addr);


/* PUBLIC CODE */

void dma_assert_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Asserting DACK on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].dack = 1;
}

void dma_assert_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Asserting DREQ on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch + 4);

  dma[controller].status |= mask;
}

void dma_drop_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Dropping DACK on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].dack = 0;
}

void dma_drop_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Dropping DREQ on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch +4);
  dma[controller].status &= ~ mask;
}

void dma_assert_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Asserting EOP on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].eop = 1;
}

void dma_drop_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Dropping EOP on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].eop = 0;
}

int dma_test_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Testing DACK on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  return dma[controller].i[ch].dack;
}

int dma_test_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Testing DREQ on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch + 4);

  return dma[controller].status & mask;
}

int dma_test_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Testing EOP on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  return dma[controller].i[ch].eop;
}




/* PRIVATE CODE */


/*
 * This removes any Endian-ness assumptions
 */

inline Bit16u get_value (multi_t data) 
{
	return ( ((Bit16u)data.bits.msb << 8) + data.bits.lsb);
}

inline void add_value (multi_t *data, Bit16u value) 
{
	Bit16u tmp;

#ifdef EXCESSIVE_DEBUG
	h_printf ("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif /* EXCESSIVE_DEBUG */

	tmp = ( ((Bit16u)data->bits.msb << 8) + data->bits.lsb);

#ifdef EXCESSIVE_DEBUG
	h_printf ("DMA: Adding %u to %u gives ", value, tmp);
#endif /* EXCESSIVE_DEBUG */

	tmp += value;

#ifdef EXCESSIVE_DEBUG
	h_printf ("%u\n", tmp);
#endif /* EXCESSIVE_DEBUG */

	data->bits.msb = (Bit8u) (tmp >> 8);
	data->bits.lsb = (Bit8u) tmp & 0xFF;

#ifdef EXCESSIVE_DEBUG
	h_printf ("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif /* EXCESSIVE_DEBUG */
}

inline void sub_value (multi_t *data, Bit16u value) 
{
	Bit16u tmp;

#ifdef EXCESSIVE_DEBUG
	h_printf ("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif /* EXCESSIVE_DEBUG */

	tmp = ( ((Bit16u)data->bits.msb << 8) + data->bits.lsb);

#ifdef EXCESSIVE_DEBUG
	h_printf ("DMA: Subtracting %u from %u gives ", value, tmp);
#endif /* EXCESSIVE_DEBUG */

	tmp -= value;

#ifdef EXCESSIVE_DEBUG
	h_printf ("%u\n", tmp);
#endif /* EXCESSIVE_DEBUG */

	data->bits.msb = (Bit8u) (tmp >> 8);
	data->bits.lsb = (Bit8u) tmp & 0xFF;

#ifdef EXCESSIVE_DEBUG
	h_printf ("DMA: MSB %u, LSB %u\n", data->bits.msb, data->bits.lsb);
#endif /* EXCESSIVE_DEBUG */
}

inline void set_value (multi_t *data, Bit16u value) 
{
	data->bits.msb = (Bit8u) (value >> 8);
	data->bits.lsb = (Bit8u) value & 0xFF;
}


inline Bit32u create_addr(Bit8u page, multi_t address, int dma_c)
{
	/* **CRISK** modified this - making this actually work */
	/* AM - Removed 'dma_c' assumption */

	Bit32u base_address;

	base_address = (
		(((Bit32u)page & 0xF) << 16) +
		((Bit16u)address.bits.msb << 8) +
		address.bits.lsb
		);

	if (dma_c == DMA1) {
		/* 64K Page */
		return (base_address);
	} else {
		/* 128K Page */
		return (base_address << 1);
	}
}

inline void dma_toggle_ff(int dma_c)
{
  dma[dma_c].ff = !dma[dma_c].ff;
}


inline Bit8u dma_read_addr (int dma_c, int channel)
{
  Bit8u r;

  r = dma[dma_c].address[channel].data[dma[dma_c].ff];
  h_printf ("DMA: Read %u from Channel %d Address (Byte %d)\n", r,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);

  return r;
}

inline Bit8u dma_read_count (int dma_c, int channel)
{
  Bit8u r;

  r = dma[dma_c].length[channel].data[dma[dma_c].ff];
  h_printf ("DMA: Read %u from Channel %d Length (Byte %d)\n", r,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);

  return r;
}


Bit8u dma_io_read(Bit32u port)
{
  Bit8u r;

  r = 0;

  switch (port)
  {
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

  case DMA1_STAT_REG:
    r = dma[DMA1].status;
    h_printf ("DMA: Read %u from DMA1 Status\n", r);
    break;
  case DMA2_STAT_REG:
    r = dma[DMA2].status;
    h_printf ("DMA: Read %u from DMA2 Status\n", r);
    break;

  default:
    h_printf("DMA: Unhandled Read on 0x%04x\n", (Bit16u) port);
  }

  return r;
}


inline void dma_write_addr (int dma_c, int channel, Bit8u value)
{
  dma[dma_c].address[channel].data[dma[dma_c].ff] = value;
  h_printf ("DMA: Wrote 0x%x into Channel %d Address (Byte %d)\n", value,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);
}

inline void dma_write_count (int dma_c, int channel, Bit8u value)
{
  dma[dma_c].length[channel].data[dma[dma_c].ff] = value;
  h_printf ("DMA: Wrote 0x%x into Channel %d Length (Byte %d)\n", value,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);
}

inline void dma_write_mask (int dma_c, Bit8u value)
{
    if (value & DMA_SELECT_BIT)
    {
      dma[dma_c].current_channel = (int) value & 3;
      h_printf ("DMA: Channel %u selected.\n", 
			(dma_c * 4) + dma[dma_c].current_channel);
    }
    else
    {
      h_printf ("DMA: Channel %u deselected.\n", 
			 (dma_c * 4) + dma[dma_c].current_channel);

      is_dma |= (1 << dma[dma_c].current_channel);

      dma[dma_c].current_channel = -1;
    }
}




void dma_io_write(Bit32u port, Bit8u value)
{
  Bit8u ch;

  switch (port)
  {
  case DMA_ADDR_0:
    dma_write_addr (DMA1, 0, value);
    break;
  case DMA_ADDR_1:
    dma_write_addr (DMA1, 1, value);
    break;
  case DMA_ADDR_2:
    dma_write_addr (DMA1, 2, value);
    break;
  case DMA_ADDR_3:
    dma_write_addr (DMA1, 3, value);
    break;
  case DMA_ADDR_4:
    dma_write_addr (DMA2, 0, value);
    h_printf ("DMA: That last write was a little dubious ....\n");
    break;
  case DMA_ADDR_5:
    dma_write_addr (DMA2, 1, value);
    break;
  case DMA_ADDR_6:
    dma_write_addr (DMA2, 2, value);
    break;
  case DMA_ADDR_7:
    dma_write_addr (DMA2, 3, value);
    break;
  case DMA_CNT_0:
    dma_write_count (DMA1, 0, value);
    break;
  case DMA_CNT_1:
    dma_write_count (DMA1, 1, value);
    break;
  case DMA_CNT_2:
    dma_write_count (DMA1, 2, value);
    break;
  case DMA_CNT_3:
    dma_write_count (DMA1, 3, value);
    break;
  case DMA_CNT_4:
    dma_write_count (DMA2, 0, value);
    break;
  case DMA_CNT_5:
    dma_write_count (DMA2, 1, value);
    break;
  case DMA_CNT_6:
    dma_write_count (DMA2, 2, value);
    break;
  case DMA_CNT_7:
    dma_write_count (DMA2, 3, value);
    break;
  case DMA_PAGE_0:
    dma[DMA1].page[0] = value;
    h_printf ("DMA: Write %u to Channel0 (Page)\n", value);
    break;
  case DMA_PAGE_1:
    dma[DMA1].page[1] = value;
    h_printf ("DMA: Write %u to Channel1 (Page)\n", value);
    break;
  case DMA_PAGE_2:
    dma[DMA1].page[2] = value;
    h_printf ("DMA: Write %u to Channel2 (Page)\n", value);
    break;
  case DMA_PAGE_3:
    dma[DMA1].page[3] = value;
    h_printf ("DMA: Write %u to Channel3 (Page)\n", value);
    break;
  /* DMA_PAGE_4 is not useable - its the cascade channel ... */
  case DMA_PAGE_5:
    dma[DMA2].page[5] = value;
    h_printf ("DMA: Write %u to Channel5 (Page)\n", value);
    break;
  case DMA_PAGE_6:
    dma[DMA2].page[6] = value;
    h_printf ("DMA: Write %u to Channel6 (Page)\n", value);
    break;
  case DMA_PAGE_7:
    dma[DMA2].page[7] = value;
    h_printf ("DMA: Write %u to Channel7 (Page)\n", value);
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
    h_printf ("DMA: Write 0x%x to DMA1 Command\n", value);
    break;
  case DMA2_CMD_REG:
    dma[DMA2].ch_config[dma[DMA2].current_channel] = value;;
    h_printf ("DMA: Write 0x%x to DMA2 Command\n", value);
    break;

  case DMA1_CLEAR_FF_REG:
    h_printf ("DMA: Clearing DMA1 Output FF\n");
                /* Kernel implies this, then ignores it */
    dma[DMA1].ff = value & 1;
    break;
  case DMA2_CLEAR_FF_REG:
    h_printf ("DMA: Clearing DMA2 Output FF\n");
                /* Kernel implies this, then ignores it */
    dma[DMA2].ff = value & 1;
    break;

  default:
    h_printf("DMA: Unhandled Write on 0x%04x\n", (Bit16u) port);
  }
}



void dma_install_handler (int ch, int wfd, int rfd, int (* handler) (int), 
			  int size)
{
  int channel, dma_c;

  h_printf ("DMA: Installing DMA Handler on channel %d [%d bytes/call]\n", 
	    ch, size);

  dma_c = ch & 4;
  channel = ch & DMA_CH_SELECT;

  if (dma[dma_c].i[channel].wfd != -1)
    close (dma[dma_c].i[channel].wfd);
  dma[dma_c].i[channel].wfd = wfd;

  if (dma[dma_c].i[channel].rfd != -1)
    close (dma[dma_c].i[channel].rfd);
  dma[dma_c].i[channel].rfd = rfd;

  dma[dma_c].i[channel].handler = handler;

  dma[dma_c].i[channel].size = size;
}


/*
 * This sets up a DMA channel for the first time.
 */

int dma_initialise_channel (int controller, int channel) 
{
	extern dma_t dma[2];
	int tmp_pipe[2];
	int ch, mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	h_printf ("DMA: Initialising transfer for channel %d, controller %d\n",
		  ch, controller +1);

	if (pipe(tmp_pipe)) {
	  /* Failed to create the pipe */
	  h_printf ("DMA: Failed to intialise transfer for channel %d on controller %d\n", 
		    ch, controller + 1);

		/* 
		 * DANG_FIXTHIS: Pipe Creation failed. Lets hope that it times out ! 
		 */

		is_dma &= ~mask;
		return -1;
	}

	dma[controller].i[channel].wfd = tmp_pipe[0];
	dma[controller].i[channel].rfd = tmp_pipe[1];

	dma_assert_DACK (ch);  /* Force DACK - makes the logic easier */
	
	/* 
	 * DANG_BEGIN_REMARK
	 * I think that DREQ should only be set on auto-init if we are the 
	 * reading portion.
	 * DANG_END_REMARK
	 */

/*      Why Comment this out ? - AM
	if (dma[controller].ch_mode[channel] & (DMA_AUTO_INIT | DMA_READ))
	  dma_assert_DREQ (ch);
	else
 */                       
	h_printf("DMA: [crisk] dropping DREQ\n");
	dma_drop_DREQ(ch);

	return 0;
}

      
/* 
 * This confirms the channel is set up correctly and tries to set it up if
 * it isn't.
 */

int dma_check_channel_setup (int controller, int channel)
{
	/* Have we started using it yet ? */
	   	   
	if ((dma[controller].i[channel].wfd == -1)
	    && (dma[controller].i[channel].rfd == -1)) {
		/* Neither part is open - lets set up the pipe */
		return (dma_initialise_channel (controller, channel));
	} else {
		return 0;
	}

}


int get_ch (int controller, int channel) {
	if (controller == DMA1) {
		return (channel);
	} else {
		return (channel + 4);
	}
}

int get_mask (int ch) {
	return ( 1 << ch );
}

void dma_process_demand_mode_write (int controller, int channel)
{
	Bit32u target_addr;
	int amount_done = 0;
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

      target_addr = create_addr (dma[controller].page[channel],
				  dma[controller].address[channel],
				 controller);

	if (dma_test_DACK(ch)) {
	    dma_drop_DACK(ch);
	} else {
	    return;
	}

	if (!get_value (dma[controller].length[channel])) {
	    /* Transfer is complete */ 
	    dma[controller].status |= (1 << channel);

		is_dma &= ~ (1 << (ch -1));
				
		if (dma[controller].i[channel].handler != NULL) {
			/* The handler is expected to close any descriptors */
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);
		} else {
			close (dma[controller].i[channel].wfd);
		}
		
		dma[controller].i[channel].wfd = -1; 

	    return; 
	  }

	if (dma_test_eop(ch)) {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);

/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1;
	    */ 
		
		is_dma &= ~mask;
	    return;
	  }

	if (dma_test_DREQ(ch)) {
	    dma[controller].i[channel].run = 1; /* TRIVIAL */

		if (get_value (dma[controller].length[channel])
		    < dma[controller].i[channel].size) {
	      dma[controller].i[channel].size 
				= get_value (dma[controller].length[channel]);
		}
 	    amount_done = dma_do_read(controller, channel, target_addr);

		if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
			sub_value (&dma[controller].address[channel],
				   amount_done);
		} else {
			add_value (&dma[controller].address[channel],
				   amount_done);
		}

		sub_value (&dma[controller].length[channel], amount_done);

		if (dma[controller].i[channel].handler != NULL) {
	      dma[controller].i[channel].handler (DMA_HANDLER_WRITE);
	  }
	}
}

void dma_process_single_mode_write (int controller, int channel)
	  {
	Bit32u target_addr;
	int amount_done = 0;
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	target_addr = create_addr(dma[controller].page[channel],
				  dma[controller].address[channel],
				  controller);
	
	if (! get_value (dma[controller].length[channel]) ) {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);

		is_dma &= ~mask;
				
		if (dma[controller].i[channel].handler != NULL) {
			/* The handler is expected to close any descriptors */
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);
		} else {			
			close (dma[controller].i[channel].wfd);
		}

		dma[controller].i[channel].wfd = -1;

	    return;
	  }

	  if (((!dma[controller].i[channel].run) && dma_test_DREQ(ch))
	    || (dma_test_DREQ(ch) && dma_test_DACK(ch))) {
		if (!dma[controller].i[channel].run) {
	      dma[controller].i[channel].run = 1;
		} else {
	      dma_drop_DACK(ch);
		}

		if (get_value (dma[controller].length[channel])
		    < dma[controller].i[channel].size) {
	      dma[controller].i[channel].size 
				= get_value (dma[controller].length[channel]);
		}

 	    amount_done = dma_do_read(controller, channel, target_addr);

		if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
			sub_value (&dma[controller].address[channel],
						amount_done );
		} else {
			add_value (&dma[controller].address[channel],
						amount_done );
		}

		sub_value (&dma[controller].length[channel], amount_done);

		if (dma[controller].i[channel].handler != NULL) {
	      dma[controller].i[channel].handler (DMA_HANDLER_WRITE);
	  }
	}
}	


void dma_process_block_mode_write (int controller, int channel)
{
	Bit32u target_addr;
	int amount_done = 0;
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	target_addr = create_addr(dma[controller].page[channel],
				  dma[controller].address[channel],
				  controller);
	
	if (dma_test_DACK(ch)) {
	    dma_drop_DACK(ch);
	} else {
	    return;
	}

	if (! get_value (dma[controller].length[channel]) ) {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);

		is_dma &= ~mask;
		
		if (dma[controller].i[channel].handler != NULL) {
			/* The handler is expected to close any descriptors */
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);
		} else {
			close (dma[controller].i[channel].wfd);
		}

		dma[controller].i[channel].wfd = -1;

	    return;
	  }

	if (dma_test_eop(ch)) {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);

/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1;
	    */ 
		is_dma &= ~mask;
	    return;
	  }

	  if ((! dma[controller].i[channel].run) && (dma_test_DREQ(ch)))
	    dma[controller].i[channel].run = 1; /* Important ! */

	if (dma[controller].i[channel].run) {
		if ( get_value (dma[controller].length[channel])
		     < dma[controller].i[channel].size) {
	      dma[controller].i[channel].size 
				= get_value (dma[controller].length[channel]);
		}

 	    amount_done = dma_do_read(controller, channel, target_addr);

		if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
			sub_value (&dma[controller].address[channel],
				   amount_done);
		} else {
			add_value (&dma[controller].address[channel],
				   amount_done);
		}

		sub_value (&dma[controller].length[channel], amount_done);
	    
		if (dma[controller].i[channel].handler != NULL) {
	      dma[controller].i[channel].handler (DMA_HANDLER_WRITE);
	  }
	}
}


/* 
 * DANG_FIXTHIS: Cascade mode Writes are not supported 
 */
void dma_process_cascade_mode_write (int controller, int channel)
{
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	  h_printf ("DMA: Attempt to use unsupported CASCADE mode\n");

/*	  close (dma[controller].i[channel].wfd);
	  dma[controller].i[channel].wfd = -1;
	  */ 

	is_dma &= ~mask;

	if (dma[controller].i[channel].handler != NULL) {
		dma[controller].i[channel].handler(DMA_HANDLER_DONE);
	}
}


void dma_process_demand_mode_read (int controller, int channel)
{
	Bit32u target_addr;
	int amount_done = 0;
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	target_addr = create_addr(dma[controller].page[channel],
				  dma[controller].address[channel],
				  controller);
	
	if (dma_test_DACK(ch)) {
	    dma_drop_DACK(ch);
	} else {
	    return;
	}

	if (! get_value(dma[controller].length[channel]) ) {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);

		is_dma &= ~mask;
				
		if (dma[controller].i[channel].handler != NULL) {
			/* The handler is expected to close any descriptors */
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);
		} else {
			close (dma[controller].i[channel].rfd);
		}

		dma[controller].i[channel].rfd = -1;

	    return;
	  }

	if (dma_test_eop(ch)) {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);

/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
	    */ 
		is_dma &= ~mask;
	    return;
	  }

	if (dma_test_DREQ(ch)) {
	    dma[controller].i[channel].run = 1; /* TRIVIAL */

		if (get_value (dma[controller].length[channel])
		    < dma[controller].i[channel].size) {
	      dma[controller].i[channel].size 
				= get_value (dma[controller].length[channel]);
		}

 	    amount_done = dma_do_write(controller, channel, target_addr);

		if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
			sub_value (&dma[controller].address[channel],
				   amount_done);
		} else {
			add_value (&dma[controller].address[channel],
				   amount_done);
		}

		sub_value (&dma[controller].length[channel],
			   amount_done);

		if (dma[controller].i[channel].handler != NULL) {
	      dma[controller].i[channel].handler (DMA_HANDLER_READ);
		}
	}
}


void dma_process_single_mode_read (int controller, int channel)
{
	Bit32u target_addr;
	size_t amount_done = 0;
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	target_addr = create_addr(dma[controller].page[channel],
				  dma[controller].address[channel],
				  controller);
	
	h_printf("DMA: Single Mode Read - length %d (%d)\n", 
		 get_value (dma[controller].length[channel]),
		 dma[controller].i[channel].size);

	if (!get_value (dma[controller].length[channel])) {
	    /* Transfer is complete */
		h_printf("DMA: [crisk] Transfer is complete\n");    
		if (dma[controller].i[channel].handler != NULL) {
			/* The handler is expected to close any descriptors */
			if ((*dma[controller].i[channel].handler)(DMA_HANDLER_DONE) 
			    != DMA_HANDLER_OK) {
			  h_printf ("DMA: Handler indicates incomplete.\n");
			  return;
			} else {
			  h_printf ("DMA: Handler indicates complete.\n");
			}
		} else {
			close (dma[controller].i[channel].rfd);
	  }

	    dma[controller].status |= (1 << channel);

		is_dma &= ~mask;
				
		dma[controller].i[channel].rfd = -1;
				
		return;
	}
			
	h_printf("DMA: [crisk] SM read (%d && %d) || (%d && %d)\n",
		 !dma[controller].i[channel].run,
		 dma_test_DREQ(ch),
		 dma_test_DREQ(ch),
		 dma_test_DACK(ch));
	
	  if ((!dma[controller].i[channel].run && dma_test_DREQ(ch))
	    || (dma_test_DREQ(ch) && dma_test_DACK(ch))) {

		if (!dma[controller].i[channel].run) {
	      dma[controller].i[channel].run = 1;
		} else {
	      dma_drop_DACK(ch);
		}

		if (get_value (dma[controller].length[channel])
		    < dma[controller].i[channel].size) {
	      dma[controller].i[channel].size 
				= get_value (dma[controller].length[channel]);
		}

		h_printf("DMA: [crisk] calling dma_do_write()\n");

 	    amount_done = dma_do_write(controller, channel, target_addr);

		if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
			sub_value (&dma[controller].address[channel],
				   (Bit16u) amount_done);
		} else {
			add_value (&dma[controller].address[channel],
				   amount_done);
		}

		sub_value (&dma[controller].length[channel], amount_done);

		if (dma[controller].i[channel].handler != NULL) {
	      dma[controller].i[channel].handler (DMA_HANDLER_READ);
	  }
	}
	h_printf("DMA: [crisk] DMA single read end trace\n");
}



void dma_process_block_mode_read (int controller, int channel)
{
	Bit32u target_addr;
	int amount_done = 0;
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	target_addr = create_addr(dma[controller].page[channel],
				  dma[controller].address[channel],
				  controller);
	
	if (dma_test_DACK(ch)) {
	    dma_drop_DACK(ch);
	} else {
	    return;
	}

	if (! get_value (dma[controller].length[channel]) ) {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);

		is_dma &= ~mask;

		if (dma[controller].i[channel].handler != NULL) {
			/* The handler is expected to close any descriptors */
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);
		} else {
			close (dma[controller].i[channel].rfd);
		}

		dma[controller].i[channel].rfd = -1;

	    return;
	  }

	if (dma_test_eop(ch)) {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);

/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
	    */ 

		is_dma &= ~mask;
	    return;
	  }

	  if ((! dma[controller].i[channel].run) && (dma_test_DREQ(ch)))
	    dma[controller].i[channel].run = 1; /* Essential ! */

	if (dma[controller].i[channel].run) {
		if ( get_value (dma[controller].length[channel] )
		     < dma[controller].i[channel].size) {

	      dma[controller].i[channel].size 
				= get_value (dma[controller].length[channel]);
		}

 	    amount_done = dma_do_write(controller, channel, target_addr);

		if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC) {
			sub_value (&dma[controller].address[channel], 
				   amount_done);
		} else {
			add_value (&dma[controller].address[channel],
				   amount_done);
		}

		sub_value (&dma[controller].length[channel], amount_done);
	    
		if (dma[controller].i[channel].handler != NULL) {
	      dma[controller].i[channel].handler (DMA_HANDLER_READ);
	  }
	}
}


/* 
 * DANG_FIXTHIS: Cascade Mode Reads are not supported 
 */
void dma_process_cascade_mode_read (int controller, int channel)
{
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

	  h_printf ("DMA: Attempt to use unsupported CASCADE mode\n");

	is_dma &= ~mask;

	if (dma[controller].i[channel].handler != NULL) {
	    dma[controller].i[channel].handler (DMA_HANDLER_DONE);
	} else {
		close (dma[controller].i[channel].rfd);
	}

	dma[controller].i[channel].rfd = -1;
}

/* 
 * DANG_FIXTHIS: The Verify Mode is not supported 
 */
void dma_process_verify_mode  (int controller, int channel)
{
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

 	h_printf ("DMA: Attempt to use unsupported VERIFY direction by controller %d, channel %d\n",
		  controller +1, channel);
	
	if (dma[controller].i[channel].handler != NULL) {
		dma[controller].i[channel].handler(DMA_HANDLER_DONE);
	} else {
	close (dma[controller].i[channel].rfd);
		close(dma[controller].i[channel].wfd);
	}

	dma[controller].i[channel].rfd = -1;
	dma[controller].i[channel].wfd = -1;

	is_dma &= ~mask;
}


/* 
 * DANG_FIXTHIS: The Invalid Mode is not supported (!) 
 */
void dma_process_invalid_mode  (int controller, int channel)
{
	int ch;
	int mask;

	ch = get_ch (controller, channel);
	mask = get_mask (ch);

 	h_printf ("DMA: Attempt to use INVALID direction by controller %d, channel %d\n",
		  controller +1, channel);

	if (dma[controller].i[channel].handler != NULL) {
		dma[controller].i[channel].handler(DMA_HANDLER_DONE);
	} else {
	close (dma[controller].i[channel].rfd);
		close(dma[controller].i[channel].wfd);
	}

	dma[controller].i[channel].rfd = -1;
	dma[controller].i[channel].wfd = -1;

	is_dma &= ~mask;
}



int dma_process_channel (int controller, int channel) {
    h_printf("DMA: processing controller %d, channel %d (rfd %d wfd %d)\n",
		 controller + 1, channel,
		 dma[controller].i[channel].rfd,
		 dma[controller].i[channel].wfd);
	
	/* Confirm the set up */
    if (dma_check_channel_setup (controller, channel) == -1) {
		/* Couldn't Set Up */
		return -1;
    }
	
	/* Should now have a valid set up */
	
	/* 
	 * Telling the DMA controller to READ means that you 
	 * want to read from the address, so we actually need 
	 * to write !
	 */
	
    switch (dma[controller].ch_mode[channel] & DMA_DIR_MASK) {
	case DMA_WRITE:
	    switch (dma[controller].ch_mode[channel] & DMA_MODE_MASK) {
		case DMA_DEMAND_MODE:
			dma_process_demand_mode_write (controller, channel);
			break;

		case DMA_SINGLE_MODE:
			dma_process_single_mode_write (controller, channel);
			break;
			
		case DMA_BLOCK_MODE:
			dma_process_block_mode_write (controller, channel);
			break;
			
		case DMA_CASCADE_MODE:
			dma_process_cascade_mode_write (controller, channel);
			break;
	    };
	    break;
		
	case DMA_READ:
		
	    switch (dma[controller].ch_mode[channel] & DMA_MODE_MASK) {
		case DMA_DEMAND_MODE:
			dma_process_cascade_mode_read (controller, channel);
			break;
			
		case DMA_SINGLE_MODE:
			dma_process_single_mode_read (controller, channel);
			break;

		case DMA_BLOCK_MODE:
			dma_process_block_mode_read (controller, channel);
			break;
	
		case DMA_CASCADE_MODE:
			dma_process_cascade_mode_read (controller, channel);
			break;
	    };
	    break;

	case DMA_VERIFY:
		dma_process_verify_mode (controller, channel);
		break;

	case DMA_INVALID:
		dma_process_invalid_mode (controller, channel);
		break;

    };

    return 0;
}


/*
 * This is surrounds the brains of the operation ....
 */

void dma_controller(void)
{
  Bit8u test;
  int controller, channel;

  for (test = 1, controller = DMA1, channel = 0; test != 0; test = test << 1, channel++) {
    if (channel == 4) {
	controller = DMA2;
	channel = 0;
    }

		/* Process the channel only if it has been deselected */
    if ((is_dma & test) && (dma[controller].current_channel != channel)) {
	/* Time to process this DMA channel */
	dma_process_channel (controller, channel);
    }
  }
}


size_t dma_do_write(int controller, int channel, Bit32u target_addr)
{
  size_t ret_val;

  h_printf("DMA: [crisk] READ (fd %d) from address %p\n",
	dma[controller].i[channel].rfd, (Bit32u *) target_addr);
   
  if (dma[controller].i[channel].rfd == -1) {
                return 0;
  }

  ret_val = write(dma[controller].i[channel].rfd, (void *) target_addr,
		  dma[controller].i[channel].size << controller);

  if (ret_val == -1) {
    if (errno == EBADF) {
      h_printf("DMA: BAD File on Channel %d of controller %d (remaining length: %d)\n",
				channel, controller, 
				get_value(dma[controller].length[channel]));
    set_value (&dma[controller].length[channel], 0);
  } else if (errno != EINTR) {
    h_printf ("DMA: Error in READ on Channel %d of controller %d (%s)\n", 
	      channel, controller, strerror(errno));
#ifdef EXCESSIVE_DEBUG
  } else {
    h_printf("DMA: READ 'interrupted' on Channel %d of controller %d - Ignored\n",
				 channel, controller);
#endif /* EXCESSIVE_DEBUG */
		}
    return 0;
  }
  else {
#ifdef EXCESSIVE_DEBUG
    h_printf ("DMA: Read %u bytes\n", ret_val);
#endif /* EXCESSIVE_DEBUG */

    return ret_val;
  }
}

size_t dma_do_read(int controller, int channel, Bit32u target_addr)
{
  size_t ret_val;

  if (dma[controller].i[channel].wfd == -1)
                return 0;

  ret_val = read(dma[controller].i[channel].wfd, (void *) target_addr,
		  dma[controller].i[channel].size << controller);

  if (ret_val == -1) {
    h_printf ("DMA: Error in WRITE on Channel %d of controller %d (%s)\n", 
	      channel, controller, strerror(errno));  
    return 0;
  }
  else {
    return ret_val;
  }
}

void dma_init(void)
{
  emu_iodev_t  io_device;
  int i, j;
  
  /* 8237 DMA controller */
  io_device.read_portb   = dma_io_read;
  io_device.write_portb  = dma_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.irq          = EMU_NO_IRQ;

  /*
   * XT Controller 
   */
  io_device.start_addr   = 0x0000;
  io_device.end_addr     = 0x000F;
  io_device.handler_name = "DMA - XT Controller";
  port_register_handler(io_device);

  /*
   * Page Registers (XT)
   */
  io_device.start_addr   = 0x0081;
  io_device.end_addr     = 0x0087;
  io_device.handler_name = "DMA - XT Pages";
  port_register_handler(io_device);

  /*
   * AT Controller
   */
  io_device.start_addr   = 0x00C0;
  io_device.end_addr     = 0x00DE;
  io_device.handler_name = "DMA - AT Controller";
  port_register_handler(io_device);

  /*
   * Page Registers (AT)
   */
  io_device.start_addr   = 0x0089;
  io_device.end_addr     = 0x008A;
  io_device.handler_name = "DMA - AT Pages";
  port_register_handler(io_device);

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 5; j++) {
      set_value (&dma[i].address[j], 0);
      set_value (&dma[i].length[j], 0);
      dma[i].ch_config[j] = 0;
      dma[i].page[j] = 0;

      dma[i].i[j].wfd = -1;
      dma[i].i[j].rfd = -1;      	

      dma[i].i[j].size = 1;

      dma[i].i[j].run = 0;
      dma[i].i[j].handler = NULL;
    }

    dma[i].status = 0;
    dma[i].ff = 0;
		dma[i].current_channel = 0;
  }

  is_dma = 0;

  h_printf ("DMA: DMA Controller initialized - 8 & 16 bit modes\n");
}
  

void dma_reset(void)
{
  int i, j;
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 4; j++) {
      set_value (&dma[i].address[j], 0);
      set_value (&dma[i].length[j], 0);
      dma[i].ch_config[j] = 0;
      dma[i].page[j] = 0;

      if (dma[i].i[j].wfd != -1) {
	h_printf ("DMA: Closing Write File for Controller %d Channel %d\n",
		  i, j);
	close (dma[i].i[j].wfd);
      }
      if (dma[i].i[j].rfd != -1) {
	h_printf ("DMA: Closing Read File for Controller %d Channel %d\n",
		  i, j);
	close (dma[i].i[j].rfd);
      }

      dma[i].i[j].wfd = -1;
      dma[i].i[j].rfd = -1;      	

      dma[i].i[j].size = 1;

      dma[i].i[j].run = 0;
      dma[i].i[j].handler = NULL;
    }

    dma[i].status = 0;
    dma[i].ff = 0;
		dma[i].current_channel = 0;
  }

  is_dma = 0;

  h_printf ("DMA: DMA Controller Reset - 8 & 16 bit modes\n");

}
