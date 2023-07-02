/* rip-off for dosemu2 project by stsp */
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
 * Based on original mpu401.c from DOSBox
 *
 */
#include "sound/midi.h"
#include "pic.h"
#include "emu.h"
/* SOFTMPU: Moved exported functions & types to header */
#include "export.h"

/* SOFTMPU: Additional defines, typedefs etc. for C */
typedef unsigned short Bit16u;
typedef int Bits;

static void MIDI_RawOutByte(Bit8u data)
{
        midi_write(data, ST_MT32);
}

void MPU401_Event(void);
void MPU401_ResetDone(void); /* SOFTMPU */
void MPU401_EOIHandler(void);
static void MPU401_Reset(void);
static void MPU401_EOIHandlerDispatch(void);

#define MPU401_VERSION  0x15
#define MPU401_REVISION 0x01
#define MPU401_TIMECONSTANT (60000000 / 1000.0)
#define MPU401_RESETBUSY 14.0

enum MpuMode { M_UART,M_INTELLIGENT };
typedef enum MpuMode MpuMode; /* SOFTMPU */
enum MpuDataType {T_OVERFLOW,T_MARK,T_MIDI_SYS,T_MIDI_NORM,T_COMMAND};
typedef enum MpuDataType MpuDataType; /* SOFTMPU */

/* Messages sent to MPU-401 from host */
#define MSG_EOX                         0xf7
#define MSG_OVERFLOW                    0xf8
#define MSG_MARK                        0xfc

/* Messages sent to host from MPU-401 */
#define MSG_MPU_OVERFLOW                0xf8
#define MSG_MPU_COMMAND_REQ             0xf9
#define MSG_MPU_END                     0xfc
#define MSG_MPU_CLOCK                   0xfd
#define MSG_MPU_ACK                     0xfe

static struct {
	bool intelligent;
        bool generate_irqs; /* SOFTMPU */
        bool mpu_ver_fix; /* SOFTMPU */
        MpuMode mode;
	/*Bitu irq;*/ /* SOFTMPU */
	struct track {
		Bits counter;
		Bit8u value[8],sys_val;
		Bit8u vlength,length;
		MpuDataType type;
	} playbuf[8],condbuf;
	struct {
		bool conductor,cond_req,cond_set, block_ack;
		bool playing,reset;
		bool wsd,wsm,wsd_start;
		bool run_irq,irq_pending;
		bool send_now;
		bool eoi_scheduled;
		Bits data_onoff;
		Bitu command_byte,cmd_pending;
		Bit8u tmask,cmask,amask;
		Bit16u midi_mask;
		Bit16u req_mask;
		Bit8u channel,old_chan;
	} state;
	struct {
		Bit8u timebase;
		Bit8u tempo;
		Bit8u tempo_rel;
		Bit8u tempo_grad;
		Bit8u cth_rate,cth_counter;
		bool clock_to_host,cth_active;
	} clock;
} mpu;

void MPU401_WriteCommand(Bit8u val) /* SOFTMPU */
{
	Bitu i; /* SOFTMPU */
	if (mpu.state.reset) {
		if (mpu.state.cmd_pending || (val!=0x3f && val!=0xff)) {
			mpu.state.cmd_pending=val+1;
			return;
		}
		PIC_RemoveEvents(RESET_DONE);
		mpu.state.reset=false;
	}
	if (val<=0x2f) {
		switch (val&3) { /* MIDI stop, start, continue */
			case 1: {MIDI_RawOutByte(0xfc);break;}
			case 2: {MIDI_RawOutByte(0xfa);break;}
			case 3: {MIDI_RawOutByte(0xfb);break;}
		}
		/*if (val&0x20) LOG(LOG_MISC,LOG_ERROR)("MPU-401:Unhandled Recording Command %x",val);*/ /* SOFTMPU */
		switch (val&0xc) {
			case  0x4:      /* Stop */
				PIC_RemoveEvents(MPU_EVENT);
				PIC_Stop();
				mpu.state.playing=false;
				for (i=0xb0;i<0xbf;i++) {  /* All notes off */
					MIDI_RawOutByte((Bit8u)i);
					MIDI_RawOutByte(0x7b);
					MIDI_RawOutByte(0);
				}
				break;
			case 0x8:       /* Play */
				/*LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Intelligent mode playback started");*/ /* SOFTMPU */
				mpu.state.playing=true;
				PIC_RemoveEvents(MPU_EVENT);
                                PIC_AddEvent(MPU_EVENT,(Bitu)MPU401_TIMECONSTANT/(mpu.clock.tempo*mpu.clock.timebase));
                                PIC_Start();
				ClrQueue();
				break;
		}
	}
	else if (val>=0xa0 && val<=0xa7) {      /* Request play counter */
		if (mpu.state.cmask&(1<<(val&7))) QueueByte((Bit8u)mpu.playbuf[val&7].counter);
	}
	else if (val>=0xd0 && val<=0xd7) {      /* Send data */
		mpu.state.old_chan=mpu.state.channel;
		mpu.state.channel=val&7;
		mpu.state.wsd=true;
		mpu.state.wsm=false;
		mpu.state.wsd_start=true;
	}
	else
	switch (val) {
		case 0xdf:      /* Send system message */
			mpu.state.wsd=false;
			mpu.state.wsm=true;
			mpu.state.wsd_start=true;
			break;
		case 0x8e:      /* Conductor */
			mpu.state.cond_set=false;
			break;
		case 0x8f:
			mpu.state.cond_set=true;
			break;
		case 0x94: /* Clock to host */
			mpu.clock.clock_to_host=false;
			break;
		case 0x95:
			mpu.clock.clock_to_host=true;
			break;
		case 0xc2: /* Internal timebase */
                        mpu.clock.timebase=48;
			break;
		case 0xc3:
                        mpu.clock.timebase=72;
			break;
		case 0xc4:
                        mpu.clock.timebase=96;
			break;
		case 0xc5:
                        mpu.clock.timebase=120;
			break;
		case 0xc6:
                        mpu.clock.timebase=144;
			break;
		case 0xc7:
                        mpu.clock.timebase=168;
			break;
		case 0xc8:
                        mpu.clock.timebase=192;
			break;
		/* Commands with data byte */
		case 0xe0: case 0xe1: case 0xe2: case 0xe4: case 0xe6: 
		case 0xe7: case 0xec: case 0xed: case 0xee: case 0xef:
			mpu.state.command_byte=val;
			break;
		/* Commands 0xa# returning data */
		case 0xab:      /* Request and clear recording counter */
			QueueByte(MSG_MPU_ACK);
			QueueByte(0);
			return;
		case 0xac:      /* Request version */
                        if (mpu.mpu_ver_fix)
                        {
                                /* SOFTMPU: Fix missing music in Gateway by reversing version and ACK */
                                QueueByte(MPU401_VERSION);
                                QueueByte(MSG_MPU_ACK);
                        }
                        else
                        {
                                QueueByte(MSG_MPU_ACK);
                                QueueByte(MPU401_VERSION);
                        }
			return;
		case 0xad:      /* Request revision */
			QueueByte(MSG_MPU_ACK);
			QueueByte(MPU401_REVISION);
			return;
		case 0xaf:      /* Request tempo */
			QueueByte(MSG_MPU_ACK);
			QueueByte(mpu.clock.tempo);
			return;
		case 0xb1:      /* Reset relative tempo */
			mpu.clock.tempo_rel=40;
			break;
		case 0xb9:      /* Clear play map */
		case 0xb8:      /* Clear play counters */
			for (i=0xb0;i<0xbf;i++) {  /* All notes off */
				MIDI_RawOutByte((Bit8u)i);
				MIDI_RawOutByte(0x7b);
				MIDI_RawOutByte(0);
			}
			for (i=0;i<8;i++) {
				mpu.playbuf[i].counter=0;
				mpu.playbuf[i].type=T_OVERFLOW;
			}
			mpu.condbuf.counter=0;
			mpu.condbuf.type=T_OVERFLOW;
			if (!(mpu.state.conductor=mpu.state.cond_set)) mpu.state.cond_req=0;
			mpu.state.amask=mpu.state.tmask;
			mpu.state.req_mask=0;
			mpu.state.irq_pending=true;
			break;
		case 0xff:      /* Reset MPU-401 */
			/*LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Reset %X",val);*/ /* SOFTMPU */
                        PIC_AddEvent(RESET_DONE,MPU401_RESETBUSY);
			mpu.state.reset=true;
			MPU401_Reset();
			if (mpu.mode==M_UART) return;//do not send ack in UART mode
			break;
		case 0x3f:      /* UART mode */
			/*LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Set UART mode %X",val);*/ /* SOFTMPU */
			mpu.mode=M_UART;
			break;
		default:
			/*LOG(LOG_MISC,LOG_NORMAL)("MPU-401:Unhandled command %X",val);*/
			break;
	}
	QueueByte(MSG_MPU_ACK);
}

void MPU401_ReadData(Bit8u data) /* SOFTMPU */
{
	Bit8u ret=data;

//        if ((mpu.queue_used == 0) && mpu.generate_irqs) PIC_DeActivateIRQ(); /* SOFTMPU */

	if (ret>=0xf0 && ret<=0xf7) { /* MIDI data request */
		mpu.state.channel=ret&7;
		mpu.state.data_onoff=0;
		mpu.state.cond_req=false;
	}
	if (ret==MSG_MPU_COMMAND_REQ) {
		mpu.state.data_onoff=0;
		mpu.state.cond_req=true;
		if (mpu.condbuf.type!=T_OVERFLOW) {
			mpu.state.block_ack=true;
			MPU401_WriteCommand(mpu.condbuf.value[0]);
			if (mpu.state.command_byte) MPU401_WriteData(mpu.condbuf.value[1]);
		}
	mpu.condbuf.type=T_OVERFLOW;
	}
	if (ret==MSG_MPU_END || ret==MSG_MPU_CLOCK || ret==MSG_MPU_ACK) {
		mpu.state.data_onoff=-1;
		MPU401_EOIHandlerDispatch();
	}
}

void MPU401_WriteData(Bit8u val) { /* SOFTMPU */
	static Bitu length,cnt,posd; /* SOFTMPU */
	if (mpu.mode==M_UART) {MIDI_RawOutByte((Bit8u)val);return;}
	switch (mpu.state.command_byte) {       /* 0xe# command data */
		case 0x00:
			break;
		case 0xe0:      /* Set tempo */
			mpu.state.command_byte=0;
			mpu.clock.tempo=val;
			return;
		case 0xe1:      /* Set relative tempo */
			mpu.state.command_byte=0;
			/*if (val!=0x40) //default value
				LOG(LOG_MISC,LOG_ERROR)("MPU-401:Relative tempo change not implemented");*/ /* SOFTMPU */
			return;
		case 0xe7:      /* Set internal clock to host interval */
			mpu.state.command_byte=0;
			mpu.clock.cth_rate=val>>2;
			return;
		case 0xec:      /* Set active track mask */
			mpu.state.command_byte=0;
			mpu.state.tmask=val;
			return;
		case 0xed: /* Set play counter mask */
			mpu.state.command_byte=0;
			mpu.state.cmask=val;
			return;
		case 0xee: /* Set 1-8 MIDI channel mask */
			mpu.state.command_byte=0;
			mpu.state.midi_mask&=0xff00;
			mpu.state.midi_mask|=val;
			return;
		case 0xef: /* Set 9-16 MIDI channel mask */
			mpu.state.command_byte=0;
			mpu.state.midi_mask&=0x00ff;
			mpu.state.midi_mask|=((Bit16u)val)<<8;
			return;
		//case 0xe2:    /* Set graduation for relative tempo */
		//case 0xe4:    /* Set metronome */
		//case 0xe6:    /* Set metronome measure length */
		default:
			mpu.state.command_byte=0;
			return;
	}
	if (mpu.state.wsd) {    /* Directly send MIDI message */
		if (mpu.state.wsd_start) {
			mpu.state.wsd_start=0;
			cnt=0;
				switch (val&0xf0) {
					case 0xc0:case 0xd0:
						mpu.playbuf[mpu.state.channel].value[0]=val;
						length=2;
						break;
					case 0x80:case 0x90:case 0xa0:case 0xb0:case 0xe0:
						mpu.playbuf[mpu.state.channel].value[0]=val;
						length=3;
						break;
					case 0xf0:
						/*LOG(LOG_MISC,LOG_ERROR)("MPU-401:Illegal WSD byte");*/ /* SOFTMPU */
						mpu.state.wsd=0;
						mpu.state.channel=mpu.state.old_chan;
						return;
					default: /* MIDI with running status */
						cnt++;
						MIDI_RawOutByte(mpu.playbuf[mpu.state.channel].value[0]);
				}
		}
		if (cnt<length) {MIDI_RawOutByte((Bit8u)val);cnt++;}
		if (cnt==length) {
			mpu.state.wsd=0;
			mpu.state.channel=mpu.state.old_chan;
		}
		return;
	}
	if (mpu.state.wsm) {    /* Directly send system message */
		if (val==MSG_EOX) {MIDI_RawOutByte(MSG_EOX);mpu.state.wsm=0;return;}
		if (mpu.state.wsd_start) {
			mpu.state.wsd_start=0;
			cnt=0;
			switch (val) {
				case 0xf2:{ length=3; break;}
				case 0xf3:{ length=2; break;}
				case 0xf6:{ length=1; break;}
				case 0xf0:{ length=0; break;}
				default:
					length=0;
			}
		}
		if (!length || cnt<length) {MIDI_RawOutByte((Bit8u)val);cnt++;}
		if (cnt==length) mpu.state.wsm=0;
		return;
	}
	if (mpu.state.cond_req) { /* Command */
		switch (mpu.state.data_onoff) {
			case -1:
				return;
			case  0: /* Timing byte */
				mpu.condbuf.vlength=0;
				if (val<0xf0) mpu.state.data_onoff++;
				else {
					mpu.state.data_onoff=-1;
					MPU401_EOIHandlerDispatch();
					return;
				}
				if (val==0) mpu.state.send_now=true;
				else mpu.state.send_now=false;
				mpu.condbuf.counter=val;
				break;
			case  1: /* Command byte #1 */
				mpu.condbuf.type=T_COMMAND;
				if (val==0xf8 || val==0xf9) mpu.condbuf.type=T_OVERFLOW;
				mpu.condbuf.value[mpu.condbuf.vlength]=val;
				mpu.condbuf.vlength++;
				if ((val&0xf0)!=0xe0) MPU401_EOIHandlerDispatch();
				else mpu.state.data_onoff++;
				break;
			case  2:/* Command byte #2 */
				mpu.condbuf.value[mpu.condbuf.vlength]=val;
				mpu.condbuf.vlength++;
				MPU401_EOIHandlerDispatch();
				break;
		}
		return;
	}
	switch (mpu.state.data_onoff) { /* Data */
		case   -1:
			return;
		case    0: /* Timing byte */
			if (val<0xf0) mpu.state.data_onoff=1;
			else {
				mpu.state.data_onoff=-1;
				MPU401_EOIHandlerDispatch();
				return;
			}
			if (val==0) mpu.state.send_now=true;
			else mpu.state.send_now=false;
			mpu.playbuf[mpu.state.channel].counter=val;
			break;
		case    1: /* MIDI */
			mpu.playbuf[mpu.state.channel].vlength++;
			posd=mpu.playbuf[mpu.state.channel].vlength;
			if (posd==1) {
				switch (val&0xf0) {
					case 0xf0: /* System message or mark */
						if (val>0xf7) {
							mpu.playbuf[mpu.state.channel].type=T_MARK;
							mpu.playbuf[mpu.state.channel].sys_val=val;
							length=1;
						} else {
							/*LOG(LOG_MISC,LOG_ERROR)("MPU-401:Illegal message");*/ /* SOFTMPU */
							mpu.playbuf[mpu.state.channel].type=T_MIDI_SYS;
							mpu.playbuf[mpu.state.channel].sys_val=val;
							length=1;
						}
						break;
					case 0xc0: case 0xd0: /* MIDI Message */
						mpu.playbuf[mpu.state.channel].type=T_MIDI_NORM;
						length=mpu.playbuf[mpu.state.channel].length=2;
						break;
					case 0x80: case 0x90: case 0xa0:  case 0xb0: case 0xe0: 
						mpu.playbuf[mpu.state.channel].type=T_MIDI_NORM;
						length=mpu.playbuf[mpu.state.channel].length=3;
						break;
					default: /* MIDI data with running status */
						posd++;
						mpu.playbuf[mpu.state.channel].vlength++;
						mpu.playbuf[mpu.state.channel].type=T_MIDI_NORM;
						length=mpu.playbuf[mpu.state.channel].length;
						break;
				}
			}
			if (!(posd==1 && val>=0xf0)) mpu.playbuf[mpu.state.channel].value[posd-1]=val;
			if (posd==length) MPU401_EOIHandlerDispatch();
	}
}

static void MPU401_IntelligentOut(Bit8u chan) {
	Bitu val, i; /* SOFTMPU */
	switch (mpu.playbuf[chan].type) {
		case T_OVERFLOW:
			break;
		case T_MARK:
			val=mpu.playbuf[chan].sys_val;
			if (val==0xfc) {
				MIDI_RawOutByte((Bit8u)val);
				mpu.state.amask&=~(1<<chan);
				mpu.state.req_mask&=~(1<<chan);
			}
			break;
		case T_MIDI_NORM:
			for (i=0;i<mpu.playbuf[chan].vlength;i++)
				MIDI_RawOutByte(mpu.playbuf[chan].value[i]);
			break;
		default:
			break;
	}
}

static void UpdateTrack(Bit8u chan) {
	MPU401_IntelligentOut(chan);
	if (mpu.state.amask&(1<<chan)) {
		mpu.playbuf[chan].vlength=0;
		mpu.playbuf[chan].type=T_OVERFLOW;
		mpu.playbuf[chan].counter=0xf0;
		mpu.state.req_mask|=(1<<chan);
	} else {
		if (mpu.state.amask==0 && !mpu.state.conductor) mpu.state.req_mask|=(1<<12);
	}
}

static void UpdateConductor(void) {
	if (mpu.condbuf.value[0]==0xfc) {
		mpu.condbuf.value[0]=0;
		mpu.state.conductor=false;
		mpu.state.req_mask&=~(1<<9);
		if (mpu.state.amask==0) mpu.state.req_mask|=(1<<12);
		return;
	}
	mpu.condbuf.vlength=0;
	mpu.condbuf.counter=0xf0;
	mpu.state.req_mask|=(1<<9);
}

void MPU401_Event(void) {
	/* SOFTMPU */
	Bitu i;
	Bitu new_time;
	if (mpu.mode==M_UART) return;
	if (mpu.state.irq_pending) goto next_event;
	for (i=0;i<8;i++) { /* Decrease counters */
		if (mpu.state.amask&(1<<i)) {
			mpu.playbuf[i].counter--;
			if (mpu.playbuf[i].counter<=0) UpdateTrack((Bit8u)i);
		}
	}
	if (mpu.state.conductor) {
		mpu.condbuf.counter--;
		if (mpu.condbuf.counter<=0) UpdateConductor();
	}
	if (mpu.clock.clock_to_host) {
		mpu.clock.cth_counter++;
		if (mpu.clock.cth_counter >= mpu.clock.cth_rate) {
			mpu.clock.cth_counter=0;
			mpu.state.req_mask|=(1<<13);
		}
	}
	if (!mpu.state.irq_pending && mpu.state.req_mask) MPU401_EOIHandler();
next_event:
	PIC_RemoveEvents(MPU_EVENT);
	if ((new_time=mpu.clock.tempo*mpu.clock.timebase)==0) return;
        PIC_AddEvent(MPU_EVENT,(Bitu)MPU401_TIMECONSTANT/new_time);
}

static void MPU401_EOIHandlerDispatch(void) {
	if (mpu.state.send_now) {
		mpu.state.eoi_scheduled=true;
                PIC_AddEvent(EOI_HANDLER,1); // Possible a bit longer
	}
	else if (!mpu.state.eoi_scheduled) MPU401_EOIHandler();
}

//Updates counters and requests new data on "End of Input"
void MPU401_EOIHandler(void) {
	Bitu i=0; /* SOFTMPU */
	mpu.state.eoi_scheduled=false;
	if (mpu.state.send_now) {
		mpu.state.send_now=false;
		if (mpu.state.cond_req) UpdateConductor();
		else UpdateTrack(mpu.state.channel);
	}
	mpu.state.irq_pending=false;
	if (!mpu.state.playing || !mpu.state.req_mask) return;
	do {
		if (mpu.state.req_mask&(1<<i)) {
			QueueByte((Bit8u)(0xf0+i));
			mpu.state.req_mask&=~(1<<i);
			break;
		}
	} while ((i++)<16);
}

void MPU401_ResetDone(void) { /* SOFTMPU */
	mpu.state.reset=false;
        if (mpu.state.cmd_pending) {
		MPU401_WriteCommand(mpu.state.cmd_pending-1);
		mpu.state.cmd_pending=0;
	}
}

static void MPU401_Reset(void) {
	Bitu i; /* SOFTMPU */

	mpu.mode=(mpu.intelligent ? M_INTELLIGENT : M_UART);
	PIC_RemoveEvents(EOI_HANDLER);
	mpu.state.eoi_scheduled=false;
	mpu.state.wsd=false;
	mpu.state.wsm=false;
	mpu.state.conductor=false;
	mpu.state.cond_req=false;
	mpu.state.cond_set=false;
	mpu.state.playing=false;
	mpu.state.run_irq=false;
	mpu.state.irq_pending=false;
	mpu.state.cmask=0xff;
	mpu.state.amask=mpu.state.tmask=0;
	mpu.state.midi_mask=0xffff;
	mpu.state.data_onoff=0;
	mpu.state.command_byte=0;
	mpu.state.block_ack=false;
	mpu.clock.tempo=100;
	mpu.clock.timebase=120;
	mpu.clock.tempo_rel=40;
	mpu.clock.tempo_grad=0;
	mpu.clock.clock_to_host=false;
	mpu.clock.cth_rate=60;
	mpu.clock.cth_counter=0;
	ClrQueue();
	mpu.state.req_mask=0;
	mpu.condbuf.counter=0;
	mpu.condbuf.type=T_OVERFLOW;
	for (i=0;i<8;i++) {mpu.playbuf[i].type=T_OVERFLOW;mpu.playbuf[i].counter=0;}
}

/* SOFTMPU: Initialisation */
void MPU401_Init(void)
{
	/* Initalise PIC code */
	PIC_Init();

	mpu.mode=M_UART;
        mpu.generate_irqs=false; /* SOFTMPU */
        mpu.mpu_ver_fix=false; /* SOFTMPU */

        mpu.intelligent=true; /* Default is on */
        mpu.generate_irqs=true;
        mpu.mpu_ver_fix=true;

        /* SOFTMPU: Moved IRQ 9 handler init to asm */
	MPU401_Reset();
}

void MPU401_Done(void)
{
        PIC_Done();
}
