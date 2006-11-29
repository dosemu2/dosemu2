#ifndef YMF262_H
#define YMF262_H


#define BUILD_YMF262 (HAS_YMF262)


/* select number of output bits: 8 or 16 */
#define OPL3_SAMPLE_BITS 16

/* compiler dependence */
#ifndef OSD_CPU_H
#define OSD_CPU_H
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */
#endif
/*
typedef INT32 stream_sample_t;
typedef stream_sample_t OPL3SAMPLE;
*/
#if (OPL3_SAMPLE_BITS==16)
typedef INT16 OPL3SAMPLE;
#endif
#if (OPL3_SAMPLE_BITS==8)
typedef INT8 OPL3SAMPLE;
#endif


typedef void (*OPL3_TIMERHANDLER)(void *param,int timer,double interval_Sec);
typedef void (*OPL3_IRQHANDLER)(void *param,int irq);
typedef void (*OPL3_UPDATEHANDLER)(void *param,int min_interval_us);



#if BUILD_YMF262

void *YMF262Init(int clock, int rate);
void YMF262Shutdown(void *chip);
void YMF262ResetChip(void *chip);
int  YMF262Write(void *chip, int a, int v);
unsigned char YMF262Read(void *chip, int a);
int  YMF262TimerOver(void *chip, int c);
void YMF262UpdateOne(void *chip, OPL3SAMPLE **buffers, int length);

void YMF262SetTimerHandler(void *chip, OPL3_TIMERHANDLER TimerHandler, void *param);
void YMF262SetIRQHandler(void *chip, OPL3_IRQHANDLER IRQHandler, void *param);
void YMF262SetUpdateHandler(void *chip, OPL3_UPDATEHANDLER UpdateHandler, void *param);

#endif


#endif /* YMF262_H */
