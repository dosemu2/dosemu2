/*
 * DANG_BEGIN_MODULE
 *
 * Description: SB emulation - Getting There ...!
 * 
 * maintainer: 
 *   Alistair MacDonald <alistair@slitesys.demon.co.uk>
 *   David Brauman <crisk@netvision.net.il>
 *   Rutger Nijlunsing <rutger@null.net>
 *
 * DANG_END_MODULE
 *
 * Previous Maintainers:
 *  Joel N. Weber II
 *  Michael Beck
 *
 * History:
 * ========
 * The original code was written by Joel N. Weber II. See README.sound
 * for more information. I (Alistair MacDonald) made the code compile, and
 * added a few extra features. I took the code and continued development on
 * 0.61. Michael Beck did a lot of work on 0.60.4, including the original
 * NetBSD code and capability detection under Linux. This code is basically
 * my 0.61 code, brought back into mainstream DOSEmu (0.63), but with
 * Michael's code where I thought it was better than mine. I also separated
 * out the NetBSD & Linux specific driver code. - Alistair
 *
 * For 0.66 we have updated many areas and re-organised others to make it 
 * easier to maintain. Many thanks for David for finding so many bugs ....
 *
 * Original Copyright Notice:
 * ==========================
 * Copyright 1995  Joel N. Weber II
 * See the file README.sound in this directory for more information 
 */


/*
 * This is defined by default. Turning it off gives less information, but
 * makes execution slightly faster.
 */
#define EXCESSIVE_DEBUG 1
/*
 * This makes the code complain more (into the debug log)
 */
#define FUSSY_SBEMU 1

#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "port.h"
#include "dma.h"
/* #include "pic.h" */
#include "sound.h"
#include <stdio.h>
#include <stdlib.h>

/*
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Internal Functions */

inline void dsp_write_output(__u8 value);
inline void dsp_clear_output(void);
inline __u8 dsp_read_output(void);

static void start_dsp_dma(void);
static void restart_dsp_dma(void);
static void pause_dsp_dma(void);

int sb_dma_handler(int status);
void sb_irq_trigger (void);

void sb_set_speed (void);
static void sb_enable_speaker (void);
static void sb_disable_speaker(void);
static void sb_write_DAC (int bits, __u8 value);

static void dsp_do_copyright(void);

static __u8 sb_read_mixer(__u8 ch);
static void sb_write_mixer(int ch, __u8 value);

/* DANG_FIXTHIS The file header needs tidying up a _LOT_ ! */

static void sb_init(void);
static void fm_init(void);
static void mpu401_init(void);

static void sb_reset(void);
static void fm_reset(void);
static void mpu401_reset(void);

static void sb_dsp_write ( Bit8u value );


__u8 dsp_read_output(void);
void dsp_write_output(__u8 value);

void sb_handle_test (void);
void sb_do_sine (void);
void sb_get_aux_status (void);
void sb_handle_dac (void);
void sb_dsp_handle_interrupt(void);
void sb_stereo_input (void);
void sb_handle_speaker (void);
void sb_get_version (void);
void sb_do_adc(void);
void stop_dsp_dma(void);
void set_dma_blocksize(void);
void sb_dsp_get_status (void);
void sb_dsp_unsupported_command (void);
void sb_write_silence (void);
void sb_do_midi_write (void);
void sb_handle_sb20_dsp_probe (void);

void sb_cms_write (Bit32u port, Bit8u value);
inline void sb_mixer_register_write (Bit8u value);
void sb_mixer_data_write (Bit8u value);
void sb_do_reset (Bit8u value);

int sb_set_length(Bit16u *variable);


/*
 ************************************************************
 * DSP Queue functions : Used to allow multi-byte responses *
 ************************************************************
 */

void dsp_write_output(__u8 value)
{
  /* There is no check for exceeding the length of the queue ... */

  ++SB_queue.holds;
  SB_queue.output[SB_queue.end++] = value;
  SB_queue.end &= (DSP_QUEUE_SIZE - 1);
  SB_dsp.data = SB_DATA_AVAIL;

#ifdef EXCESSIVE_DEBUG
  S_printf ("SB: Insert into output Queue [%u]... (0x%x)\n", 
	    SB_queue.holds, value);
#endif /* EXCESSIVE_DEBUG */
}

void dsp_clear_output(void)
{
#ifdef EXCESSIVE_DEBUG
  S_printf ("SB: Clearing the output Queue\n");
#endif /* EXCESSIVE_DEBUG */

  SB_queue.holds = 0;
  SB_queue.end   = 0;
  SB_queue.start = 0;
  SB_dsp.data    = SB_DATA_UNAVAIL;
}

__u8 dsp_read_output(void)
{
  unsigned char r = 0xFF;

  if (SB_queue.holds) {
    r = SB_queue.output[SB_queue.start];
    SB_queue.output[SB_queue.start++] = 0x00;
    SB_queue.start &= (DSP_QUEUE_SIZE - 1);

    if (--SB_queue.holds)
      SB_dsp.data = SB_DATA_AVAIL;
    else
      SB_dsp.data = SB_DATA_UNAVAIL;

#ifdef EXCESSIVE_DEBUG
    S_printf ("SB: Remove from output Queue [%u]... (0x%X)\n", 
	      SB_queue.holds, r);
#endif /* EXCESSIVE_DEBUG */
  }
  return r;
}


/*
 * Main IO Routines - Read
 * =======================
 */

/*
 * DANG_BEGIN_FUNCTION sb_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * description:
 * This handles all of the reads for the SB emulation. The value read is
 * returned. The value of 0xFF indicates an invalid read. (assumes the ports
 * float high when not pulled low by the hardware.)
 *
 * DANG_END_FUNCTION
 */

Bit8u sb_io_read(Bit32u port)
{
  __u32 addr;
  __u8 value;

  addr = port - config.sb_base;

  switch (addr) {

    /* == FM Music == */

   case 0x00:
	/* FM Music Left Status Port - SBPro */
	if (SB_info.version >= SB_PRO) {
		return fm_io_read (FM_LEFT_STATUS);
	} else {
		return 0xFF;
	}
	break;

   case 0x02:
	/* FM Music Right Status Port - SBPro */
	if (SB_info.version >= SB_PRO) {
		return fm_io_read (FM_RIGHT_STATUS);
	}
	else {
		return 0xFF;
	}
	break;
    
   case 0x05: /* Mixer Data Register */
		break;

   case 0x06: /* Reset ? */
     S_printf("SB: read from Reset address\n");
     return 0; /* Some programms read this whilst resetting */
     break;

   case 0x08:
	/* FM Music Compatible Status Port - SB */
	/* (Alias to 0x00 on SBPro) */
	return fm_io_read(FM_LEFT_STATUS);
	break;

   case 0x0A: /* DSP Read Data - SB */
     value = dsp_read_output(); 
     S_printf ("SB: Read 0x%x from SB DSP\n", value);
     return value;
     break;

   case 0x0C: /* DSP Write Buffer Status */
     S_printf ("SB: Write? %x\n", SB_dsp.ready);
     return SB_dsp.ready;
		break;

   case 0x0D: /* DSP Timer Interrupt Clear - SB16 ? */
     S_printf("SB: read 16-bit timer interrupt status. Not Implemented.\n");
     return -1;
     break;

   case 0x0E:		
     /* DSP Data Available Status - SB */
     /* DSP 8-bit IRQ Ack - SB */
     S_printf("SB: Ready?/8-bit IRQ Ack: %x\n", SB_dsp.data);
     return SB_dsp.data; 
     break;

   case 0x0F: /* 0x0F: DSP 16-bit IRQ - SB16 */
     S_printf("SB: 16-bit IRQ Ack: %x\n", SB_dsp.data);
     return SB_dsp.data;
     break;

     /* == CD-ROM - UNIMPLEMENTED == */

   case 0x10: /* CD-ROM Data Register - SBPro */
     if (SB_info.version > SB_PRO) {
       S_printf("SB: read from CD-ROM Data register.\n");
       return 0;
     }
     else {
       S_printf("SB: CD-ROM not supported in this version.\n");
       return 0xFF;
     }
		break;

   case 0x11: /* CD-ROM Status Port - SBPro */
     if (SB_info.version > SB_PRO) {
       S_printf("SB: read from CD-ROM status port.\n");
       return 0xFE;
     }
     else {
       S_printf("SB: CD-ROM not supported in this version.\n");
       return 0xFF;
     }
		break;

   default:
	S_printf("SB: %lx is an unhandled read port.\n", port);
		return 0xFF;
	};

	/* Unreachable ??? */
	return 0xFF;
}

Bit8u sb_mixer_data_read (void)
{
	if (SB_info.version > SB_20) {
	    switch (SB_info.mixer_index) {
		case 0x04:
			return sb_read_mixer(SB_MIXER_PCM);
			break;

		case 0x0A:
			return sb_read_mixer(SB_MIXER_MIC);
			break;

		case 0x0E:
			return SB_dsp.stereo;
			break;

		case 0x22:
			return sb_read_mixer(SB_MIXER_VOLUME);
			break;

		case 0x26:
			return sb_read_mixer(SB_MIXER_SYNTH);
			break;

		case 0x28:
			return sb_read_mixer(SB_MIXER_CD);
			break;

		case 0x2E:
			return sb_read_mixer(SB_MIXER_LINE);
			break;
    
		default:
			S_printf("SB: invalid read from mixer (%x)\n", 
				 SB_info.mixer_index);
			return 0xFF;
			break;
	    };
	} else {
		S_printf("SB: mixer not supported on this SB version.\n");
		return 0xFF;
	}
}


/*
 * DANG_BEGIN_FUNCTION adlib_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * description:
 * This handles all of the reads for the adlib (FM) emulation. The value read 
 * is returned. The value of 0xFF indicates an invalid read. (assumes the ports
 * float high when not pulled low by the hardware.)
 * The FM emulation is not written yet. The current plan is to use the midi
 * emulation where available as this is the most common use for the FM sound.
 *
 * DANG_END_FUNCTION
 */

Bit8u adlib_io_read(Bit32u port)
{
  /* Adlib Base Port is 0x388 */
  /* Adv. Adlib Base Port is 0x38A */
  
  switch (port){
  case 0x388:    
    S_printf ("Adlib: Read from Adlib port (%lx)\n", port);
    return fm_io_read (ADLIB_STATUS);
  case 0x38A:    
    S_printf ("Adv_Adlib: Read from Adlib Advanced port (%lx)\n", port);
    return fm_io_read (ADV_ADLIB_STATUS);
	default:
		S_printf("%lx is an unhandled read port\n", port);
  };
  return 0;
}

Bit8u fm_io_read (Bit32u port)
{
  switch (port){
  case ADLIB_STATUS:
		/* DANG_FIXTHIS Adlib status reads are unimplemented */
    return 31;
    break;
  case ADV_ADLIB_STATUS:
		/* DANG_FIXTHIS Advanced adlib reads are unimplemented */
    return 31;
    break;
  };
  
  return 0;
}

/*
 * DANG_BEGIN_FUNCTION mpu401_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * description:
 * The MPU-401 functionality is primarily provided by 'midid' - a standalone
 * program. This makes most of the MPU-401 code simply a pass-through driver.
 *
 * DANG_END_FUNCTION
 */

Bit8u mpu401_io_read(Bit32u port)
{
  __u32 addr;
	Bit8u r=0xff;
  
  addr = port - config.mpu401_base;
  
  switch(addr) {
  case 0:
    /* Read data port */
    r=0xfe; /* Whatever happened before, send a MPU_ACK */
		mpu401_info.isdata=0;
    S_printf("MPU401: Read data port = 0x%02x\n",r);
    break;
  case 1:
     /* Read status port */
    /* 0x40=OUTPUT_AVAIL; 0x80=INPUT_AVAIL */
    r=0xff & (~0x40); /* Output is always possible */
		if (mpu401_info.isdata) r &= (~0x80);
    S_printf("MPU401: Read status port = 0x%02x\n",r);
  }
  return r;
}


/*
 * Main IO Routines - Write
 * ========================
 */

/*
 * DANG_BEGIN_FUNCTION sb_io_write
 *
 * arguments:
 * port - The I/O port being written to.
 * value - The value being output.
 *
 * description:
 * This handles the writes for the SB emulation. Very little of the processing
 * is performed in this function as it basically consists of a very large
 * switch() statement. The processing here is limited to trivial (1 line) items
 * and distinguishing between the different actions and responses that the
 * different revisions of the SB series give.
 *
 * DANG_END_FUNCTION
 */

void sb_io_write(Bit32u port, Bit8u value)
{
  __u32 addr;
  
  addr = port - config.sb_base;

#ifdef EXCESSIVE_DEBUG  
  S_printf("SB: [crisk] port 0x%04x value 0x%02x\n", (Bit16u)port, value);
#endif /* EXCESSIVE_DEBUG */
  
  switch (addr) {
    
    /* == FM MUSIC or C/MS == */
    
  case 0x00:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Left Register Port - SBPro */
      fm_io_write (FM_LEFT_REGISTER, value);
		} else {
			/* C/MS Data Port (1-6) - SB Only */
			sb_cms_write (CMS_LOWER_DATA, value);
    }
    break;

  case 0x01:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Left Data Register - SBPro */
      fm_io_write (FM_LEFT_DATA, value);
		} else {
			/* C/MS Register Port (1-6) - SB Only */
			sb_cms_write (CMS_LOWER_REGISTER, value);
    }
    break;

  case 0x02:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Right Register Port - SBPro */
      fm_io_write (FM_RIGHT_REGISTER, value);
    }
    else {
			/* C/MS Data Port (7-12) - SB Only */
			sb_cms_write (CMS_UPPER_DATA, value);
    }
    break;

  case 0x03:
    if (SB_info.version >= SB_PRO) {
			/* FM Music Right Data Register - SBPro */
      fm_io_write (FM_RIGHT_DATA, value);
    }
    else {
			/* C/MS Register Port (7-12) - SB Only */
			sb_cms_write (CMS_UPPER_REGISTER, value);
    }
    break;
    
    /* == MIXER == */
    
  case 0x04: /* Mixer Register Port - SBPro */
		sb_mixer_register_write (value);
    break;

  case 0x05: /* Mixer Data Register - SBPro */
		sb_mixer_data_write (value);
		break;

		/* == RESET == */

  case 0x06:		/* reset */
		sb_do_reset (value);
		break;

		/* == FM MUSIC == */

  case 0x08:		
		/* FM Music Register Port - SB */
		/* Alias for 0x00 - SBPro */
		fm_io_write (FM_LEFT_REGISTER, value);
		break;

  case 0x09:
		/* FM Music Data Port - SB */
		/* Alias for 0x01 - SBPro */
		fm_io_write (FM_LEFT_DATA, value);
		break;

		/* == DSP == */

  case 0x0C:		/* dsp write register */
		sb_dsp_write ( value );
		break;

		/* 0x0D: Timer Interrupt Clear - SB16 */
		/* 0x10 - 0x13: CD-ROM - SBPro ***IGNORED*** */

  default:
		S_printf("SB: %x is an unhandled write port\n", addr);
  };
}

/*
 * DANG_FIXTHIS CMS Writes are unimplemented.
 */
void sb_cms_write (Bit32u port, Bit8u value)
{
	switch (port) {
	case CMS_LOWER_DATA:
		S_printf("SB: Write 0x%x to C/MS 1-6 Data Port\n", value);
		break;

	case CMS_LOWER_REGISTER:
		S_printf("SB: Write 0x%x to C/MS 1-6 Register Port\n", value);
		break;

	case CMS_UPPER_DATA:
		S_printf("SB: Write 0x%x to C/MS 7-12 Data Port\n", value);
		break;

	case CMS_UPPER_REGISTER:
		S_printf("SB: Write 0x%x to C/MS 7-12 Register Port\n", value);
		break;

	default:
		S_printf("SB: Invalid write to C/MS (0x%x, 0x%lx)\n", value,
			 port);
	}
}

inline void sb_mixer_register_write (Bit8u value)
{
    if (SB_info.version > SB_20) {
		SB_info.mixer_index = value;
    }
}

void sb_mixer_data_write (Bit8u value)
{
  if (SB_info.version > SB_20) {
    switch (SB_info.mixer_index) {
      case 0:
	/* 0 is the reset register, but we'll ignore it */
			break;

      case 0x04:
			sb_write_mixer(SB_MIXER_PCM, value);
			break;

      case 0x0A:
			sb_write_mixer(SB_MIXER_MIC, value);
			break;

      case 0x0C:
			/* 0x0C is ignored - sets record source and a filter */
			break;

      case 0x0E: 
	SB_dsp.stereo = value & 2;
	S_printf ("SB: Stereo Mode set to %d\n", SB_dsp.stereo);
			/* I ignored the output filter */
			break;	
		
      case 0x22: 
	sb_write_mixer(SB_MIXER_VOLUME, value);
	break;

      case 0x26: 
	sb_write_mixer(SB_MIXER_SYNTH, value);
	break;

      case 0x28: 
	sb_write_mixer(SB_MIXER_CD, value);
	break;

      case 0x2E: 
	sb_write_mixer(SB_MIXER_LINE, value);
	break;

		default:
			S_printf ("SB: Unknown index 0x%x in Mixer Write\n",
				  SB_info.mixer_index);
	break;
    }; 
  }
}
    
void sb_do_reset (Bit8u value)
{
  sb_reset();
  if (value != 0xC6) {
		/* From CRISK: Undocumented */
    dsp_write_output(0xAA);
  }
}
    
    
/*
 * DANG_BEGIN_FUNCTION sb_dsp_write
 *
 * arguments:
 * value - The value being written to the DSP.
 *
 * description:
 * The SB DSP is a complete I/O system in itself controlled via a number of
 * data bytes. The number of bytes depends upon the function. The function
 * to be executed is determined by the first byte.
 * If there is no existing command then the command is stored. This then used
 * in the switch to identify the action to be taken. When the command has 
 * supplied all of its arguments, or failed, then the command storage is 
 * cleared. Each DSP function is responsible for clearing this itself.
 * Again, this function relies on other functions to do the real work, and
 * apart from storing details of the command and parameters is basically a
 * large switch statement.
 *
 * DANG_END_FUNCTION
 */
    
void sb_dsp_write ( Bit8u value ) 
{
	/* 
	 * ALL commands set SB_dsp.command to SB_NO_DSP_COMMAND when they 
	 * complete 
	 */

      if (SB_dsp.command == SB_NO_DSP_COMMAND) {
      	/* No command yet */
      	SB_dsp.command = value;
      	SB_dsp.parameter = 0;
      	SB_dsp.write_size_mode = 0;
      	SB_dsp.have_parameter = SB_PARAMETER_EMPTY;
      }
      else {
      	SB_dsp.parameter = value;
      	SB_dsp.have_parameter = SB_PARAMETER_FULL;
      }

      switch (SB_dsp.command) {
      /* == STATUS == */

		/* 0x03: ASP Status - SB16ASP */
	
	case 0x04:	
	if (SB_info.version >= SB_20 && SB_info.version <= SB_PRO) {
			/* DSP Status - SB2.0-Pro2 - Obselete */
			sb_dsp_get_status ();
		} else {
			/* ASP ? - SB16ASP */
			sb_dsp_unsupported_command ();
	}
	break;
	
		/* 0x05: ??? - SB16ASP */
	
	case 0x10:	
		/* Direct 8-bit DAC - SB */
		sb_handle_dac();
	break;
	
	
		/* == OUTPUT COMMANDS == */

	case 0x14:
		/* DMA 8-bit DAC - SB */
		start_dsp_dma ();
	break;

	case 0x16:
		/* DMA 2-bit ADPCM DAC - SB */
		start_dsp_dma ();
	break;

	case 0x17:
		/* DMA 2-bit ADPCM DAC (Reference) - SB */
		start_dsp_dma();
	break;
	
	case 0x1C:	
		/* DMA 8-bit DAC (Auto-Init) - SB2.0 */
		start_dsp_dma ();
		break;
	
	case 0x1F:	
		/* DMA 2-bit ADPCM DAC (Reference, Auto-Init) - SB2.0 */
		start_dsp_dma ();
	break;


		/* == INPUT COMMANDS == */

	case 0x20:	
		/* Direct 8-bit ADC - SB */
		sb_do_adc();
	break;

	case 0x24:
		/* DMA 8-bit DAC - SB */
		start_dsp_dma();

	case 0x28:
		/* Direct 8-bit ADC (Burst) - SBPro2 */
		sb_do_adc();
		break;
			
	case 0x2C:	
		/* DMA 8-bit ADC (Auto-Init) - SB2.0 */
		start_dsp_dma();
		break;


		/* == MIDI == */

	case 0x30:
		/* Midi Read Poll - SB */
		sb_do_midi_write();
		break;

	case 0x31:
		/* Midi Read Interrupt - SB */
		sb_do_midi_write();
		break;

	case 0x32:
		/* Midi Read Timestamp Poll - SB ? */
		sb_do_midi_write();
		break;

	case 0x33:
		/* Midi Read Timestamp Interrupt - SB ? */
		sb_do_midi_write();
		break;

	case 0x35:
		/* Midi Read/Write Poll (UART) - SB2.0 */
		sb_do_midi_write();
		break;

	case 0x36:
		/* Midi Read Interrupt/Write Poll (UART) - SB2.0 ? */
		sb_do_midi_write();
	break;
	
	case 0x37:
		/* Midi Read Interrupt Timestamp/Write Poll (UART) - SB2.0? */
		sb_do_midi_write();
		break;

	case 0x38:
		/* Midi Write Poll - SB */
		sb_do_midi_write();
		break;
	

		/* == SAMPLE SPEED == */

	case 0x40:
		/* Set Time Constant */
		sb_set_speed();
		break;

	case 0x41:
		/* Set Sample Rate - SB16 */
		sb_set_speed();
		break;


	/* == OUTPUT == */
	
	case 0x45:
		/* Continue Auto-Init 8-bit DMA - SB16 */
		restart_dsp_dma();
	break;
	
	case 0x47:
		/* Continue Auto-Init 16-bit DMA - SB16 */
		restart_dsp_dma();
	break;
	
	case 0x48:	
		/* Set DMA Block Size - SB2.0 */
		set_dma_blocksize();
		break;

	case 0x74:
		/* DMA 4-bit ADPCM DAC - SB */
		start_dsp_dma();
		break;
		
	case 0x75:
		/* DMA 4-bit ADPCM DAC (Reference) - SB */
		start_dsp_dma();
		break;

	case 0x76:
		/* DMA 2.6-bit ADPCM DAC - SB */
		start_dsp_dma();
		break;

	case 0x77:
		/* DMA 2.6-bit ADPCM DAC (Reference) - SB */
		start_dsp_dma();
		break;

	case 0x7D:
		/* DMA 4-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
		start_dsp_dma();
		break;

	case 0x7F:
		/* DMA 2.6-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
		start_dsp_dma();
	break;
	
	case 0x80:
		/* Silence DAC - SB */
		sb_write_silence();
		break;

	case 0x90:
		/* DMA 8-bit DAC (High Speed, Auto-Init) - SB2.0-Pro2 */
		start_dsp_dma();
		break;


	/* == INPUT == */
	
	case 0x98:
		/* DMA 8-bit ADC (High Speed, Auto-Init) - SB2.0-Pro2 */
		start_dsp_dma();
	break;
	
	
		/* == STEREO MODE == */

	case 0xA0:	
		/* Enable Stereo Input - SBPro Only */
		sb_stereo_input();
	break;

	case 0xA8:
		/* Disable Stereo Input - SBPro Only */
		sb_stereo_input();
	break;
	

	/* == DMA == */
	
	case 0xD0:
		/* Halt 8-bit DMA - SB */
	pause_dsp_dma(); 
	break;
	

		/* == SPEAKER == */
		
	case 0xD1:
		/* Enable Speaker - SB */
		sb_handle_speaker();
	break;
	
	case 0xD3:
		/* Disable Speaker - SB */
		sb_handle_speaker();
	break;
	
	
		/* == DMA == */

	case 0xD4:
		/* Continue 8-bit DMA - SB */
		restart_dsp_dma();
	break;

	case 0xD5:
		/* Halt 16-bit DMA - SB16 */
		pause_dsp_dma();
		break;

	case 0xD6:
		/* Continue 16-bit DMA - SB16 */
		restart_dsp_dma();
	break;
	

		/* == SPEAKER == */
		
	case 0xD8:
		/* Speaker Status */
		sb_handle_speaker();
		break;
	
	/* == DMA == */
	
	case 0xD9:
		/* Exit Auto-Init 16-bit DMA - SB16 */
		stop_dsp_dma();
		break;

	case 0xDA:
		/* Exit Auto-Init 8-bit DMA - SB2.0 */
		stop_dsp_dma();
		break;

	
	/* == DSP IDENTIFICATION == */
	
	case 0xE0:
		/* DSP Identification - SB2.0 */
		sb_handle_sb20_dsp_probe();
	break;

	case 0xE1:
		/* DSP Version - SB */
		sb_get_version();
		break;

	case 0xE3:
		/* DSP Copyright - SBPro2 ? */
	  dsp_do_copyright();
		break;

	case 0xE4:
		/* Write to Test - SB2.0 */
		sb_handle_test();
		break;


		/* == TEST == */

	case 0xE8:
		/* Read from Test - SB2.0 */
		sb_handle_test();
		break;

	case 0xF0:
		/* Sine Generator - SB */
		sb_do_sine();
		break;


		/* == STATUS == */

	case 0xF1:	
		/* DSP Auxiliary Status - SBPro2 */
		sb_get_aux_status();
		break;


		/* == IRQ == */

	case 0xF2:	
		/* 8-bit IRQ - SB */
		sb_dsp_handle_interrupt();
		break;

	case 0xF3:
		/* 16-bit IRQ - SB16 */
		sb_dsp_handle_interrupt();
		break;

		/* 0xFB: DSP Status - SB16 */
		/* 0xFC: DSP Auxiliary Status - SB16 */
		/* 0xFD: DSP Command Status - SB16 */
      }
}


/* 
 * DANG_FIXTHIS DSP Status is unimplemented 
 */
void sb_dsp_get_status (void) 
{
	Bit8u output = 0;

	S_printf("SB: Request for SB2.0/SBPRO status (Unimplemented)\n");

	if (SB_info.version != SB_20 && SB_info.speaker) {
		output |= 1;
	}

	dsp_write_output(output);
	SB_dsp.command = SB_NO_DSP_COMMAND;
}
	
void sb_dsp_unsupported_command (void)
{
	S_printf ("SB: Unsupported Command 0x%x\n", SB_dsp.command);
	
	SB_dsp.command = SB_NO_DSP_COMMAND;
}


/*
 * DANG_FIXTHIS Write Silence is not implemented.
 */
void sb_write_silence (void)
{
	if (sb_set_length (&SB_dsp.length)) {
		S_printf("SB: DAC silence length set to %d. (Unimplemented)\n",
			 SB_dsp.length);

		SB_dsp.command = SB_NO_DSP_COMMAND;
		/*
		 * DANG_BEGIN_REMARK
		 * Write silence could probably be implemented by setting up a
		 * "DMA" transfer from /dev/null - AM
		 * DANG_END_REMARK
		 */
	}
}


void sb_set_speed (void)
{
    switch (SB_dsp.command) {
	case 0x40:
		if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
			SB_dsp.time_constant = SB_dsp.parameter;
			SB_dsp.command = SB_NO_DSP_COMMAND;

			if (SB_dsp.stereo) {
				SB_dsp.sample_rate = 1000000 
					/ (2 * (256 - SB_dsp.time_constant));
			}
			else {
				SB_dsp.sample_rate = 1000000 
					/ (256 - SB_dsp.time_constant);
			}

		} else {
			return;
		}
		break;

	case 0x41:
		SB_dsp.command = SB_NO_DSP_COMMAND;
		if (SB_info.version >= SB_16) {
			if (!sb_set_length(&SB_dsp.sample_rate)) {
				return;
			};
		} else {
			return;
		}
	break;
	
	default:
		/* 
		 * Allow drop through, since this is probably the speaker
		 * (or something!)
		 */
		break;
	
    }

    S_printf("SB: Trying to set speed to %u Hz.\n", SB_dsp.sample_rate);

    if (SB_driver.set_speed == NULL) {
		S_printf("SB: Required function 'set_speed' not provided.\n");
    }
    else {
		(*SB_driver.set_speed) (SB_dsp.sample_rate, SB_dsp.stereo);
    }

}


void adlib_io_write(Bit32u port, Bit8u value)
{
    /* Base Port for Adlib is 0x388 */
    /* Base Port for Adv. Adlib is 0x38a */

    switch (port) {
	case 0x388:		/* Adlib Register Port */
		S_printf("Adlib: Write 0x%x to Register Port\n", value);
		fm_io_write(ADLIB_REGISTER, value);
		break;
	case 0x389:		/* Adlib Data Port */
		S_printf("Adlib: Write 0x%x to Data Port\n", value);
		fm_io_write(ADLIB_DATA, value);
	break;
	
	case 0x38A:		/* Adv. Adlib Register Port */
		S_printf("Adv_Adlib: Write 0x%x to Register Port\n", value);
		fm_io_write(ADV_ADLIB_REGISTER, value);
		break;
	case 0x38B:		/* Adv. Adlib Data Port */
		S_printf("Adv_Adlib: Write 0x%x to Data Port\n", value);
		fm_io_write(ADV_ADLIB_DATA, value);
		break;
    };
}
	
void fm_io_write(Bit32u port, Bit8u value)
{
    switch (port) {
	case ADLIB_REGISTER:
		/* DANG_FIXTHIS Adlib register writes are unimplemented */
	break;

	case ADLIB_DATA:
		/* DANG_FIXTHIS Adlib data writes are unimplemented */
		break;
	
	case ADV_ADLIB_REGISTER:
		/* DANG_FIXTHIS Advanced Adlib register writes are unimplemented */
		break;

	case ADV_ADLIB_DATA:
		/* DANG_FIXTHIS Advanced Adlib data writes are unimplemented */
		break;
	
    };
}


void mpu401_io_write(Bit32u port, Bit8u value)
{
	__u32 addr;

	addr = port - config.mpu401_base;

	switch (addr) {
	case 0:
		/* Write data port */
		(*mpu401_info.data_write)(value);
		S_printf("MPU401: Write 0x%02x to data port\n",value);
		break;
	case 1:
		/* Write command port */
		if (port == (config.mpu401_base+1)) {
			mpu401_info.isdata=1; /* A command is sent: MPU_ACK it next time */
			/* Known commands:
			   0x3F = UART mode
			   0xFF = reset MPU. Does anyone know more? */
			S_printf("MPU401: Write 0x%02x to command port\n",value);
			break;
		}
		break;
	}
}


/* 
 * DANG_FIXTHIS SB Midi is Unimplemented 
 */

void sb_do_midi_write (void) 
{
	switch (SB_dsp.command) {
	case 0x30:
		/* Midi Read Poll - SB */
		S_printf("SB: Attempt to read from SB Midi (Poll)\n");
		dsp_write_output(0);
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	case 0x31:
		/* Midi Read Interrupt - SB */
		S_printf("SB: Attempt to read from SB Midi (Interrupt)\n");
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	case 0x32:
		/* Midi Read Timestamp Poll - SB ? */
		S_printf("SB: Attempt to read from SB Midi (Poll)\n");
		dsp_write_output(0);
		dsp_write_output(0);
		dsp_write_output(0);
		dsp_write_output(0);
			SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	case 0x33:
		/* Midi Read Timestamp Interrupt - SB ? */
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	case 0x35:
		/* Midi Read/Write Poll (UART) - SB2.0 */
	case 0x36:
		/* Midi Read Interrupt/Write Poll (UART) - SB2.0 ? */
	case 0x37:
		/* Midi Read Interrupt Timestamp/Write Poll (UART) - SB2.0 ? */
		if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
		if (SB_info.version >= SB_20) {
				S_printf ("SB: Write 0x%u to SB Midi Port\n",
					  SB_dsp.parameter);
				/* Command Ends _only_ on reset */
			} else {
				SB_dsp.command = SB_NO_DSP_COMMAND;
		}
		}
		break;
		 
	case 0x38:
		/* Midi Write Poll - SB */
		if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
			S_printf ("SB: Write 0x%u to SB Midi Port\n",
				  SB_dsp.parameter);
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}
		break;

	default:
		S_printf("SB: Unknown MIDI command 0x%x\n", SB_dsp.command);
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;
	}
}


static void dsp_do_copyright(void)
{
	char cs[] = "(c) Copyright 1995 Alistair MacDonald";
	char *ptr;
	int i;

	if (SB_info.version > SB_PRO) {
		S_printf("SB: DSP Copyright requested.\n");
		for (i = strlen(cs) + 1, ptr = cs; i >= 0; ptr++, i--)
			dsp_write_output((__u8) * ptr);
	} else {
		S_printf("SB: DSP copyright not supported by this SB Version.\n");
	}

	SB_dsp.command = SB_NO_DSP_COMMAND;
}

void sb_handle_sb20_dsp_probe (void)
{
	if (SB_info.version >= SB_20) {
		if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
			S_printf("SB: Identify SB2.0 DSP.\n");
			dsp_write_output(~SB_dsp.parameter);
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}
	}
	else {
		S_printf("SB: Identify DSP not supported by this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
	}
}

void sb_handle_test (void)
{
    switch (SB_dsp.command) {
	case 0xE8:
		if (SB_info.version >= SB_20) {
			S_printf("SB: Read %x from test register.\n", 
				 SB_dsp.test);
			dsp_write_output(SB_dsp.test);
			}
			else {
			S_printf("SB: read from test register not supported by SB version.\n");
		}
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	case 0xE4:
		if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
			if (SB_info.version >= SB_20) {
				SB_dsp.test = SB_dsp.parameter;
			}
			else {
				S_printf("SB: write to test register not in this SB version.\n");
			}
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}
		break;

	default:
		S_printf ("SB: Unknown test 0x%x\n", SB_dsp.command);
		SB_dsp.command = SB_NO_DSP_COMMAND;
    }
}

/* 
 * DANG_FIXTHIS Sine Generation is unimplemented 
 */
void sb_do_sine (void)
{
	S_printf("SB: Start Sine generator. (unimplemented)\n");
	SB_dsp.time_constant = 0xC0;
	
	if (SB_info.version < SB_16) {
		sb_enable_speaker();
	}

	SB_dsp.command = SB_NO_DSP_COMMAND;
}

/* 
 * DANG_FIXTHIS AUX Status is Unimplemented 
 */
void sb_get_aux_status (void)
{
	if (SB_info.version > SB_PRO) {
		S_printf("SB: DSP aux. status (unimplemented)\n");
		dsp_write_output(18);
	}
	else {
		S_printf("SB: DSP aux. status not supported by SB version.\n");
	}
		SB_dsp.command = SB_NO_DSP_COMMAND;
}

void sb_handle_dac (void)
{
    switch (SB_dsp.command) {
	case 0x10:
		if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
			S_printf("SB: Direct 8-bit DAC write (%u)\n", 
				 SB_dsp.parameter);
			sb_write_DAC(8, SB_dsp.parameter);
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}

	default:
		S_printf ("Invalid DAC command 0x%x\n", SB_dsp.command);
		SB_dsp.command = SB_NO_DSP_COMMAND;
    }
}
    

void sb_dsp_handle_interrupt(void)
{
    switch (SB_dsp.command) {
	case 0xF2:
		/* Trigger the interrupt */
		pic_request(SB_info.irq);
		break;
    
	case 0xF3:
		/* 0xF3: 16-bit IRQ - SB16 */
		break;

	default:
		S_printf ("SB: Invalid Interrupt command 0x%x", SB_dsp.command);
		break;
    }

    SB_dsp.command = SB_NO_DSP_COMMAND;
}

/*
 * DANG_FIXTHIS Stero Input is no implemented
 */
void sb_stereo_input (void)
{
    switch (SB_dsp.command) {
	case 0xA0:
		S_printf ("SB: Enable Stereo Input not implemented\n");
		break;

	case 0xA8:
		S_printf ("SB: Disable Stereo Input not implemented\n");
		break;

	default:
		S_printf ("SB: Invalid Stereo Input command 0x%x\n",
			  SB_dsp.command);
		break;
    }

    SB_dsp.command = SB_NO_DSP_COMMAND;
}

void sb_handle_speaker (void)
{
    S_printf ("SB: In Handle Speaker (0x%x)\n", SB_dsp.command);

    switch (SB_dsp.command) {
	case 0xD1:
		sb_enable_speaker();
		break;

	case 0xD3:
		sb_disable_speaker();

		/* 
		 * in this case we shouldn't emulate
		 * dma, 'cause this is used to check
		 * the used dma-channel
		 * just generate the appropriate IRQ
		 * and the software should be happy
		 * XXX -- next time  - Michael
		 */

		break;

	case 0xD8:
		dsp_write_output (SB_info.speaker);
		break;

	default:
		S_printf ("SB: Invalid speaker command 0x%x\n", 
			  SB_dsp.command);
		break;
    }

    SB_dsp.command = SB_NO_DSP_COMMAND;
}

void sb_get_version (void)
{
	S_printf("SB: Query Version\n");
	dsp_write_output(SB_info.version >> 8);
	dsp_write_output(SB_info.version & 0xFF);
	SB_dsp.command = SB_NO_DSP_COMMAND;
}


/*
 * ADC Support 
 */

/*
 * DANG_FIXTHIS ADC is Unimplemented
 */

void sb_do_adc(void)
{
    switch (SB_dsp.command) {
	case 0x20:
		S_printf ("SB: 8-bit ADC (Unimplemented)\n");
 		dsp_write_output (0);
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	case 0x28:
		if (SB_info.version > SB_PRO) {
			S_printf("SB: Burst mode 8-bit ADC (Unimplemented)\n");
		}
 		dsp_write_output (0);
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

	default:
		S_printf ("SB: Invalid ADC command 0x%x\n", SB_dsp.command);
		SB_dsp.command = SB_NO_DSP_COMMAND;
    }

}

/*
 * DMA Support
 * ===========
 */

/*
 * SB operations use READ, SINGLE-MODE
 */

void pause_dsp_dma(void)
{
  if (SB_driver.DMA_pause != NULL) {
    (*SB_driver.DMA_pause)();
  }
#ifdef FUSSY_SBEMU
  else {
    S_printf ("SB: Optional function 'DMA_pause' not provided.\n");
  }
#endif /* FUSSY_SBEMU */

  dma_drop_DREQ(config.sb_dma);

  SB_dsp.command = SB_NO_DSP_COMMAND;
}

void restart_dsp_dma(void)
{
  if (SB_driver.DMA_resume != NULL) {
    (*SB_driver.DMA_resume)();
  }
#ifdef FUSSY_SBEMU
  else {
    S_printf ("SB: Optional function 'DMA_resume' not provided.\n");
  }
#endif /* FUSSY_SBEMU */

  dma_assert_DREQ(config.sb_dma);

  SB_dsp.command = SB_NO_DSP_COMMAND;
}


/*
 * Sets the length parameter. Returns 1 when both bytes are set, and 0
 * otherwise.
 */

int sb_set_length(Bit16u *variable)
{
	if (SB_dsp.have_parameter != SB_PARAMETER_EMPTY) {
		if (!SB_dsp.write_size_mode) {
			*variable = SB_dsp.parameter;
			SB_dsp.write_size_mode = !SB_dsp.write_size_mode;
			return 0;
		}
		else {
			*variable += (SB_dsp.parameter << 8) + 1;
			SB_dsp.write_size_mode = !SB_dsp.write_size_mode;
			return 1;
		}
	} else {
		return 0;
	}
}

void start_dsp_dma(void)
{
  Bit8u command;

  command = SB_dsp.command;

  switch (command) {
	case 0x14:
	case 0x16:
	case 0x17:
	case 0x24:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
		/* Multi-parameter DMA commands - Supported (!) */
		if (! sb_set_length(&SB_dsp.length)) {
			/* Not complete yet */
			return;
		} else {
			/* Need to add 1 for DMA transfers */
			SB_dsp.length ++;
		}
		break;

	case 0x1C:
		if (SB_info.version < SB_20) {
			S_printf("SB: 8-bit Auto-Init DMA DAC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;

	case 0x1F:
		if (SB_info.version < SB_20) {
			S_printf("SB: 2-bit Auto-Init DMA DAC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;

	case 0x2C:
		if (SB_info.version < SB_20) {
			S_printf("SB: 8-bit Auto-Init DMA ADC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;

	case 0x7D:
		if (SB_info.version < SB_20) {
			S_printf ("SB: 4-bit Auto-Init ADPCM DMA DAC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;

	case 0x7F:
		if (SB_info.version < SB_20) {
			S_printf ("SB: 2.6-bit Auto-Init ADPCM DMA DAC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;

	case 0x90:
		if (SB_info.version < SB_20 || SB_info.version > SB_PRO) {
			S_printf ("SB: 8-bit Auto-Init High Speed DMA DAC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;

	case 0x98:
		if (SB_info.version < SB_20 || SB_info.version > SB_PRO) {
			S_printf ("SB: 8-bit Auto-Init High Speed DMA ADC not supported on this SB version.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
			return;
		}
		break;


	default:
		S_printf("SB: Invalid or unsupported DMA sequence (0x%x)\n",
			 command);
		SB_dsp.command = SB_NO_DSP_COMMAND;

		return;
  }

  SB_dsp.command = SB_NO_DSP_COMMAND;

  S_printf("SB: Starting to open DMA access to DSP\n");

  if (!SB_info.speaker) {
    S_printf ("SB: Speaker not enabled\n");
    /* Michael thinks we should trigger the interrupt now */
    pic_request(SB_info.irq);    
    return;
  }

  if (SB_driver.DMA_start_init == NULL) {
    S_printf ("SB: Required function 'DMA_start_init' not provided.\n");
  }
  else {
    (*SB_driver.DMA_start_init)(command);
  }

  switch (command) {
  case 0x14: /* 8-bit DMA */
    S_printf ("SB: 8-bit DMA starting\n");
    break;
  case 0x1C: /* 8-bit DMA (Auto-Init) */
    S_printf ("SB: 8-bit DMA (Auto-Init) starting\n");
    break;
  case 0x90: /* 8-bit DMA (Auto-Init, High Speed) */
    S_printf ("SB: 8-bit DMA (High Speed, Auto-Init) starting\n");
    break;
  default:
		S_printf("SB: Unsupported DMA type (0x%x)\n", command);
    return;
    break;
  };

  if (SB_driver.DMA_start_complete == NULL) {
    S_printf ("SB: Required function 'DMA_start_complete' not provided.\n");
  }
  else {
    (*SB_driver.DMA_start_complete)();
  }

  /* Commence Firing ... */
  dma_assert_DREQ(config.sb_dma);
}

/*
 * DANG_FIXTHIS Stopping Auto-Init DMA is not implemented
 */
void stop_dsp_dma(void)
{
	Bit8u command;

	command = SB_dsp.command;
	SB_dsp.command = SB_NO_DSP_COMMAND;

	/*	switch (command) {
		}
	*/
}

void set_dma_blocksize(void) 
{
	if (SB_info.version >= SB_20) {
		if (sb_set_length (&SB_dsp.length)) {
			/* Always need to move 1 more byte */
			SB_dsp.length++;
			S_printf("SB: DMA blocksize set to %u.\n", 
				 SB_dsp.length);
		}
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
}

int sb_dma_handler (int status)
{
  int result;

  result = DMA_HANDLER_OK;

#ifdef EXCESSIVE_DEBUG
  S_printf ("SB: In DMA Handler\n");
#endif /* EXCESSIVE_DEBUG */

  switch (status) {
  case DMA_HANDLER_READ:
#ifdef EXCESSIVE_DEBUG
    S_printf ("SB: Asserting DACK\n");
#endif /* EXCESSIVE_DEBUG */
    dma_assert_DACK(config.sb_dma);

/* AM - Never starts up if we do this! */
/*		S_printf("SB: [crisk] ... and dropping DREQ\n"); */
/*		dma_drop_DREQ(config.sb_dma); */
    return DMA_HANDLER_OK;
    break;

  case DMA_HANDLER_WRITE:
    S_printf ("SB: Handled WRITE response (!) INVALID !!!!\n");
    return DMA_HANDLER_NOT_OK;
    break;

  case DMA_HANDLER_ERROR:
    S_printf ("SB: Error in DMA\n");
    return DMA_HANDLER_NOT_OK;
    break;

  case DMA_HANDLER_DONE:
    if (SB_driver.DMA_complete_test != NULL) {
      result = (*SB_driver.DMA_complete_test)();
      /*      result = linux_sb_dma_complete_test();*/
    }
#ifdef FUSSY_SBEMU
    else {
      S_printf ("SB: Optional function 'DMA_complete_test' not provided.\n");
    }
#endif /* FUSSY_SBEMU */

S_printf ("SB: Returned from completion test.\n");

    if (result == DMA_HANDLER_OK) {
#ifdef EXCESSIVE_DEBUG
      S_printf ("SB: Asserting DACK & triggering Interrupt\n");
#endif /* EXCESSIVE_DEBUG */
      dma_assert_DACK(config.sb_dma);

      /* Finally, trigger the interrupt */
      pic_request(SB_info.irq);    

      return DMA_HANDLER_OK;
    } else {
      return DMA_HANDLER_NOT_OK;
    }
  };

  return DMA_HANDLER_NOT_OK;
}

void sb_irq_trigger (void)
{
  S_printf ("SB: Interrupt activated.\n");

	if (SB_driver.DMA_complete != NULL) {
		(*SB_driver.DMA_complete)();
	}
#ifdef FUSSY_SBEMU
	else {
		S_printf ("SB: Optional function 'DMA_complete' not provided.\n");
	}
#endif /* FUSSY_SBEMU */

  do_irq();
}


/*
 * Sound Initialisation
 * ====================
 */

void sound_init(void)
{
  sb_init();
  fm_init();
  mpu401_init();
}

static void sb_init(void)
{
  emu_iodev_t  io_device;

  S_printf ("SB: SB Initialisation\n");

  /* First - Check if the DMA/IRQ values make sense */

  /* Must have an IRQ between 1 & 15 inclusive */
  if (config.sb_irq < 1 || config.sb_irq > 15 ) {
    S_printf ("SB: Invalid IRQ (%d). SB Disabled.\n", config.sb_irq);
    SB_info.version = SB_NONE;
    return;
  }

  /* Must have a DMA between 0 & 7, excluding 4 [unsigned, so don't test < 0]*/
  if (config.sb_dma == 4 || config.sb_dma > 7) {
    S_printf ("SB: Invalid DMA channel (%d). SB Disabled.\n", config.sb_dma);
    SB_info.version = SB_NONE;
    return;
  }

  SB_info.version = SB_driver_init();

  switch (SB_info.version) {
  case SB_NONE:
    S_printf ("SB: No SB emulation available. Disabling SB\n");
    return;
    break;
  case SB_OLD:
    S_printf ("SB: \"Old\" SB emulation available\n");
    break;
  case SB_20:
    S_printf ("SB: SB 2.0 emulation available\n");
    break;
  case SB_PRO:
    S_printf ("SB: SB PRO emulation available\n");
    break;
  case SB_16:
    S_printf ("SB: SB 16 emulation available\n");
    break;
  default:
    S_printf ("SB: Incorrect value for emulation. Disabling SB.\n");
    SB_info.version = SB_NONE;
    return;
  }

  /* SB Emulation */
  io_device.read_portb   = sb_io_read;
  io_device.write_portb  = sb_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.handler_name = "SB Emulation";
  io_device.start_addr   = config.sb_base;
  io_device.end_addr     = config.sb_base+ 0x013;
  io_device.irq          = config.sb_irq;
  port_register_handler(io_device);

  /* Register the Interrupt */

  SB_info.irq = pic_irq_list[config.sb_irq];

  /* We let DOSEMU handle the interrupt */
  pic_seti(SB_info.irq, sb_irq_trigger, 0);
  pic_unmaski(SB_info.irq);

  S_printf ("SB: Initialisation - Base 0x%03x, IRQ %d, DMA %d\n", 
	    config.sb_base, config.sb_irq, config.sb_dma);

}

static void fm_init(void)
{
  emu_iodev_t  io_device;
  
  S_printf ("SB: FM Initialisation\n");

  /* This is the FM (Adlib + Advanced Adlib) */
  io_device.read_portb   = adlib_io_read;
  io_device.write_portb  = adlib_io_write;
  io_device.handler_name = "Adlib (+ Advanced) Emulation";
  io_device.start_addr   = 0x388;
  io_device.end_addr     = 0x38B;
  io_device.irq          = EMU_NO_IRQ;
  port_register_handler(io_device);

  FM_driver_init();
}

static void mpu401_init(void)
{
  emu_iodev_t  io_device;
  
  S_printf ("MPU401: MPU-401 Initialisation\n");

  /* This is the MPU-401 */
  io_device.read_portb   = mpu401_io_read;
  io_device.write_portb  = mpu401_io_write;
  io_device.handler_name = "Midi Emulation";
  io_device.start_addr   = config.mpu401_base;
  io_device.end_addr     = config.mpu401_base + 0x001;
  io_device.irq          = EMU_NO_IRQ;
  port_register_handler(io_device);

  S_printf ("MPU401: MPU-401 Initialisation - Base 0x%03x \n", 
	    config.mpu401_base);

	mpu401_info.isdata=TRUE;

  MPU_driver_init();
}


/*
 * Sound Reset
 * ===========
 */

void sound_reset(void)
{
  sb_reset();
  fm_reset();
  mpu401_reset();
}

static void sb_reset (void)
{
  S_printf ("SB: Resetting SB\n");

  if (SB_info.version != SB_NONE) {
  sb_disable_speaker();

  dsp_clear_output ();

  SB_dsp.last_write = 0;
  SB_info.mixer_index = 0;
  SB_dsp.write_size_mode = 0;

  SB_driver_reset();
  }
}

static void fm_reset (void)
{
  S_printf ("SB: Resetting FM\n");

  FM_driver_reset();
}

static void mpu401_reset (void)
{
	S_printf("MPU401: Resetting MPU-401\n");

  MPU_driver_reset();
}

static void sb_write_DAC (int bits, __u8 value)
{
  S_printf ("SB: Direct DAC write (%u bits)\n", bits);
  
  if (SB_driver.DAC_write == NULL) {
    S_printf ("SB: Required function 'DAC_write' not provided.\n");
  }
  else {
    (*SB_driver.DAC_write)(bits, value);
  }
}


/*
 * Miscellaneous Functions
 */

void sb_enable_speaker (void)
{
  if (SB_info.speaker) {
    S_printf ("SB: Speaker already Enabled. Ignoring\n");
    return;
  }

  S_printf ("SB: Enabling Speaker\n");

  if (SB_driver.speaker_on == NULL) {
    S_printf ("SB: Required function 'speaker_on' not provided.\n");
  }
  else {
    (*SB_driver.speaker_on)();
  }

  SB_info.speaker = 1;

  sb_set_speed ();
}

void sb_disable_speaker(void)
{
  S_printf ("SB: Disabling Speaker\n");

  if (SB_driver.speaker_off == NULL) {
    S_printf ("SB: Required function 'speaker_off' not provided.\n");
  }
  else {
    (*SB_driver.speaker_off)();
  }

  SB_info.speaker = 0;
}

static __u8 sb_read_mixer(__u8 ch)
{
  S_printf ("SB: Read mixer setting on channel %u.\n", ch);

  if (SB_driver.read_mixer == NULL) {
    S_printf ("SB: Required function 'read_mixer' not provided.\n");
    return 0xFF;
  }
  else {
    return (*SB_driver.read_mixer)(ch);
  }
}

static void sb_write_mixer (int ch, __u8 value)
{
  S_printf ("SB: Read mixer setting on channel %u.\n", ch);

  if (SB_driver.write_mixer == NULL) {
    S_printf ("SB: Required function 'write_mixer' not provided.\n");
  }
  else {
    return (*SB_driver.write_mixer)(ch, value);
  }
}
