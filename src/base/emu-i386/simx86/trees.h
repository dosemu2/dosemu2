/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 * 2. The tree handling routines were adapted from libavl:
 *  libavl - manipulates AVL trees.
 *  Copyright (C) 1998, 1999 Free Software Foundation, Inc.
 *  The author may be contacted at <pfaffben@pilot.msu.edu> on the
 *  Internet, or as Ben Pfaff, 12167 Airport Rd, DeWitt MI 48820, USA
 *  through more mundane means.
 *
 ***************************************************************************/

#ifndef _EMU86_TREES_H
#define _EMU86_TREES_H

#define ZONED 0
#if ZONED
/* large zoned hash table w/o collisions at 1st mb */
#define FH_SIZE (1024*1024*4)
#define ZONE_P2SZ 20  /* 2^20=1M, can also use 19 for 512K */
#define FH_STATS 0
#define FH_ZONED 1
#else
/* small linear layout */
#define FH_SIZE (1024*128)
#define ZONE_P2SZ 17  /* 2^17=128K, no zoning */

#define FH_STATS 0  /* not expensive due to small amount of buckets */
#define FH_ZONED 0
#endif
#include "misc/fhmap.h"  // FH_STATS must be defined before

/////////////////////////////////////////////////////////////////////////////
//
// Tree node key definition.
//

struct TNode;

typedef struct _bkref {
	struct _bkref *next;
	struct TNode *ref;
	char branch;
} backref;

typedef struct _lnkdesc {
	unsigned int link;
	unsigned int target;
	struct TNode *ref;
} linkdesc;

typedef struct _imgen {
	unsigned int op, mode;
	unsigned int p0;
	/* not a union to alias: use link if and only if mode&MLINK */
	union {
		struct {
			unsigned int p1,p2,p3,p4;
		};
		unsigned char *link;
	};
} IGen;

typedef struct _ianpc {
	unsigned int daddr;
	signed short dnpc;
} Addr2Pc;

typedef struct _imeta {
	int npc;
	unsigned short flags;
	int ngen;
	IGen gen[NUMGENS];
} IMeta;

extern IMeta InstrMeta[MAXINODES];
extern int NodesExecd;
extern int TotalNodesExecd;
extern int PrejitNodesExecd;
extern int NodesPrejitted;
extern int NodesParsed;
extern int TotalNodesParsed;
extern int MaxNodes;
extern int MaxNodeSize;
extern int MaxDepth;
extern int NodesNotFound;
extern int NodesFastFound;
extern int EmuSignals;
extern int NodesFound;
extern int TreeCleanups;

typedef struct TNode
{
/* -------------------------------------------------------------- */
	int key;		/* signed! and don't move it from here! */
/* -------------------------------------------------------------- */
	struct fh_node fhnode;
	int alive;
	unsigned char *addr;
	unsigned short len, flags, seqlen, seqnum;
	linkdesc clink_t;
	linkdesc clink_nt;
	backref *bkr;
	unsigned cs;
	unsigned mode;
	Addr2Pc meta[]; /* there are seqnum+1 of these */
} TNode;

TNode *FindTree(int key);
void Move2Tree(TNode *G);

void InitTrees(void);

unsigned int FindPC(const unsigned char *addr);
static inline unsigned int FindPC_X(const unsigned char *addr)
{
    return FindPC(GetGenCodeBuf(addr));
}
void InvalidateNodeRange(int addr, int len);
void InvalidateNodeRangeFromFault(int addr, int len, unsigned char *eip);
void RemoveNode(TNode *G);
void NodeLinker(TNode *LG, TNode *G);
extern unsigned char *BrokenCodePtr;

#ifdef DEBUG_TREE
extern FILE *tLog;
#endif

#endif
