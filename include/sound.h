/* These defaults are the defaults for an SB Pro as far as I can tell, and
   they are what I have on my system... (Yes, I guess this means I can escape
   adding config file support! */
static unsigned char sound_dma_ch = 1;
static unsigned char sound_irq = PIC_IRQ5;
static unsigned int  sound_base = 0x220;

#ifdef __linux__
/* This param controls fragments.  Try the following values if dsp is
perfocming poorly:
0x00020008  The value Hannu notes in his explaination
0x00FF0008  Try more fragments
See /usr/src/linux/drivers/sound/experimental.txt for full details */
#define SOUND_FRAG 0x00020008;

/* These are the files.  Someone should probably change these to variables
when we make configuration possible... */
#define MIXER_PATH "/dev/mixer"
#define DSP_PATH "/dev/dsp"
#endif

#ifdef __NetBSD__
#define MIXER_PATH "/dev/mixer"
#define DSP_PATH "/dev/sound"
#endif

/************************************************************************
ATTENTION DOSEMU CONFIGURATION CODERS: ALL OF THE ABOVE PARAMS SHOULD BECOME
CONFIGURABLE.  THE #defines ARE USED IN ../sound/sound.c  PLEASE MAKE
EVERYTHING ABOVE THIS CONFIGURABLE BUT KEEP THESE AS DEFAULTS.  DON'T TOUCH
ANYTHING BELOW THIS UNLESS YOU'RE MAKING EXTENSIVE CHANGES TO MY CODE
*************************************************************************/

/* This is anded with the address to see if it's valid */
#define SOUND_IO_MASK 0xFFF0
#define SOUND_IO_MASK2 0x0F

extern void sb_write(unsigned char addr, unsigned char value);
extern void fm_write(unsigned char addr, unsigned char value);
extern unsigned char sb_read(unsigned char addr);
extern void sound_init();

