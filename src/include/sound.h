/*
 ***************************************************************************
 *                                   Sound.h                               *
 *                                   -------                               *
 * The Sound Definitions are split into 3 sections, because the driver has *
 * been separated into 3 parts. This separates the MIDI, Digital and FM    *
 * drivers so that they can be supported on a piecemeal basis on different *
 * architectures.                                                          *
 *                                                             - Alistair  *
 ***************************************************************************
 */

#include "types.h"

/*
 ***************************************************************************
 * SB Support                                                              *
 ***************************************************************************
 */

/*
 * The different SoundBlaster versions returned by the init function.
 */

#define SB_NONE 0x000
#define SB_OLD	0x105
#define SB_20	0x201
#define SB_PRO	0x300
#define SB_16	0x405 /* Was 40D, but that is AWE32+ - AM */

/*
 * Various Status values
 */

#define SB_DATA_AVAIL    0x80
#define SB_DATA_UNAVAIL  0x00

#define SB_WRITE_AVAIL   0x00
#define SB_WRITE_UNAVAIL 0x80

/*
 * Mixer Definitions
 */

#define SB_MIXER_VOLUME  1
#define SB_MIXER_PCM     2
#define SB_MIXER_SYNTH   3
#define SB_MIXER_CD      4
#define SB_MIXER_LINE    5
#define SB_MIXER_MIC     6

/* 
 * SB information / states
 */

EXTERN struct sb_information_t {
  __u8  mixer_index;           /* Which Mixer channel to work on */
  __u16 irq;                   /* Internal IRQ (PIC) */
  __u8  speaker;               /* Speaker Status */
  __u16 version;               /* Version of the SB being emulated */
} SB_info;

/* 
 * DSP information / states 
 */

EXTERN struct DSP_information_t {
  __u8  channels;              /* Number of Channels on the DSP */
  __u8  time_constant;         /* The current Time constant for writes */
  __u16 sample_rate;           /* The current sample rate */
  __u8  test;                  /* Storage for the test value */
  __u8  stereo;                /* Is the device Stereo */
  __u8  ready;                 /* Is DSP Ready ? */
  __u8  data;                  /* Data is available */
  __u8  write_size_mode;       /* Are we writing the upper or lower byte */
  __u8  last_write;            /* First part of a multi-byte instruction */
  __u16 length;                /* Length of the DMA transfer */
  __u8  command;               /* DSP command in progress */
#define SB_NO_DSP_COMMAND 0
  __u8  parameter;             /* value of parameter */
  __u8  have_parameter;        /* Have we the parameter */
#define SB_PARAMETER_EMPTY 0
#define SB_PARAMETER_FULL  1
} SB_dsp;


/*
 * SB Driver - Architecture Specific Interface
 */

EXTERN struct SB_driver_t {

  /*
   * Mixer Functions
   */
  void  (* write_mixer)(int channel, __u8 value);
  __u8  (* read_mixer)(int channel);

  /*
   * Speaker Control
   */
  void  (* speaker_on)(void);
  void  (* speaker_off)(void);

  /*
   * Direct DAC access
   */
  void  (* DAC_write)(int bits, __u8 value);

  /*
   * DMA functions
   */
  void  (* DMA_start_init)(__u32);
  void  (* DMA_start_complete)(void);
  void  (* DMA_pause)(void);
  void  (* DMA_resume)(void);
  void  (* DMA_stop)(void);
  int   (* DMA_complete_test)(void);
  void  (* DMA_complete)(void);

  /*
   * Miscellaneous Functions
   */
  void  (* set_speed)(__u16 speed, __u8 stereo);

} SB_driver; 



/*
 ***************************************************************************
 * FM Support                                                              *
 ***************************************************************************
 */

#define ADLIB_REGISTER        0
#define ADLIB_STATUS          ADLIB_REGISTER
#define ADLIB_DATA            1
#define ADV_ADLIB_REGISTER    2
#define ADV_ADLIB_STATUS      ADV_ADLIB_REGISTER
#define ADV_ADLIB_DATA        3

#define FM_LEFT_REGISTER      ADLIB_REGISTER
#define FM_LEFT_STATUS        ADLIB_STATUS
#define FM_LEFT_DATA          ADLIB_DATA
#define FM_RIGHT_REGISTER     ADV_ADLIB_REGISTER
#define FM_RIGHT_STATUS       ADV_ADLIB_STATUS
#define FM_RIGHT_DATA         ADV_ADLIB_DATA

#define CMS_LOWER_DATA        0
#define CMS_LOWER_REGISTER    1
#define CMS_UPPER_DATA        2
#define CMS_UPPER_REGISTER    3

/* This is a Kludge! */
#define ADLIB_NORMAL   1
#define ADLIB_ADV      2
#define ADLIB_NONE     0

/*
 ***************************************************************************
 * MPU-401 Support                                                         *
 ***************************************************************************
 */

/* This is a Kludge! */  /* Are these relevant? (Rutger) */
#define MPU_NORMAL     1
#define MPU_NONE       0

EXTERN struct mpu401_info_t {
	/* MPU401 state data */
	int isdata;         /* TRUE iff byte available at input */
	/* Architecture specific procedures */
	void (*data_write)(__u8 data); /* process 1 MIDI byte */
} mpu401_info;


/*
 ***************************************************************************
 * Private Code                                                            *
 ***************************************************************************
 */

/* 
 * This is anded with the address to see if it's valid 
 */

#define SOUND_IO_MASK 0xFFF0
#define SOUND_IO_MASK2 0x0F

/* 
 * Output Queue information 
 */

/* 
 * How big the output buffer should be. This _MUST_ be a multiple of 2.
 * The queue is big, so that it can accomodate the copyright string.
 */

#define DSP_QUEUE_SIZE  64

EXTERN struct SB_queue_t {
  __u8  output[DSP_QUEUE_SIZE];  /* Output Queue */
  int   holds;                   /* Items in the Queue */
  int   start;                   /* Current Queue Start */
  int   end;                     /* Current Queue End */
} SB_queue;

/*
 * The definitions needed for setting up the device structures.
 */

extern void adlib_io_write  (Bit32u addr, Bit8u value); /* Stray */
extern int sb_dma_handler (int status); /* Stray */

extern void sb_io_write     (Bit32u addr, Bit8u value);
extern void fm_io_write     (Bit32u addr, Bit8u value);
extern void mpu401_io_write (Bit32u addr, Bit8u value);
extern Bit8u sb_io_read     (Bit32u addr);
extern Bit8u fm_io_read     (Bit32u addr);
extern Bit8u mpu401_io_read (Bit32u addr);
extern void sound_init      (void);
extern void sound_reset     (void);

extern void SB_driver_reset (void);
extern void FM_driver_reset (void);
extern void MPU_driver_reset (void);

extern int SB_driver_init (void);
extern int FM_driver_init (void);
extern int MPU_driver_init (void);
