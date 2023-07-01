/* rip-off for dosemu2 project by stsp */
/*
 *  Copyright (C) 2002-2012  The DOSBox Team
 *  Copyright (C) 2013-2014  bjt
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * ------------------------------------------
 * SoftMPU by bjt - Software MPU-401 Emulator
 * ------------------------------------------
 *
 * Based on original pic.c from DOSBox
 *
 */
#include <stddef.h>
#include "sig.h"
#include "evtimer.h"
/* SOFTMPU: Moved exported functions & types to header */
#include "export.h"

static void *evtimer;

static void PIC_Update(int ms_ticks, void *arg);

static void _MPU401_Event(void *arg)
{
        MPU401_Event();
}

static void _MPU401_ResetDone(void *arg)
{
        MPU401_ResetDone();
}

static void _MPU401_EOIHandler(void *arg)
{
        MPU401_EOIHandler();
}

/* SOFTMPU: Event countdown timers */
static Bitu event_countdown[NUM_EVENTS];
static Bitu MIDI_sysex_delay; /* SOFTMPU: Initialised in midi.c */

void PIC_AddEvent(EventID event, Bitu delay)
{
        /* Dispatch event immediately on zero delay */
        /* Watch out for blocking loops here... */
        if (delay==0)
        {
                switch (event)
                {
                case MPU_EVENT:
                        /* Don't dispatch immediately as we'll enter an
                        infinite loop if tempo is high enough */
                        delay=1; /* Enforce minimum delay */
                        break;
                case RESET_DONE:
                        MPU401_ResetDone();
                        break;
                case EOI_HANDLER:
                        MPU401_EOIHandler();
                        break;
                default:
                        break;
                }
        }

        /* SOFTMPU: Set the countdown timer */
        event_countdown[event]=delay;
}

void PIC_RemoveEvents(EventID event)
{
        /* SOFTMPU: Zero the countdown timer (disable event) */
        event_countdown[event]=0;
}

void PIC_Init(void)
{
        Bitu i;

        /* SOFTMPU: Zero countdown timers */
        for (i=0;i<NUM_EVENTS;i++)
        {
                PIC_RemoveEvents(i);
        }

        evtimer = evtimer_create(PIC_Update, NULL);
        evtimer_set_rel(evtimer, SCALE_MS, 1);
}

void PIC_Done(void)
{
        evtimer_delete(evtimer);
}

static void PIC_Update(int ms_ticks, void *arg)
{
        Bitu i;

        /* SOFTMPU: Decrement sysex delay used in midi.c */
        if (MIDI_sysex_delay >= ms_ticks)
        {
                MIDI_sysex_delay -= ms_ticks;
        }
        else
                MIDI_sysex_delay = 0;

        /* SOFTMPU: Decrement countdown timers and dispatch as needed */
        for (i=0;i<NUM_EVENTS;i++)
        {
                if (event_countdown[i] > 0) {
                        if (event_countdown[i] >= ms_ticks)
                            event_countdown[i] -= ms_ticks;
                        else
                            event_countdown[i] = 0;

                        if (event_countdown[i]==0)
                        {
                                /* Dispatch */
                                switch (i)
                                {
                                        case MPU_EVENT:
                                                add_thread_callback(_MPU401_Event, NULL, "mpu401 event");
                                                break;
                                        case RESET_DONE:
                                                add_thread_callback(_MPU401_ResetDone, NULL, "mpu401 reset done");
                                                break;
                                        case EOI_HANDLER:
                                                add_thread_callback(_MPU401_EOIHandler, NULL, "mpu401 EOI");
                                                break;
                                        default:
                                                break;
                                }
                        }
                }
        }
}
