/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2000 Alberto Vignani, FIAT Research Center
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

typedef struct avltr_node
{
/* ----- Structure for a node in a right-threaded AVL tree. ----- */
    struct avltr_node *link[2];	/* Subtrees or threads. */
    signed char bal;		/* Balance factor. */
    char cache;			/* Used during insertion. */
    char pad;			/* Reserved for fully threaded trees. */
    signed char rtag;		/* Right thread tag. */
/* -------------------------------------------------------------- */
	long key;		/* signed! */
	unsigned char *addr, *npc;
	int jcount;
	unsigned short len, flags, cklen;
	struct avltr_node *nxnode;
	long nxkey;
} TNode;

/* Used for traversing a right-threaded AVL tree. */
typedef struct avltr_traverser
{
    int init;				/* Initialized? */
    const struct avltr_node *p;		/* Last node returned. */
} avltr_traverser;

/* Structure which holds information about a threaded AVL tree. */
typedef struct avltr_tree
{
    struct avltr_node root;	/* Tree root node. */
    int count;			/* Number of nodes in the tree. */
} avltr_tree;

#define AVL_MAX_HEIGHT	20

/* Tag types. */
#define PLUS +1
#define MINUS -1

extern avltr_tree CollectTree;

//
TNode *FindTree(unsigned char *addr);
TNode *Move2ITree(void);
//
void InitTrees(void);
void InvalidateTreePaged (unsigned char *addr, int len);

#endif
