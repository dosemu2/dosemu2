/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef EMU_SOUND_H
#define EMU_SOUND_H
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

#include "extern.h"
#include "types.h"

/*
 ***************************************************************************
 * SB Support                                                              *
 ***************************************************************************
 */

/*
 * The different SoundBlaster versions returned by the init function.
 */

#define SB_NONE  0x000
#define SB_OLD	 0x105
#define SB_20	 0x201
#define SB_PRO	 0x300
#define SB_16	 0x404 /* Was 40D, but that is AWE32+ - AM */
#define SB_AWE32 0x40C

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

struct sb_irq_t {
  uint16_t irq8;                  /* B-bit IRQ (internal PIC value) */
  uint16_t irq16;                 /* 16-bit IRQ (internal PIC value) */
  uint16_t midi;                  /* Midi IRQ (internal PIC value) */

  uint8_t  pending;               /* Currently pending IRQs */
  uint8_t  active;                /* Currently active IRQs */
};

EXTERN struct sb_information_t {
  uint8_t  mixer_index;           /* Which Mixer channel to work on */

  struct sb_irq_t irq;         /* IRQ information */

  uint8_t  speaker;               /* Speaker Status */
  uint16_t version;               /* Version of the SB being emulated */
} SB_info;

/* 
 * DSP information / states 
 */

EXTERN struct DSP_information_t {
  uint8_t  time_constant;         /* The current Time constant for writes */
  uint16_t output_sample_rate;    /* The current sample rate for output */
  uint16_t input_sample_rate;     /* The current sample rate for input */
  uint8_t  test;                  /* Storage for the test value */
  uint8_t  stereo;                /* Is the device Stereo */
  uint8_t  is_sb16_command;       /* Was it an SB16-only command? */
  uint8_t  is_16bit;              /* Is the device 16-bit */
  uint8_t  is_signed;             /* Is the device accepting signed samples */
  uint8_t  ready;                 /* Is DSP Ready ? */
  uint8_t  data;                  /* Data is available */
  int32_t  length;                /* Length of the DMA transfer, in bytes or words */
  size_t   units_left;            /* No. of bytes/words left in current blk */
/* 
 * This is the maximum number of bytes/words transferred via DMA. If you turn
 * it too low, the dosemu-overhead gets too big, so the transfer is
 * interrupted (clicking, buzzing), if you make it too high, some
 * programs reading the dma-registers are confused, because the registers
 * jump in too high steps 
 */
#define IRQ_AT_EMPTY 1
#define START_DMA_AT_EMPTY 2
#define DMA_CONTINUE 4		/* NOT for DMA pause/restart !!! */
#define DREQ_AT_EOI 8
  uint8_t  empty_state;	       /* what we have to do when transfer ends */ 
  uint8_t  pause_state;	       /* is DMA transfer paused? */ 
#define SB_USES_DMA 1
#define HIGH_SPEED_DMA 2
#define SB_DMA_INPUT 4
#define SB_DMA_AUTO_INIT 8
  uint8_t  dma_mode;              /* Information we need on the DMA transfer */
  uint8_t  command;               /* DSP command in progress */
#define SB_NO_DSP_COMMAND 0
#define MAX_DSP_PARAMS 3
  uint8_t  parameter[MAX_DSP_PARAMS];  /* value of parameter */
  uint8_t  num_parameters;        /* do we have the parameter */
} SB_dsp;


/*
 * SB Driver - Architecture Specific Interface
 */

EXTERN struct SB_driver_t {

  /*
   * Mixer Functions
   */
  void  (* write_mixer)(int channel, uint8_t value);
  uint8_t  (* read_mixer)(int channel);

  /*
   * Speaker Control
   */
  void  (* speaker_on)(void);
  void  (* speaker_off)(void);

  /*
   * Direct DAC access
   */
  void  (* DAC_write)(int bits, uint8_t value);

  /*
   * DMA functions
   */
  int  (* DMA_start_init)(int read);
  size_t  (* DMA_do_read)(void*, size_t);
  size_t  (* DMA_do_write)(void*, size_t);
  void  (* DMA_pause)(void);
  void  (* DMA_resume)(void);
  void  (* DMA_stop)(void);
  int   (* DMA_complete_test)(void);
  int   (* DMA_can_change_speed)(void);
  void  (* DMA_complete)(void);
  void  (* DMA_set_blocksize)(int, int); /* **CRISK** */
   
  /*
   * Miscellaneous Functions
   */
  int  (* set_speed)(uint16_t speed, uint8_t stereo, uint8_t is_16bit, uint8_t is_signed);
  void  (* play_buffer)(void *buffer, uint16_t length);

} SB_driver; 

#define SB_IRQ_8BIT           1
#define SB_IRQ_16BIT          2
#define SB_IRQ_MIDI           4

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

EXTERN struct adlib_timer_t {
  Bit8u reg; /* Timer register */
  Bit8u counter;  /* Timer counter */
  Bit8u enabled;  /* Enabled flag */
  Bit8u expired;  /* Expired flag */
} adlib_timers[2];

EXTERN struct adlib_info_t {
  Bit8u reg; /* Register currently in use */
  /*  struct adlib_timer_t timers[2]; */

} adlib_info;

/* 
 * Output Queue information 
 */

/* 
 * How big the output buffer should be. This _MUST_ be a power of 2.
 * The queue is big, so that it can accomodate the copyright string.
 */

#define QUEUE_SIZE  64
#define Q_HOLDS(q) (q.end - q.start)
#define Q_AVAIL(q) (QUEUE_SIZE - q.end)
#define Q_CLEAR(q) (q.start = q.end = 0)
#define Q_PUT(q, v) { if (q.end < QUEUE_SIZE) q.output[q.end++] = v; }
#define Q_GET(q) ({ \
    Bit8u __ret = Q_HOLDS(q) ? q.output[q.start++] : 0xff; \
    if (!Q_HOLDS(q)) Q_CLEAR(q); \
    __ret; \
})
#define Q_ADD(q, buf, len) ({ \
    int __len = len; \
    int __to_copy = min(__len, Q_AVAIL(q)); \
    memcpy(q.output + q.end, buf, __to_copy); \
    q.end += __to_copy; \
})

typedef struct {
  uint8_t  output[QUEUE_SIZE];  /* Output Queue */
  int   start;                   /* Current Queue Start */
  int   end;                     /* Current Queue End */
} queue_t;

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
	queue_t data;
	/* Architecture specific procedures */
	void (*data_write)(uint8_t data); /* process 1 MIDI byte */
	int (*data_read)(uint8_t data[], int max_size);
	void (*register_io_callback)(void (*io_callback)(void));
} mpu401_info;

/*
 ***************************************************************************
 * Misc Code                                                               *
 ***************************************************************************
 */

EXTERN uint8_t sb_is_running; /* Do we need a tick ? */

#define FM_TIMER_RUN   1 /* Indicates FM sub-system in operation */
#define DSP_OUTPUT_RUN 2 /* Indicates DSP sub-system in operation */

extern void sb_controller(void);
extern void run_new_sb(void);

/* This is the correct way to run an SB timer tick */
#define run_sb()  \
if (config.sound == 2) run_new_sb(); else if (sb_is_running) sb_controller()

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

EXTERN queue_t SB_queue;

/*
 * The definitions needed for setting up the device structures.
 */

extern void sound_init      (void);
extern void sound_reset     (void);
extern void sound_done	    (void);

extern void sound_new_init  (void);
extern void sound_new_reset (void);
extern void sound_new_done  (void);

extern void SB_driver_reset (void);
extern void FM_driver_reset (void);
extern void MPU_driver_reset (void);

extern int SB_driver_init (void);
extern int FM_driver_init (void);
extern int MPU_driver_init (void);

#endif		/* EMU_SOUND_H */
