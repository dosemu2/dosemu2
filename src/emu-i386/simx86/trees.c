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
 ***************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "emu86.h"
#include "codegen.h"

#undef  DEBUG_TREE

IMeta	InstrMeta[MAXGNODES];
int	NextFreeIMeta = 1;
IMeta	*ForwIRef = NULL;
IMeta	*LastIMeta = NULL;

/* Tree structure to store collected code sequences. This is a dynamic tree
 * and stores sequences of instructions copied from the G-Tree at every
 * cleanup point */
TNode	*CollectTree = NULL;
int ninodes = 0;

/* Definitions for periodical cleanup of the collecting tree.
 * Code sequences should decay exponentially, otherwise short loops
 * which are repeated many times could persist forever in the cache */
#define AGENODE		240
#define UNAGE		12

#define MIN_FRAGLEN	2

/////////////////////////////////////////////////////////////////////////////

extern unsigned char byrev[];

/* Key generation for binary tree. As the addresses come in sequence,
 * they cannot be directly used as keys; instead, the lower 8 bits are
 * reversed, then the whole longword is byte-reversed. So we have:
 *		0...7|15...8|23...16|31..24
 * which should be random enough for our purposes to keep the tree
 * quite balanced.
 */
static inline unsigned long makekey(unsigned long a)
{
    *((unsigned	char *)&a) = byrev[(a&0xff)];
    __asm__("bswap %0" : "=r" (a) : "0" (a));
    return a ^ 0x80;
}

static inline unsigned long invkey(unsigned long k)
{
    __asm__("bswap %0" : "=r" (k) : "0" (k));
    *((unsigned	char *)&k) = byrev[(k&0xff)];
    return k;
}

/////////////////////////////////////////////////////////////////////////////

void GCPrint(unsigned char *cp, int len)
{
	int i;
	while ((d.emu>4) && len) {
		e_printf(">>> %08lx:",(long)cp);
		for (i=0; (i<16) && len; i++,len--) e_printf(" %02x",*cp++);
		e_printf("\n");
	}
}

#ifdef DEBUG_TREE
static void printtree(TNode *I, int spc)
{
	if (I->left)
		printtree(I->left, spc+1);
	//
	if (d.emu>3) {
		for (int i=0; i<spc; i++) e_printf("|       ");
		e_printf("%08lx%c\n",(long)I,(I->jcount? ' ':'*'));
	}
	//
	if (I->right)
		printtree(I->right, spc+1);
}

static void TraversePN(void)
{
	TNode *G;
	int n;
	G = CollectTree;
	n = 0;
	while (G->next) {
		n++; G = G->next;
		e_printf("---- n=%08lx a=%lx\n",(long)G,(long)G->npc);
	}
	dbug_printf("Traverse next = %d nodes\n",n);
	n = 0;
	while (G->prev) {
		n++; G = G->prev;
		e_printf("---- n=%08lx a=%lx\n",(long)G,(long)G->npc);
	}
	dbug_printf("Traverse prev = %d nodes\n",n);
}
#endif

/////////////////////////////////////////////////////////////////////////////

static long long LastKey = 0L;
static TNode *LastTNode = NULL;
static TNode *CurrTNode = NULL;

/*
 * This code deletes a node from the collector tree. It is the best I found
 * on my books.
 */
TNode *DelITree(TNode *t)
{
	TNode *x;
	TNode *p;
	hitimer_t t0;
	long long key;

	if (t==CollectTree) return t;
	x = t;
	p = t->root;
	t0 = GETTSC();
	key = t->key.lk;
	if (d.emu>3) e_printf("Remove ITree-node %08lx L->%08lx R->%08lx P->%08lx\n",
		(long)t,(long)t->left,(long)t->right,(long)t->root);

	if (t->right == NULL) {
		x = t->left;
	}
	else {
		if (t->right->left == NULL) {
			x = t->right;
			x->left = t->left;
			if (x->left) x->left->root = x;
		}
		else {		/* uh,oh... looks heavy */
			TNode *c = t->right;
			while (c->left->left != NULL) c = c->left;
			x = c->left;
			c->left = x->right;  if (c->left) c->left->root = c;
			x->left = t->left;   if (x->left) x->left->root = x;
			x->right = t->right; if (x->right) x->right->root = x;
		}
	}
	if (t->addr) free(t->addr);
	if (t->prev) t->prev->next = t->next;
	if (t->next) t->next->prev = t->prev;
	if (t==LastTNode) { LastKey = 0L; LastTNode = NULL; }
	if (t==CurrTNode) { CurrTNode = CurrTNode->prev; }
	memset(t, 0, sizeof(TNode));
	free(t);
	ninodes--;
	if (key < p->key.lk) p->left = x; else p->right = x;
	if (x) x->root = p;
	CleanupTime += (GETTSC() - t0);
	return x;
}

/////////////////////////////////////////////////////////////////////////////


static void TraverseAndClean (void)
{
	static int first=1;
	static TNode *G;
	if (first) {
		G = CollectTree->next; first = 0;
	}
	if (G) {
		G->jcount -= UNAGE;
		if (d.emu>4)
			e_printf("TraverseAndClean: node %08lx of %d count=%d\n",
				(long)G, ninodes, G->jcount);
		if (G->jcount<=0)
			G = DelITree(G);
		if (G) G = G->next; else first = 1;
	}
	else first = 1;
}


/////////////////////////////////////////////////////////////////////////////
/*
 * Add a node to the collector tree. This means copying the original GenTree
 * node (with the references to the code buffer), allocating another buffer
 * large enough to store the code sequence and add the resulting thing to the ITree.
 */
TNode *Move2ITree(void)
{
	IMeta *G0 = &InstrMeta[0];		// root of code buffer
	TNode *I = CollectTree;
	TNode *nI;
	hitimer_t t0 = GETTSC();
	GKey key;
	int len;

	key.sk.a = makekey((long)G0->npc);
	key.sk.c = *((long *)G0->npc);

	if (G0->ncount < MIN_FRAGLEN) {
		if (d.emu>3) e_printf("Short sequence not collected\n");
		nI = NULL;
		goto noinode;
	}
	while (I) {
		if (key.lk < I->key.lk) {
			if (I->left) I = I->left;
			else { 
				nI = (TNode *)malloc(sizeof(TNode));
				I->left = nI;
				goto addinode;
			}
		}
		else {
		    if (key.lk > I->key.lk) {
			if (I->right) I	= I->right;
			    else {
				nI = (TNode *)malloc(sizeof(TNode));
				I->right = nI;
addinode:
				if (d.emu>5) e_printf("Head len=%d n_ops=%d PC=%08lx\n",
					G0->len, G0->ncount, (long)G0->npc);
				nI->key.lk = key.lk;
				nI->npc = G0->npc;
				nI->jcount = AGENODE;
				nI->len = len = G0->len + sizeof(TailCode);
				nI->addr = (unsigned char *)malloc(len+8);
				if (d.emu>5) e_printf("Move sequence from %08lx to %08lx l=%d\n",
					(long)G0->addr, (long)nI->addr, len);
				memcpy(nI->addr, G0->addr, len);
				nI->flags = G0->flags;
				nI->left = nI->right = NULL;
				ninodes++;
				if (d.emu>3) e_printf("New Inode %d at=%08lx key=%016Lx root=%08lx\n",
					ninodes,(long)nI,nI->key.lk,(long)I);

				nI->root = I;
				nI->prev = CurrTNode; nI->next = NULL;
				if (CurrTNode) CurrTNode->next = nI;
				CurrTNode = nI;
				goto noinode;
			    }
		    }
		    else {
			if (I->jcount)
				e_printf("AddITree: equal keys %16Lx\n",key.lk);
			nI = NULL;
noinode:
			LastIMeta = NULL;
			NextFreeIMeta = 0;
			memset(&InstrMeta[0],0,sizeof(IMeta));
			ResetCodeBuf();
			AddTime += (GETTSC() - t0);
			return nI;
		    }
		}
	}
	AddTime += (GETTSC() - t0);
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////


TNode *FindTree(unsigned char *addr)
{
	TNode *I;
	GKey key;
	hitimer_t t0;
	int n = 0;

	if (ninodes==0) return NULL;

	t0 = GETTSC();
	key.sk.a = makekey((long)addr);
	key.sk.c = *((long *)addr);
	if (key.lk == LastKey) {
		SearchTime += (GETTSC() - t0);
		if (d.emu>3) e_printf("CollectTree: Found last key\n");
		LastTNode->jcount = AGENODE;
		return LastTNode;
	}
	if (LastTNode) {
		TNode *nxI = LastTNode->next;
		if (nxI && (key.lk == nxI->key.lk)) {
			if (d.emu>3) e_printf("CollectTree: Found next key\n");
			LastTNode = nxI;
			LastKey = nxI->key.lk;
			LastTNode->jcount = AGENODE;
			return LastTNode;
		}
	}

	I = CollectTree;
	while (I) {
		if (key.sk.a == I->key.sk.a) {
			if (key.sk.c != I->key.sk.c) I->jcount=0;
			if (I->jcount) break;
			DelITree(I);
			goto nquit;
		}
		else if (key.sk.a < I->key.sk.a) {
			if (!I->left) goto nquit;
			I = I->left; n++;
		}
		else {
			if (!I->right) goto nquit;
			I = I->right; n++;
		}
	}
	if (I->addr) {
		I->jcount = AGENODE;
		LastKey = key.lk;
		LastTNode = I;
		SearchTime += (GETTSC() - t0);
		if (d.emu>3) e_printf("Found key %16Lx for %08lx hits=%d try=%d\n",
			key.lk,(long)I->npc,I->jcount,n);
		return I;
	}
nquit:
	TraverseAndClean();
	SearchTime += (GETTSC() - t0);
	if (d.emu>6) e_printf("Not found key %16Lx try=%d\n",key.lk,n);
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////

void CleanIMeta(void)
{
	hitimer_t t0 = GETTSC();
	if (d.emu>1)
		e_printf("CodeCache: InstrMeta=%d bytes %d nodes\n",
			(CodePtr-CodeBuf), NextFreeIMeta-1);
	if (d.emu>1)
		e_printf("CodeCache: CollectTree=%d nodes\n",ninodes);
	NextFreeIMeta = 0;
	memset(&InstrMeta[0],0,sizeof(IMeta));
	LastIMeta = NULL;
	ResetCodeBuf();
	CleanupTime += (GETTSC() - t0);
}


static void CleanITree2(TNode *G)
{
	if (G) {
		if (G->left) {
			CleanITree2(G->left);
		}
		if (G->right) {
			CleanITree2(G->right);
		}
		if (G!=CollectTree) {
			if (G->addr) free(G->addr);
			free(G); ninodes--;
		}
	}
}

void CleanITree(TNode *G)
{
	if (d.emu>1) e_printf("Cleaning CollectTree\n");
	CleanITree2(G);
}


/////////////////////////////////////////////////////////////////////////////


IMeta *NewIMeta(unsigned char *npc, int	mode, int *rc, void *aux)
{
	hitimer_t t0 = GETTSC();

	if (CodePtr != PrevCodePtr) {		// new code was	created?
		unsigned char *cp;
		// add new opcode metadata to the G-List
		IMeta *G  = &InstrMeta[NextFreeIMeta++];
		IMeta *G0 = &InstrMeta[0];
		if (NextFreeIMeta>=MAXGNODES) { *rc = -1; goto quit; }
		G0->ncount += 1;
		G->npc = npc;

		cp = PrevCodePtr;
		G->addr	= cp;			// code addr in buffer
		G->len = CodePtr - cp;		// instruction code length
		G->flags = mode>>16;		// FP and flags affected

		if (LastIMeta) {
			G0->len += G->len;
			G0->flags |= (G->flags&15);
		}
		if (PrevCodePtr == CodeBuf) {		// no open code sequences
			if (d.emu>2) e_printf("============ Opening sequence at %08lx\n",(long)npc);
		}
		PrevCodePtr = CodePtr;
		LastIMeta = G;
		if (d.emu>3) {
			e_printf("Metadata %03d a=%08lx PC=%08lx mode=%x(%x) l=%d\n",
				NextFreeIMeta,(long)G->addr,(long)G->npc,G->flags,
				G0->flags,G->len);
			GCPrint(cp, G->len);
		}
#ifdef OPTIMIZE_FW_JUMPS
		if (G->flags&F_FJMP) {
			G->fwref = ForwIRef;
			G->jtgt = aux;
			ForwIRef = G;
		}
#endif
		*rc = 1;
		AddTime += (GETTSC() - t0);
		return G;
	}
	*rc = 0;
quit:
	AddTime += (GETTSC() - t0);
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////

void InitTrees(void)
{
	CollectTree = (TNode *)malloc(sizeof(TNode));		// root
	memset(&InstrMeta[0], 0, sizeof(IMeta));
	NextFreeIMeta = 0;
	ForwIRef = NULL;
	memset(CollectTree, 0, sizeof(TNode));
	if (d.emu>1) e_printf("Root tree node at %08lx\n",(long)CollectTree);
	LastIMeta = NULL;
	LastKey = 0L;
	LastTNode = NULL;
	CurrTNode = CollectTree;
}

void EndGen(void)
{
	CleanIMeta();
	CleanITree(CollectTree);
	if (CollectTree) free(CollectTree);
}

/////////////////////////////////////////////////////////////////////////////

