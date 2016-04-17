#ifndef EMU_SOUND_H
#define EMU_SOUND_H

extern void run_sb(void);
extern void run_sound(void);
extern void sound_init(void);
extern void sound_reset(void);
extern void sound_done(void);
extern int get_mpu401_irq_num(void);
extern int mpu401_enable_imode(int on);

#endif		/* EMU_SOUND_H */
