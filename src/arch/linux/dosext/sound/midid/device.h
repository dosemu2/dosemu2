/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _DEVICE_H
#define _DEVICE_H

#include "midid.h"
#include <stdarg.h>

#define dev_flush() for_each_dev(run_flush)
#define dev_noteon(chn, note, vol) for_each_dev(run_noteon, chn, note, vol)
#define dev_noteoff(chn, note, vol) for_each_dev(run_noteoff, chn, note, vol)
#define dev_notepressure(chn, control, value) \
    for_each_dev(run_notepressure, chn, control, value)
#define dev_channelpressure(chn, vol) \
    for_each_dev(run_channelpressure, chn, vol)
#define dev_control(chn, control, value) \
    for_each_dev(run_control, chn, control, value)
#define dev_program(chn, pgm) for_each_dev(run_program, chn, pgm)
#define dev_bender(chn, pitch) for_each_dev(run_bender, chn, pitch)
#define dev_sysex(buf, len) for_each_dev(run_sysex, buf, len)

/* Linked list of output devices */
typedef struct Device {
	struct Device *next;	/* Next device */
	char *name;
	int version;		/* v1.00 = 100 */
	bool detected;
	bool active;
	bool ready;
	bool (*detect) (void);	/* returns TRUE if detected */
	bool (*init) (void);	/* returns TRUE if init was succesful */
	void (*done) (void);
	void (*pause) (void);
	void (*resume) (void);
	void (*flush) (void);   /* Flush all commands sent */
        bool (*setmode) (Emumode new_mode);
         /* Set (emulation) mode to new_mode; returns TRUE iff possible */
        /* MIDI commands */
	void (*noteon) (int chn, int note, int vel);             /* 0x90 */
	void (*noteoff) (int chn, int note, int vel);            /* 0x80 */
	void (*notepressure) (int chn, int control, int value);  /* 0xA0 */
	void (*channelpressure) (int chn, int vel);              /* 0xD0 */
	void (*control) (int chn, int control, int value);       /* 0xB0 */
	void (*program) (int chn, int pgm);                      /* 0xC0 */
	void (*bender) (int chn, int pitch);                     /* 0xE0 */
	void (*sysex) (unsigned char buf[], int len);            /* 0xF0 */
} Device;

void device_add(void (*register_func) (Device * dev));
void device_register_all(void);
void device_detect_all(void);
int device_init_all(void);
void device_stop_all(void);
void device_pause_all(void);
void device_resume_all(void);
Device *dev_find_first_detected(void);
Device *dev_find_first_active(void);
void device_printall(void);
void device_printactive(void);
Device *device_activate(int number);

bool for_each_dev(bool (*func)(Device *, va_list), ...);

/* The register function are all in their own device file */
#ifdef USE_ULTRA
extern void register_gus(Device * dev);
#endif
extern void register_oss(Device * dev);
extern void register_null(Device * dev);
extern void register_timid(Device * dev);
extern void register_midout(Device * dev);

bool run_flush(Device *dev, va_list args);
bool run_noteon(Device *dev, va_list args);
bool run_noteoff(Device *dev, va_list args);
bool run_notepressure(Device *dev, va_list args);
bool run_channelpressure(Device *dev, va_list args);
bool run_control(Device *dev, va_list args);
bool run_program(Device *dev, va_list args);
bool run_bender(Device *dev, va_list args);
bool run_sysex(Device *dev, va_list args);

#endif
