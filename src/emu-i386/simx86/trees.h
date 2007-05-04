/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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

/////////////////////////////////////////////////////////////////////////////
//
// Tree node key definition.
//

struct avltr_node;

typedef struct _bkref {
	struct _bkref *next;
	struct avltr_node **ref;
	char branch;
} backref;

typedef struct _lnkdesc {
	unsigned char t_type;
	unsigned short nrefs;
	union {
		unsigned int *abs;
		unsigned int rel;
	} t_link;
	union {
		unsigned int *abs;
		unsigned int rel;
	} nt_link;
	unsigned int t_undo, nt_undo;
	struct avltr_node **t_ref, **nt_ref;
	backref bkr;
} linkdesc;

typedef struct _imgen {
	int op, mode, ovds;
	int p0,p1,p2,p3,p4;
	linkdesc *lt;
} IGen;

typedef struct _ianpc {
	unsigned short daddr, dnpc __attribute__ ((packed));
} Addr2Pc;

typedef struct _imeta {
	unsigned char *addr, *npc, *seqbase, *jtgt;
	unsigned short ncount, len, flags, seqlen, totlen;
	linkdesc clink;
	int ngen;
	IGen gen[NUMGENS];
} IMeta;

typedef struct _codebufhdr {
	struct avltr_node *bkptr;
	void *selfptr;
	Addr2Pc meta[0]; /* there are nap of these */
	/* behind these follows the code */
} CodeBuf;

extern IMeta InstrMeta[];
extern int   CurrIMeta;
extern int NodesExecd;
extern int TotalNodesExecd;
extern int NodesParsed;
extern int TotalNodesParsed;
extern int MaxNodes;
extern int MaxNodeSize;
extern int MaxDepth;
extern int NodesNotFound;
extern int NodesFastFound;
extern int PageFaults;
extern int EmuSignals;
extern int NodesFound;
extern int TreeCleanups;

extern CodeBuf *GenCodeBuf;
extern int GenBufSize;
extern unsigned char *CodePtr;

extern unsigned char TailCode[];

typedef struct avltr_node
{
/* ----- Structure for a node in a right-threaded AVL tree. ----- */
    struct avltr_node *link[2];	/* Subtrees or threads. */
    signed char bal;		/* Balance factor. */
    char cache;			/* Used during insertion. */
    char pad;			/* Reserved for fully threaded trees. */
    signed char rtag;		/* Right thread tag. */
/* -------------------------------------------------------------- */
	int key;		/* signed! and don't move it from here! */
/* -------------------------------------------------------------- */
	int alive;
	CodeBuf *mblock;
	unsigned char *addr;
	Addr2Pc *pmeta;
	unsigned short len, flags, seqlen, seqnum __attribute__ ((packed));
	long nxkey, seqbase;
	struct avltr_node *nxnode;
	linkdesc clink;
} TNode;

extern TNode *LastXNode;

/* Used for traversing a right-threaded AVL tree. */
typedef struct avltr_traverser
{
    int init;				/* Initialized? */
    struct avltr_node *p;		/* Last node returned. */
} avltr_traverser;

/* Structure which holds information about a threaded AVL tree. */
typedef struct avltr_tree
{
    struct avltr_node root;	/* Tree root node. */
    int count;			/* Number of nodes in the tree. */
} avltr_tree;

/* Tag types. */
#define PLUS +1
#define MINUS -1

#ifdef HOST_ARCH_X86
extern avltr_tree CollectTree;

void avltr_delete (const int key);
//
TNode *FindTree(int key);
TNode *Move2Tree(void);
//
#endif

void InitTrees(void);

#ifdef HOST_ARCH_X86
int  FindCodeNode(long addr);
int  InvalidateSingleNode (long addr, long eip);
int  InvalidateNodePage(long addr, int len, long eip, int *codehit);
#endif

#endif
