/*
 *  Copyright (C) 2002-2012  The DOSBox Team
 *  Copyright (C) 2013-2014  bjt, elianda
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
 * C types/functions used from ASM
 *
 */

#ifndef EXPORT_H

typedef unsigned char Bit8u;
typedef unsigned int Bitu;

typedef enum EventID {MPU_EVENT,RESET_DONE,EOI_HANDLER,NUM_EVENTS} EventID;

/* Interface functions */
void MPU401_Init(void);
void MPU401_Done(void);
void MPU401_WriteCommand(Bit8u val);
void MPU401_ReadData(Bit8u val);
void MPU401_WriteData(Bit8u val);

void MPU401_Event(void);
void MPU401_ResetDone(void);
void MPU401_EOIHandler(void);

void PIC_Init(void); /* SOFTMPU */
void PIC_Done(void);
void PIC_AddEvent(EventID event,Bitu delay);
void PIC_RemoveEvents(EventID event);

void QueueByte(Bit8u data);
void ClrQueue(void);

#define EXPORT_H
#endif
