/*
 * DANG_BEGIN_MODULE
 *
 * Description: SB emulation - Getting There ...!
 * 
 * maintainer: 
 *   Joel N. Weber II
 *   Alistair MacDonald <alistair@slitesys.demon.co.uk>
 *   Michael Beck
 *   Rutger Nijlunsing <csg465@wing4.wing.rug.nl>
 *
 * DANG_END_MODULE
 *
 * $Date: $
 * $Source: $
 * $Revision: $
 * $State: $
 *
 * $Log: $
 *
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

/*
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
*/

/* MPU401 data */
static int mpu401avail = 1;  /* 1 if byte available at input */
static int mpufile=0; /* File descriptor */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Internal Functions */

inline void dsp_write_output(__u8 value);
inline void dsp_clear_output(void);
inline __u8 dsp_read_output(void);

static void start_dsp_dma(__u32 command);
static void restart_dsp_dma(void);
static void pause_dsp_dma(void);

void sb_dma_handler(int status);
void sb_irq_trigger (void);

void sb_set_speed (void);
static void sb_enable_speaker (void);
static void sb_disable_speaker(void);
static void sb_write_DAC (int bits, __u8 value);

static void dsp_do_copyright(void);

static __u8 sb_read_mixer(__u8 ch);
static void sb_write_mixer(int ch, __u8 value);

/* FIXME: The file header needs tidying up a _LOT_ ! */

static void sb_init(void);
static void fm_init(void);
static void mpu401_init(void);

static void sb_reset(void);
static void fm_reset(void);
static void mpu401_reset(void);

__u8 dsp_read_output(void);
void dsp_write_output(__u8 value);

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

Bit8u sb_io_read(Bit32u port)
{
  __u32 addr;
  __u8 value;

  addr = port - config.sb_base;

  switch (addr)
  {

    /* == FM Music == */

  case 0x00: /* FM Music Left Status Port - SBPro */
  case 0x08: /* FM Music Compatible Status Port - SB (Alias to 0x00 on SBPro)*/
    S_printf("SB: read from Left FM status port.\n");
    return fm_io_read (FM_LEFT_STATUS);
  case 0x02: /* FM Music Right Status Port - SBPro */
    /* FIXME: Unimplemented */
    if (SB_info.version > SB_PRO) {
      S_printf("SB: read from Right FM status port.\n");
      return fm_io_read (FM_RIGHT_STATUS);
    }
    else {
      S_printf("SB: read from Right FM status port not supported on this SB version.\n");
      return 0xff;
    }
    
  case 0x05: /* Mixer Data Register */
    if (SB_info.version > SB_20) {
      switch (SB_info.mixer_index)
       {
       case 0x0E: 
	 return SB_dsp.stereo;
       case 0x22: 
	 return sb_read_mixer(SB_MIXER_VOLUME);
       case 0x04: 
	 return sb_read_mixer(SB_MIXER_PCM);
       case 0x26: 
	 return sb_read_mixer(SB_MIXER_SYNTH);
       case 0x28: 
	 return sb_read_mixer(SB_MIXER_CD);
       case 0x2E: 
	 return sb_read_mixer(SB_MIXER_LINE);
       case 0x0A: 
	 return sb_read_mixer(SB_MIXER_MIC);
       default:
         S_printf("SB: invalid read from mixer (%x)\n", SB_info.mixer_index);
         return -1;
       };
     }
     S_printf("SB: mixer not supported on this SB version.\n");
     return -1;

   case 0x06: /* Reset ? */
     S_printf("SB: read from Reset address\n");
     return 0; /* Some programms read this whilst resetting */

     /* Alias to 0x00 ? */

   case 0x0A: /* DSP Read Data - SB */
     value = dsp_read_output(); 
     S_printf ("SB: Read 0x%x from SB DSP\n", value);
     return value;
     break;

   case 0x0C: /* DSP Write Buffer Status */
     S_printf ("SB: Write? %x\n", SB_dsp.ready);
     return SB_dsp.ready;

   case 0x0D: /* DSP Timer Interrupt Clear - SB16 ? */
     S_printf("SB: read 16-bit timer interrupt status. Not Implemented.\n");
     return -1;

   case 0x0E: /* DSP Data Available Status/DSP 8-bit IRQ Ack - SB */
     S_printf ("SB: Ready? %x\n", SB_dsp.data);
     return SB_dsp.data; 
     break;

   case 0x0F: /* 0x0F: DSP 16-bit IRQ - SB16 */
     S_printf("SB: read 16-bit DSP IRQ. Not Implemented.\n");
     return -1;
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
   case 0x11: /* CD-ROM Status Port - SBPro */
     if (SB_info.version > SB_PRO) {
       S_printf("SB: read from CD-ROM status port.\n");
       return 0xFE;
     }
     else {
       S_printf("SB: CD-ROM not supported in this version.\n");
       return 0xFF;
     }
    
  default: S_printf ("SB: %lx is an unhandled read port.\n", port);
  };
   return 0xFF;
}

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
  default:       S_printf ("%lx is an unhandled read port\n", port);
  };
  return 0;
}

Bit8u fm_io_read (Bit32u port)
{
  switch (port){
  case ADLIB_STATUS:
    /* FIXME: Unimplemented */
    return 31;
    break;
  case ADV_ADLIB_STATUS:
    /* FIXME: Unimplemented */
    return 31;
    break;
  };
  
  return 0;
}

Bit8u mpu401_io_read(Bit32u port)
{
  __u32 addr;
  Bit8u r=0xff;   /* Rutger: _not_ INT ! */
  
  addr = port - config.mpu401_base;
  
  switch(addr) {
  case 0:
    /* Read data port */
    r=0xfe; /* Whatever happened before, send a MPU_ACK */
    mpu401avail=0;
    S_printf("MPU401: Read data port = 0x%02x\n",r);
    break;
  case 1:
     /* Read status port */
    /* 0x40=OUTPUT_AVAIL; 0x80=INPUT_AVAIL */
    r=0xff & (~0x40); /* Output is always possible */
    if (mpu401avail) r &= (~0x80);
    S_printf("MPU401: Read status port = 0x%02x\n",r);
  }
  return r;
}


/*
 * Main IO Routines - Write
 * ========================
 */

void sb_io_write(Bit32u port, Bit8u value)
{
  __u32 addr;
  
  addr = port - config.sb_base;
  
  switch (addr) {
    
    /* == FM MUSIC or C/MS == */
    
  case 0x00: /* C/MS Data Port (1-6) - SB Only */
    /* FM Music Left Register Port - SBPro */
    if (SB_info.version >= SB_PRO) {
      S_printf ("SB: Write 0x%x to 0x%x (FM Left Register)\n", value, addr);
      fm_io_write (FM_LEFT_REGISTER, value);
    }
    else {
      S_printf ("SB: Write 0x%x to 0x%x (C/MS 1-6 Data)\n", value, addr);
    }
    break;
  case 0x01: /* C/MS Register Port (1-6) - SB Only */
    /* FM Music Left Data Register - SBPro */
    if (SB_info.version >= SB_PRO) {
      S_printf ("SB: Write 0x%x to 0x%x (FM Left Data)\n", value, addr);
      fm_io_write (FM_LEFT_DATA, value);
    }
    else {
      S_printf ("SB: Write 0x%x to 0x%x (C/MS 1-6 Register)\n", value, addr);
    }
    break;
  case 0x02: /* C/MS Data Port (7-12) - SB Only */
    /* FM Music Right Register Port - SBPro */
    if (SB_info.version >= SB_PRO) {
      S_printf ("SB: Write 0x%x to 0x%x (FM Right Register)\n", value, addr);
      fm_io_write (FM_RIGHT_REGISTER, value);
    }
    else {
      S_printf ("SB: Write 0x%x to 0x%x (C/MS 7-12 Data)\n", value, addr);
    }
    break;
  case 0x03: /* C/MS Register Port (7-12) - SB Only */
    /* FM Music Right Data Register - SBPro */
    if (SB_info.version >= SB_PRO) {
      S_printf ("SB: Write 0x%x to 0x%x (FM Right Data)\n", value, addr);
      fm_io_write (FM_RIGHT_DATA, value);
    }
    else {
      S_printf ("SB: Write 0x%x to 0x%x (C/MS 7-12 Register)\n", value, addr);
    }
    break;
    
    /* == MIXER == */
    
  case 0x04: /* Mixer Register Port - SBPro */
    if (SB_info.version > SB_20) {
      SB_info.mixer_index = value; 
    }
    break;
  case 0x05: /* Mixer Data Register - SBPro */
    if (SB_info.version > SB_20) {
      switch (SB_info.mixer_index) 
      {
	/* 0 is the reset register, but we'll ignore it */
	/* 0x0C is ignored; it sets record source and a filter */
      case 0x0E: 
	SB_dsp.stereo = value & 2;
	S_printf ("SB: Stereo Mode set to %d\n", SB_dsp.stereo);
	break;			/* I ignored the output filter */
      case 0x22: 
	sb_write_mixer(SB_MIXER_VOLUME, value);
	break;
      case 0x04: 
	sb_write_mixer(SB_MIXER_PCM, value);
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
      case 0x0A: 
	sb_write_mixer(SB_MIXER_MIC, value);
	break;
      }; 
    }
    break;
    
    /* == RESET == */
    
  case 0x06: /* reset */
    sb_reset();
    dsp_write_output(0xAA);
    break;
    
    /* == FM MUSIC == */
    
  case 0x08: /* FM Music Register Port - SB */
    /* Alias for 0x00 - SBPro */
  case 0x09: /* FM Music Data Port - SB */
    /* Alias for 0x01 - SBPro */
    S_printf ("SB: Write 0x%x to 0x%x (FM Music Port)\n", value, addr);
    /* FIXME: Unimplemented */
    break;
    
    /* == DSP == */
    
  case 0x0C: /* dsp write register */

      /* ALL commands set SB_dsp.command to SB_NO_DSP_COMMAND when they finish */
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

      /* Unknowns */
      /* == STATUS == */
      /* 0x03: ASP Status - SB16ASP */
      /* 0x05: ??? - SB16ASP */

    if (SB_dsp.have_parameter == SB_PARAMETER_EMPTY) {
      	/* The following commands take no parameters, or we need  */
      
      switch (value) 
      {
	/* == STATUS == */
	
      case 0x04: /* DSP Status - SB2.0-Pro2 - Obselete */
	/* ASP ? - SB16ASP */
	/* FIXME: Unimplemented */
	if (SB_info.version >= SB_20 && SB_info.version <= SB_PRO) {
		S_printf("SB: Request for SB2.0/SBPRO status (Unimplemented)\n");
	}
	dsp_write_output(0);
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == OUTPUT COMMANDS == */
	
      case 0x1C: /* DMA 8-bit DAC (Auto-Init) - SB2.0 */
      case 0x1F: /* DMA 2-bit ADPCM DAC (Reference, Auto-Init) - SB2.0 */
	if (SB_info.version >= SB_20) {
		start_dsp_dma(SB_dsp.command);
	}
	else {
		S_printf("SB: Auto-Init DMA not supported on this SB version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == INPUT COMMANDS == */
	
      case 0x20: /* Direct 8-bit ADC - SB */
	/* FIXME: Unimplemented */
	S_printf("SB: 8-bit ADC (Unimplemented)\n");
	dsp_write_output(0);
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
      case 0x28:	/* Direct 8-bit ADC (Burst) - SBPro2 */
	/* FIXME: Unimplemented */
	if (SB_info.version > SB_PRO) {
		S_printf("SB: Burst mode 8-bit ADC (Unimplemented)\n");
	}
	else {
		S_printf("SB: Burst mode 8-bit ADC not supported by SB version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
      case 0x2C: /* DMA 8-bit DAC (Auto-Init) - SB2.0 */
	if (SB_info.version >= SB_20) {
		start_dsp_dma(SB_dsp.command);
	}
	else {
		S_printf("SB: Auto-Init DMA not supported on this SB version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == MIDI == */
	
      case 0x30: /* Midi Read Poll - SB */
	/* FIXME: Unimplemented */
	S_printf ("SB: Attempt to read from SB Midi (Poll)\n");
	dsp_write_output(0);
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
      case 0x31: /* Midi Read Interrupt - SB */
	/* FIXME: Unimplemented */
	S_printf ("SB: Attempt to read from SB Midi (Interrupt)\n");
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
      case 0x32: /* Midi Read Timestamp Poll - SB ? */
	/* FIXME: Unimplemented */
	S_printf ("SB: Attempt to read from SB Midi (Poll)\n");
	dsp_write_output(0); dsp_write_output(0);
	dsp_write_output(0); dsp_write_output(0);
	SB_dsp.command = SB_NO_DSP_COMMAND;
      case 0x33: /* Midi Read Timestamp Interrupt - SB ? */
	/* FIXME: Unimplemented */
	S_printf ("SB: Attempt to read from SB Midi Timestamp (Interrupt)\n");
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == OUTPUT == */
	
      case 0x45:	/* Continue Auto-Init 8-bit DMA - SB16 */
      case 0x47:	/* Continue Auto-Init 16-bit DMA - SB16 */
	/* FIXME: Unimplemented */
	if (SB_info.version >= SB_16) {
		S_printf("SB: Continue Auto-init DMA. (unimplemented)\n");
	}
	else {
		S_printf("SB: Continue Auto-init DMA unsupported on this SB.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
      case 0x7D: /* DMA 4-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
      case 0x7F: /* DMA 2.6-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
	if (SB_info.version >= SB_20) {
		start_dsp_dma(value);
	}
	else {
		S_printf("SB: Auto-init DMA not supported on this SB version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
      case 0x90: /* DMA 8-bit DAC (High Speed, Auto-Init) - SB2.0-Pro2 */
	/* FIXME: Unimplemented */
	if (SB_info.version >= SB_20 && SB_info.version <= SB_PRO) {
		S_printf("SB: Auto-init DAC DMA (unimplemented.)\n");
	}
	else {
		S_printf("SB: Auto-init DAC DMA not supported on this SB.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == INPUT == */
	
      case 0x98: /* DMA 8-bit ADC (High Speed, Auto-Init) - SB2.0-Pro2 */
	/* FIXME: Unimplemented */
	if (SB_info.version >= SB_20 && SB_info.version <= SB_PRO) {
		S_printf("SB: Auto-init ADC DMA (unimplemented.)\n");
	}
	else {
		S_printf("SB: Auto-init ADC DMA not supported on this SB.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == STEREO == */
	
      case 0xA0: /* Enable Stereo Input - SBPro Only */
	/* FIXME: Unimplemented */
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
      case 0xA8: /* Disable Stereo Input - SBPro Only */
	/* FIXME: Unimplemented */
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == DMA == */
	
      case 0xD0: /* Halt 8-bit DMA - SB */
	pause_dsp_dma(); 
	break;
	
      case 0xD4: /* Continue 8-bit DMA - SB */
	restart_dsp_dma(); 
	break;
	
      case 0xD5:	/* Halt 16-bit DMA - SB16 */
      case 0xD6:	/* Continue 16-bit DMA - SB16 */
	if (SB_info.version >= SB_16) {
		S_printf("SB: 16-bit DMA operations unimplemented\n");
	}
	else {
		S_printf("SB: 16-bit DMA operations not supported on this SB.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == SPEAKER == */
	
      case 0xD1: /* Enable Speaker - SB */
	sb_enable_speaker ();
	break;
      case 0xD3: /* Disable Speaker - SB */
	sb_disable_speaker ();
	/* 
	 * in this case we shouldn't emulate
	 * dma, 'cause this is used to check
	 * the used dma-channel
	 * just generate the appropriate IRQ
	 * and the software should be happy
	 * XXX -- next time  - Michael
	 */
	break;
	
      case 0xD8: /* Speaker Status */
	dsp_write_output(SB_info.speaker);
	
	/* == DMA == */
	
      case 0xD9: /* Exit Auto-Init 16-bit DMA - SB16 */
      case 0xDA: /* Exit Auto-Init 8-bit DMA - SB2.0 */
	/* FIXME: Unimplemented */
	SB_dsp.command = SB_NO_DSP_COMMAND;
	
	/* == DSP IDENTIFICATION == */
	
      case 0xE1: /* DSP Version - SB */
	S_printf ("SB: Query Version\n");
	dsp_write_output(SB_info.version >> 8);
	dsp_write_output(SB_info.version & 0xFF);
	break;
      case 0xE3: /* DSP Copyright - SBPro2 ? */
	if (SB_info.version > SB_PRO) {
	  S_printf("SB: DSP Copyright requested.\n");
	  dsp_do_copyright();
	}
	else {
		S_printf("SB: DSP copyright not supported by this SB Version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == TEST == */
	
      case 0xE8: /* Read from Test - SB2.0 */
	if (SB_info.version >= SB_20) {
	  S_printf("SB: Read %x from test register.\n", SB_dsp.test);
	  dsp_write_output(SB_dsp.test);
	}
	else {
		S_printf("SB: read from test register not supported by SB version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
      case 0xF0: /* Sine Generator - SB */
	/* FIXME: Unimplemented */
	S_printf("SB: Start Sine generator. (unimplemented)\n");
	SB_dsp.time_constant = 0xC0;

	if (SB_info.version < SB_16)
		sb_enable_speaker();

	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == STATUS == */
	
      case 0xF1: /* DSP Auxiliary Status - SBPro2 */
	/* FIXME: Unimplemented */
	if (SB_info.version > SB_PRO) {
	  S_printf("SB: DSP aux. status (unimplemented)\n");
	  dsp_write_output(18);
	}
	else {
		S_printf("SB: DSP aux. status not supported by SB version.\n");
	}
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;
	
	/* == IRQ == */
	
      case 0xF2: /* 8-bit IRQ - SB */
	/* Trigger the interrupt */
	pic_request(SB_info.irq);
	SB_dsp.command = SB_NO_DSP_COMMAND;
	break;

	/* 0xF3: 16-bit IRQ - SB16 */
	
	/* 0xFB: DSP Status - SB16 */
	/* 0xFC: DSP Auxiliary Status - SB16 */
	/* 0xFD: DSP Command Status - SB16 */
	
      };
    }
    else {		/* SB_dsp.have_parameter != SB_PARAMETER_EMPTY */
	/* The following commands take at least one parameter */
	switch (SB_dsp.command) {
	case 0x10:	/* Direct 8-bit DAC - SB */
		S_printf("SB: Direct 8-bit DAC write (%u)\n",value);
		sb_write_DAC(8, value);
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

		/* == DMA == */

	case 0x14:	/* DMA 8-bit DAC - SB */
	case 0x16:	/* DMA 2-bit ADPCM DAC - SB */
	case 0x17:	/* DMA 2-bit ADPCM DAC (Reference) - SB */
	case 0x24:	/* DMA 8-bit DAC - SB */
	case 0x74:	/* DMA 4-bit ADPCM DAC - SB */
	case 0x75:	/* DMA 4-bit ADPCM DAC (Reference) - SB */
	case 0x76:	/* DMA 2.6-bit ADPCM DAC - SB */
	case 0x77:	/* DMA 2.6-bit ADPCM DAC (Reference) - SB */
		if (!SB_dsp.write_size_mode) {
			SB_dsp.length = SB_dsp.parameter;
		}
		else {
			SB_dsp.length += (SB_dsp.parameter << 8) + 1;
			start_dsp_dma(SB_dsp.command);
			SB_dsp.command = SB_NO_DSP_COMMAND;
		};
		SB_dsp.write_size_mode = !SB_dsp.write_size_mode;
		break;

		/* == MIDI == */

	case 0x35:	/* Midi Read/Write Poll (UART) - SB2.0 */
	case 0x36:	/* Midi Read Interrupt/Write Poll (UART) - SB2.0 ? */
	case 0x37:	/* Midi Read Interrupt Timestamp/Write Poll (UART) - SB2.0? */
		/* FIXME: Unimplemented */
		if (SB_info.version >= SB_20) {
			S_printf("SB: Write %u to SB 2.0 MIDI port.\n", SB_dsp.parameter);
			/* This is terminated by a DSP reset */
		}
		else {
			S_printf("SB: Write to SB 2.0 MIDI port when not an SB 2.0.\n");
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}
		break;
	case 0x38:	/* Midi Write Poll - SB */
		/* FIXME: Unimplemented */
		S_printf("SB: Write %u to SB MIDI port.\n", SB_dsp.parameter);
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;

		/* == SAMPLE SPEED == */

	case 0x40:	/* Set Time Constant */
		SB_dsp.time_constant = SB_dsp.parameter;
		SB_dsp.last_write = SB_NO_DSP_COMMAND;
		sb_set_speed();
		break;
	case 0x41:	/* Set Sample Rate - SB16 */
		/* FIXME: Unimplemented */
		if (SB_info.version >= SB_16) {
			if (!SB_dsp.write_size_mode) {
				SB_dsp.length = SB_dsp.parameter;
			}
			else {
				SB_dsp.length += (SB_dsp.parameter << 8);
				S_printf("SB: sample rate set to %d. (unimplemented)\n", SB_dsp.length);
				SB_dsp.command = SB_NO_DSP_COMMAND;
			};
			SB_dsp.write_size_mode = !SB_dsp.write_size_mode;
			break;
		}
		else {
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}

		/* == DMA == */

	case 0x48:	/* Set DMA Block Size - SB2.0 */
		if (SB_info.version >= SB_20) {
			if (!SB_dsp.write_size_mode) {
				SB_dsp.length = SB_dsp.parameter;
			}
			else {
				SB_dsp.length += (SB_dsp.parameter << 8);
				S_printf("SB: DMA blocksize set to %d.\n", SB_dsp.length);
				SB_dsp.command = SB_NO_DSP_COMMAND;
			};
			SB_dsp.write_size_mode = !SB_dsp.write_size_mode;
			break;
		}
		else {
			SB_dsp.command = SB_NO_DSP_COMMAND;
		}

		/* == OUTPUT == */

	case 0x80:	/* Silence DAC - SB */
		/* FIXME: Unimplemented */
		if (!SB_dsp.write_size_mode) {
			SB_dsp.length = SB_dsp.parameter;
		}
		else {
			SB_dsp.length += (SB_dsp.parameter << 8);
			S_printf("SB: DAC silence length set to %d. (Unimplemented)\n", SB_dsp.length);
			SB_dsp.command = SB_NO_DSP_COMMAND;
		};
		SB_dsp.write_size_mode = !SB_dsp.write_size_mode;
		break;

		/* == DSP IDENTIFICATION == */

	case 0xE0:	/* DSP Identification - SB2.0 */
		if (SB_info.version >= SB_20) {
			S_printf("SB: Identify SB2.0 DSP.\n");
			dsp_write_output(~SB_dsp.parameter);
		}
		else {
			S_printf("SB: Identify DSP not supported by this SB version.\n");
		}
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;
	case 0xE4:	/* Write to Test - SB2.0 */
		if (SB_info.version >= SB_20) {
			SB_dsp.test = SB_dsp.parameter;
		}
		else {
			S_printf("SB: write to test register not in this SB version.\n");
		}
		SB_dsp.command = SB_NO_DSP_COMMAND;
		break;
	}
    }
    break;
    
    /* 0x0D: Timer Interrupt Clear - SB16 */
    /* 0x10 - 0x13: CD-ROM - SBPro ***IGNORED*** */
    
  default:   S_printf ("SB: %x is an unhandled write port\n", addr);
  };
}

void adlib_io_write(Bit32u port, Bit8u value)
{
  /* Base Port for Adlib is 0x388 */
  /* Base Port for Adv. Adlib is 0x38a */

  switch (port) {
  case 0x388:  /* Adlib Register Port */
    S_printf ("Adlib: Write 0x%x to Register Port\n", value);
    fm_io_write (ADLIB_REGISTER, value);
    break;
  case 0x389:  /* Adlib Data Port */
    S_printf ("Adlib: Write 0x%x to Data Port\n", value);
    fm_io_write (ADLIB_DATA, value);
    break;
    
  case 0x38A:  /* Adv. Adlib Register Port */
    S_printf ("Adv_Adlib: Write 0x%x to Register Port\n", value);
    fm_io_write (ADV_ADLIB_REGISTER, value);
    break;
  case 0x38B:  /* Adv. Adlib Data Port */
    S_printf ("Adv_Adlib: Write 0x%x to Data Port\n", value);
    fm_io_write (ADV_ADLIB_DATA, value);
    break;
  };
}

void fm_io_write (Bit32u port, Bit8u value)
{
  switch (port){
  case ADLIB_REGISTER:
    /* FIXME: Unimplemented */
    break;
  case ADLIB_DATA:
    /* FIXME: Unimplemented */
    break;
  case ADV_ADLIB_REGISTER:
    /* FIXME: Unimplemented */
    break;
  case ADV_ADLIB_DATA:
    /* FIXME: Unimplemented */
    break;
  };
}


void mpu401_io_write(Bit32u port, Bit8u value)
{
  __u32 addr;

  addr = port - config.mpu401_base;

  switch (addr){
    case 0:
	/* Write data port */
	if (mpufile > 0) write(mpufile,&value,1);
	S_printf("MPU401: Write 0x%02x to data port\n",value);
	break;
    case 1:
	/* Write command port */
	if (port == (config.mpu401_base+1)) {
		mpu401avail=1; /* A command is sent: MPU_ACK it next time */
		S_printf("MPU401: Write 0x%02x to command port\n",value);
		break;
	}
	break;
    default: S_printf ("SB: %lx is an unhandled MPU-401 write port\n", port);
    }
}



static void dsp_do_copyright (void)
{
  char cs[]="(c) Copyright 1995 Alistair MacDonald";
  char *ptr;
  int i;

  for (i = strlen(cs) + 1, ptr = cs; i >= 0; ptr++, i--)
    dsp_write_output((__u8) *ptr);
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
}

void start_dsp_dma(__u32 command)
{
  S_printf ("SB: Starting to open DMA access to DSP\n");

  if (! SB_info.speaker)
  {
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

  switch(command)
  {
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
    S_printf ("SB: Unsupported DMA type (%x)\n", command);
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

void sb_dma_handler (int status)
{
#ifdef EXCESSIVE_DEBUG
  S_printf ("SB: In DMA Handler\n");
#endif /* EXCESSIVE_DEBUG */

  switch (status)
  {
  case DMA_HANDLER_READ:
#ifdef EXCESSIVE_DEBUG
    S_printf ("SB: Asserting DACK\n");
#endif /* EXCESSIVE_DEBUG */
    dma_assert_DACK(config.sb_dma);
    break;

  case DMA_HANDLER_WRITE:
    S_printf ("SB: Handled WRITE response (!) INVALID !!!!\n");
    break;

  case DMA_HANDLER_ERROR:
    S_printf ("SB: Error in DMA\n");
    break;

  case DMA_HANDLER_DONE:
#ifdef EXCESSIVE_DEBUG
    S_printf ("SB: Asserting DACK & triggering Interrupt\n");
#endif /* EXCESSIVE_DEBUG */
    dma_assert_DACK(config.sb_dma);

    /* Finally, trigger the interrupt */
    pic_request(SB_info.irq);    
  };

}

void sb_irq_trigger (void)
{
  /* Dummy trigger .... */
  S_printf ("SB: Interrupt activated.\n");
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

  SB_info.version = SB_driver_init();

  switch (SB_info.version) {
  case SB_NONE:
    S_printf ("SB: No SB emulation available\n");
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
    S_printf ("SB: Incorrect value for emulation. Setting to NONE\n");
    SB_info.version = SB_NONE;
  }
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

  /* FIXME: Rutger, this is _driver_ specific */
  /* Quick hack */
  mpufile=open("/var/run/dosemu-midi",O_WRONLY);
  /* NOTE: If it can't be opened, mpufile will be -1, which is checked
   *       above in mpu401_io_write()
   * FIXME: ... and where will mpufile closed? --Hans
   */
  if (mpufile < 0) S_printf("MPU401: Cannot open /var/run/dosemu-midi\n");

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

  sb_disable_speaker();

  dsp_clear_output ();

  SB_dsp.last_write = 0;
  SB_info.mixer_index = 0;
  SB_dsp.write_size_mode = 0;

  SB_driver_reset();
}

static void fm_reset (void)
{
  S_printf ("SB: Resetting FM\n");

  FM_driver_reset();
}

static void mpu401_reset (void)
{
  S_printf ("MPU401: Resetting MPU401\n");

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

void sb_set_speed (void)
{   
  __u16 real_speed;

  if (SB_dsp.stereo)
  {
    real_speed = 1000000 / ( 2 * (256 - SB_dsp.time_constant));
  }
  else
  {
    real_speed = 1000000 / (256 - SB_dsp.time_constant);
  }

  S_printf ("SB: Trying to set speed to %u Hz.\n", real_speed);

  if (SB_driver.set_speed == NULL) {
    S_printf ("SB: Required function 'set_speed' not provided.\n");
  }
  else {
    (*SB_driver.set_speed)(real_speed, SB_dsp.stereo);
  }
}

void sb_enable_speaker (void)
{
  if (SB_info.speaker)
  {
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
