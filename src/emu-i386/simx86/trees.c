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

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>
#include "emu86.h"
#include "codegen.h"

/* debug dump */
#undef  DEBUG_TREE
#define DT_LEV		2

IMeta	InstrMeta[MAXGNODES];
int	NextFreeIMeta = 1;
IMeta	*ForwIRef = NULL;
IMeta	*LastIMeta = NULL;

/* Tree structure to store collected code sequences */
avltr_tree CollectTree;
avltr_traverser Traverser;
int MaxDepth = 0;
int ninodes = 0;
int MaxNodes = 0;
int NodesCleaned = 0;
long PrevMp = 0;
TNode *NextNode = NULL;
long NextKey = -1;

#define NODES_IN_POOL	12000
TNode *TNodePool;

/*
 * The BIG problem here is: we know exactly when to store compiled code
 * sequences into the tree, but HOW we can know when to delete them?
 *
 * Current solution: periodical cleanup.
 * There are two ways to traverse the collecting tree: one uses the
 * left/right pointers, the other uses prev/next.
 * Left/right follow the binary tree order based on the key, while
 * prev/next follow the historical insertion order.
 * A cleanup function (ideally a separate thread) traverses the tree
 * in a circular way along the prev/next chain and looks at the "age"
 * of the nodes (stored in jcount). As soon as the "node age" decays
 * to 0, the node is deleted; if needed, it will be parsed again from
 * scratch. Any time a node is found and executed its age is reset
 * to the "young" value.
 * How to make node ages decay in an optimal way is another matter;
 * suggestions are welcome.
 */
#define NODELIFE(n)	((n)->len<<1)
#define CLEANFREQ	8
#define AGENODE		12

/*
 * Another optimization issue - it is more costly to store very small
 * sequences or to reparse them every time? Needs some analysis.
 */
#define MIN_FRAGLEN	2

/////////////////////////////////////////////////////////////////////////////

#define makekey(a)	(a)

#define NEXTNODE(g)	{if ((g)->rtag==MINUS) (g)=(g)->link[1];\
			  else { (g)=(g)->link[1];\
			  while ((g)->link[0]!=NULL) (g)=(g)->link[0];}}

static inline TNode *Tmalloc(void)
{
  TNode *G  = TNodePool->link[0];
  TNode *G1 = G->link[0];
  if (G1==TNodePool) leavedos(0x4c4c); // return NULL;
  TNodePool->link[0] = G1; G->link[0]=NULL; return G;
}

static inline void Tfree(TNode *G)
{
  G->key = 0; G->addr = NULL;
  G->link[0] = TNodePool->link[0];
  TNodePool->link[0] = G;
}

/////////////////////////////////////////////////////////////////////////////

static inline void datacopy(TNode *nd, TNode *ns)
{
  char *s = (char *)&(ns->key);
  char *d = (char *)&(nd->key);
  int l = sizeof(TNode)-offsetof(TNode,key);
  memcpy(d,s,l);
}

static TNode *avltr_probe (const long key, int *found)
{
  avltr_tree *tree = &CollectTree;
  TNode *t;
  TNode *s, *p, *q, *r;
  int k = 1;

  t = &tree->root;
  s = p = t->link[0];

  if (s == NULL) {
      tree->count++;
      ninodes = tree->count;
      q = t->link[0] = Tmalloc();
      q->link[0] = NULL;
      q->link[1] = t;
      q->rtag = MINUS;
      q->bal = 0;
      return q;
  }

  for (;;) {
      int diff = (key - p->key);

      if (diff < 0) {
	  p->cache = 0;
	  q = p->link[0];
	  if (q == NULL) {
	      q = Tmalloc();
	      p->link[0] = q;
	      q->link[0] = NULL;
	      q->link[1] = p;
	      q->rtag = MINUS;
	      break;
	  }
      }
      else if (diff > 0) {
	  p->cache = 1;
	  q = p->link[1];
	  if (p->rtag == MINUS) {
	      q = Tmalloc();
	      q->link[1] = p->link[1];
	      q->rtag = p->rtag;
	      p->link[1] = q;
	      p->rtag = PLUS;
	      q->link[0] = NULL;
	      break;
	  }
      }
      else {	/* found */
        *found = 1;
	return p;
      }

      if (q->bal != 0) t = p, s = q;
      p = q;
      k++;
/**/ if (k>=AVL_MAX_HEIGHT) leavedos(0x777);
      if (k>MaxDepth) MaxDepth=k;
  }
  
  tree->count++;
  ninodes = tree->count;
  if (ninodes > MaxNodes) MaxNodes = ninodes;
  q->bal = 0;
  
  r = p = s->link[(int) s->cache];
  while (p != q) {
      p->bal = p->cache * 2 - 1;
      p = p->link[(int) p->cache];
  }

  if (s->cache == 0) {
      if (s->bal == 0) {
	  s->bal = -1;
	  return q;
      }
      else if (s->bal == +1) {
	  s->bal = 0;
	  return q;
      }

      if (r->bal == -1)	{
	  p = r;
	  if (r->rtag == MINUS) {
	      s->link[0] = NULL;
	      r->link[1] = s;
	      r->rtag = PLUS;
	  }
	  else {
	      s->link[0] = r->link[1];
	      r->link[1] = s;
	  }
	  s->bal = r->bal = 0;
      }
      else {
	  p = r->link[1];
	  r->link[1] = p->link[0];
	  p->link[0] = r;
	  s->link[0] = p->link[1];
	  p->link[1] = s;
	  if (p->bal == -1) s->bal = 1, r->bal = 0;
	    else if (p->bal == 0) s->bal = r->bal = 0;
	      else s->bal = 0, r->bal = -1;
	  p->bal = 0;
	  p->rtag = PLUS;
	  if (s->link[0] == s) s->link[0] = NULL;
	  if (r->link[1] == NULL) {
	      r->link[1] = p;
	      r->rtag = MINUS;
	  }
      }
  }
  else {
      if (s->bal == 0) {
	  s->bal = 1;
	  return q;
      }
      else if (s->bal == -1) {
	  s->bal = 0;
	  return q;
      }

      if (r->bal == +1)	{
	  p = r;
	  if (r->link[0] == NULL) {
	      s->rtag = MINUS;
	      r->link[0] = s;
	  }
	  else {
	      s->link[1] = r->link[0];
	      s->rtag = PLUS;
	      r->link[0] = s;
	  }
	  s->bal = r->bal = 0;
      }
      else {
	  p = r->link[0];
	  r->link[0] = p->link[1];
	  p->link[1] = r;
	  s->link[1] = p->link[0];
	  p->link[0] = s;
	  if (p->bal == +1) s->bal = -1, r->bal = 0;
	    else if (p->bal == 0) s->bal = r->bal = 0;
	      else s->bal = 0, r->bal = 1;
	  p->rtag = PLUS;
	  if (s->link[1] == NULL) {
	      s->link[1] = p;
	      s->rtag = MINUS;
	  }
	  if (r->link[0] == r) r->link[0] = NULL;
	  p->bal = 0;
      }
  }
		
  if (t != &tree->root && s == t->link[1]) t->link[1] = p;
    else t->link[0] = p;

  return q;
}

  
static void avltr_delete (const long key)
{
  avltr_tree *tree = &CollectTree;
  TNode *pa[AVL_MAX_HEIGHT];		/* Stack P: Nodes. */
  unsigned char a[AVL_MAX_HEIGHT];	/* Stack P: Bits. */
  int k = 1;				/* Stack P: Pointer. */
  TNode *p;

  a[0] = 0;
  pa[0] = &tree->root;
  p = tree->root.link[0];
  if (p == NULL) return;

  for (;;) {
      int diff = (key - p->key);

      if (diff==0) break;
      pa[k] = p;
      if (diff < 0) {
	  if (p->link[0] == NULL) return;
	  p = p->link[0]; a[k] = 0;
      }
      else if (diff > 0) {
	  if (p->rtag != PLUS) return;
	  p = p->link[1]; a[k] = 1;
      }
      k++;
/**/ if (k>=AVL_MAX_HEIGHT) leavedos(0x777);
  }
  if (d.emu>DT_LEV) e_printf("Found node to delete at %08lx\n",(long)p);
  tree->count--;
  ninodes = tree->count;

  {
    TNode *t = p;
    TNode **q = &pa[k - 1]->link[(int) a[k - 1]];

    if (t->rtag == MINUS) {
	if (t->link[0] != NULL) {
	    TNode *const x = t->link[0];

	    *q = x;
	    (*q)->bal = 0;
	    if (x->rtag == MINUS) {
		if (a[k - 1] == 1) x->link[1] = t->link[1];
		  else x->link[1] = pa[k - 1];
	    }
	}
	else {
	    *q = t->link[a[k - 1]];
	    if (a[k - 1] == 0) pa[k - 1]->link[0] = NULL;
	      else pa[k - 1]->rtag = MINUS;
	}
    }
    else {
	TNode *r = t->link[1];
	if (r->link[0] == NULL) {
	    r->link[0] = t->link[0];
	    r->bal = t->bal;
	    if (r->link[0] != NULL) {
		TNode *s = r->link[0];
		while (s->rtag == PLUS) s = s->link[1];
		s->link[1] = r;
	    }
	    *q = r;
	    a[k] = 1;
	    pa[k++] = r;
	}
	else {
	    TNode *s = r->link[0];

	    a[k] = 1;
	    pa[k++] = t;

	    a[k] = 0;
	    pa[k++] = r;
	    
	    while (s->link[0] != NULL) {
		r = s;
		s = r->link[0];
		a[k] = 0;
		pa[k++] = r;
	    }

	    if (t->addr) free(t->addr);
	    if (NextKey==s->key) NextNode=t;
	      else if (NextKey==t->key) NextKey=-1;
// e_printf("<03 node exchange %08lx->%08lx>\n",(long)s,(long)t);
	    datacopy(t, s);
	    s->addr = NULL;
	    s->key = 0;

	    if (s->rtag == PLUS) r->link[0] = s->link[1];
	      else r->link[0] = NULL;
	    p = s;
	}
    }
  }

/**/ if (Traverser.p==p) Traverser.init=0;
  if (d.emu>DT_LEV) e_printf("Removed ITree-node %08lx\n",(long)p);
  if (NextKey==p->key) NextKey=-1;
  if (p->addr) free(p->addr);
  Tfree(p);

  while (--k) {
      TNode *const s = pa[k];

      if (a[k] == 0) {
	  TNode *const r = s->link[1];
	  
	  if (s->bal == -1) {
	      s->bal = 0;
	      continue;
	  }
	  else if (s->bal == 0) {
	      s->bal = +1;
	      break;
	  }

	  if (s->rtag == MINUS || r->bal == 0) {
	      s->link[1] = r->link[0];
	      r->link[0] = s;
	      r->bal = -1;
	      pa[k - 1]->link[(int) a[k - 1]] = r;
	      break;
	  }
	  else if (r->bal == +1) {
	      if (r->link[0] != NULL) {
		  s->rtag = PLUS;
		  s->link[1] = r->link[0];
	      }
	      else
		s->rtag = MINUS;
	      r->link[0] = s;
	      s->bal = r->bal = 0;
	      pa[k - 1]->link[a[k - 1]] = r;
	  }
	  else {
	      p = r->link[0];
	      if (p->rtag == PLUS) r->link[0] = p->link[1];
	        else r->link[0] = NULL;
	      p->link[1] = r;
	      p->rtag = PLUS;
	      if (p->link[0] == NULL) {
		  s->link[1] = p;
		  s->rtag = MINUS;
	      }
	      else {
		  s->link[1] = p->link[0];
		  s->rtag = PLUS;
	      }
	      p->link[0] = s;
	      if (p->bal == +1)	s->bal = -1, r->bal = 0;
	        else if (p->bal == 0) s->bal = r->bal = 0;
		  else s->bal = 0, r->bal = +1;
	      p->bal = 0;
	      pa[k - 1]->link[(int) a[k - 1]] = p;
	      if (a[k - 1] == 1) pa[k - 1]->rtag = PLUS;
	  }
      }
      else {
	  TNode *const r = s->link[0];
	  
	  if (s->bal == +1) {
	      s->bal = 0;
	      continue;
	  }
	  else if (s->bal == 0) {
	      s->bal = -1;
	      break;
	  }

	  if (s->link[0] == NULL || r->bal == 0) {
	      s->link[0] = r->link[1];
	      r->link[1] = s;
	      r->bal = +1;
	      pa[k - 1]->link[(int) a[k - 1]] = r;
	      break;
	  }
	  else if (r->bal == -1) {
	      if (r->rtag == PLUS) s->link[0] = r->link[1];
	        else s->link[0] = NULL;
	      r->link[1] = s;
	      r->rtag = PLUS;
	      s->bal = r->bal = 0;
	      pa[k - 1]->link[a[k - 1]] = r;
	  }
	  else {
	      p = r->link[1];
	      if (p->link[0] != NULL) {
		  r->rtag = PLUS;
		  r->link[1] = p->link[0];
	      }
	      else
		r->rtag = MINUS;
	      p->link[0] = r;
	      if (p->rtag == MINUS) s->link[0] = NULL;
	        else s->link[0] = p->link[1];
	      p->link[1] = s;
	      p->rtag = PLUS;
	      if (p->bal == -1)	s->bal = +1, r->bal = 0;
	        else if (p->bal == 0) s->bal = r->bal = 0;
		  else s->bal = 0, r->bal = -1;
	      p->bal = 0;
	      if (a[k - 1] == 1)
		pa[k - 1]->rtag = PLUS;
	      pa[k - 1]->link[(int) a[k - 1]] = p;
	  }
      }
  }
}


static void avltr_destroy(void)
{
  avltr_tree *tree = &CollectTree;

  if (tree->root.link[0] != &tree->root) {
      TNode *an[AVL_MAX_HEIGHT];	/* Stack A: nodes. */
      char ab[AVL_MAX_HEIGHT];		/* Stack A: bits. */
      int ap = 0;			/* Stack A: height. */
      TNode *p = tree->root.link[0];

      for (;;) {
	  while (p != NULL) {
	      ab[ap] = 0;
	      an[ap++] = p;
	      p = p->link[0];
	  }

	  for (;;) {
	      if (ap == 0) return;

	      p = an[--ap];
	      if (ab[ap] == 0) {
		  ab[ap++] = 1;
		  if (p->rtag == MINUS) continue;
		  p = p->link[1];
		  break;
	      }
	      if (p->addr) free(p->addr);
	      Tfree (p);
	  }
      }
  }
}


/////////////////////////////////////////////////////////////////////////////


static int TraverseAndClean(void)
{
  hitimer_t t0;
  TNode *p;

  t0 = GETTSC();
  if (Traverser.init == 0) {
      Traverser.p = p = &CollectTree.root;
      Traverser.init = 1;
  }
  else
      p = (TNode *)Traverser.p;

  /* walk to next node */
  NEXTNODE(p);
  if (p == &CollectTree.root) {
      NEXTNODE(p);
  }

  if ((p->addr != NULL) && (p->jcount > 0)) {
      p->jcount -= AGENODE;
  }
  if ((p->addr == NULL) || (p->jcount <= 0)) {
      if (d.emu>DT_LEV) e_printf("Delete node %08lx\n",p->key);
      avltr_delete(p->key);
  }
  else {
      if (d.emu>(DT_LEV+1))
	e_printf("TraverseAndClean: node %08lx of %d count=%d\n",
		(long)p,ninodes,p->jcount);
      Traverser.p = p;
  }
  CleanupTime += (GETTSC() - t0);
  return 1;
}


/*
 * Add a node to the collector tree.
 * The code is linearly stored in the CodeBuf and its associated structures
 * are in the InstrMeta array. We allocate a buffer and copy the code, then
 * we copy the sequence data from the head element of InstrMeta. In this
 * process we lose all the correspondances between original code and compiled
 * code addresses. At the end, we reset both CodeBuf and InstrMeta to prepare
 * for a new sequence.
 */
TNode *Move2ITree(void)
{
  IMeta *G0 = &InstrMeta[0];		// root of code buffer
  TNode *nI;
  hitimer_t t0 = GETTSC();
  long key;
  int len, found;

  key = makekey((long)G0->npc);

  if (G0->ncount < MIN_FRAGLEN) {
#ifndef SINGLESTEP
	if (d.emu>(DT_LEV+1)) e_printf("Short sequence not collected\n");
#endif
	nI = NULL;
	goto noinode;
  }

  found = 0;
  nI = avltr_probe(key, &found);

  if (found) {
	if (d.emu>DT_LEV) {
		e_printf("Equal keys: replace TNode %d at=%08lx key=%08lx\n",
			ninodes,(long)nI,key);
	}
	/* ->REPLACE the code of the node found with the latest
	   compiled version */
	if (nI->addr) free(nI->addr);
  }
  else {
	if (d.emu>DT_LEV) {
		e_printf("New TNode %d at=%08lx key=%08lx\n",
			ninodes,(long)nI,key);
		if (d.emu>(DT_LEV+1))
			e_printf("Head len=%d n_ops=%d PC=%08lx\n",
				G0->len, G0->ncount, (long)G0->npc);
	}
	nI->key = key;
  }

  nI->npc = G0->npc;
  nI->cklen = G0->cklen;
  nI->len = len = G0->len + sizeof(TailCode);
  nI->jcount = NODELIFE(nI);
  nI->addr = (unsigned char *)malloc(len+8);
  if (d.emu>(DT_LEV+1)) e_printf("Move sequence from %08lx to %08lx l=%d\n",
	(long)G0->addr, (long)nI->addr, len);
  memcpy(nI->addr, G0->addr, len);

  if ((long)nI->npc >= 0x1000) {
    long mp = ((long)nI->npc)&~(PAGE_SIZE-1);
    if (mp != PrevMp) {
	e_printf("Mprotect %08lx\n",mp);
	if (mprotect((void *)mp,PAGE_SIZE,PROT_READ))
		perror("mprotect");
    }
    PrevMp = mp;
  }  
  nI->flags = G0->flags;

noinode:
  LastIMeta = NULL;
  NextFreeIMeta = 0;
  memset(&InstrMeta[0],0,sizeof(IMeta));
  ResetCodeBuf();
  AddTime += (GETTSC() - t0);
  return nI;
}


TNode *FindTree(unsigned char *addr)
{
  TNode *I;
  long key;
  static int tccount=0;
  hitimer_t t0;

  t0 = GETTSC();
  key = makekey((long)addr);

  /* cache hits on next key are about 30% */
  if (key==NextKey) {
	I = NextNode;
  }
  else {
    I = CollectTree.root.link[0];
    if (I == NULL) return NULL;

    for (;;) {
      int diff = (key - I->key);

      if (diff < 0) {
	  I = I->link[0];
	  if (I == NULL) goto endsrch;
      }
      else if (diff > 0) {
	  if (I->rtag == MINUS) goto endsrch;
	  I = I->link[1];
      }
      else break;
    }
  }

  if (I && I->addr && (I->jcount>0)) {
	TNode *G;
	I->jcount = NODELIFE(I);
	if (d.emu>(DT_LEV+1)) e_printf("Found key %08lx count=%d\n",
		key, I->jcount);

	G = I; NEXTNODE(G); NextNode = G;
	NextKey = (NextNode==&CollectTree.root? -1 : NextNode->key);

	SearchTime += (GETTSC() - t0);
	return I;
  }

endsrch:
  SearchTime += (GETTSC() - t0);

  if (((++tccount) >= CLEANFREQ) || NodesCleaned) {
	  do {
		(void)TraverseAndClean();
		if (NodesCleaned) NodesCleaned--;
	  } while (NodesCleaned > 0);
	  tccount=0;
  }

  if (d.emu>(DT_LEV+2)) e_printf("Not found key %08lx\n",key);
  return NULL;
}


/////////////////////////////////////////////////////////////////////////////

static void GCPrint(unsigned char *cp, int len)
{
	int i;
	while (len) {
		dbug_printf(">>> %08lx:",(long)cp);
		for (i=0; (i<16) && len; i++,len--) dbug_printf(" %02x",*cp++);
		dbug_printf("\n");
	}
}

static void print_structure (avltr_tree *tree, TNode *node, int level)
{
  char lc[] = "([{<`";
  char rc[] = ")]}>'";

  if (node == NULL)
    {
      fprintf (stderr," :nil");
      return;
    }
  else if (level >= AVL_MAX_HEIGHT)
    {
      fprintf(stderr,"Too deep, giving up.\n");
      return;
    }
  else if (node == &tree->root)
    {
      fprintf (stderr," root");
      return;
    }
  fprintf (stderr," %c%08lx", lc[level % 5], node->key);
  fflush (stderr);

  print_structure (tree, node->link[0], level + 1);
  fflush (stderr);

  if (node->rtag == PLUS)
    print_structure (tree, node->link[1], level + 1);
  else if (node->link[1] != &tree->root)
    fprintf (stderr," :%08lx", node->link[1]->key);
  else
    fprintf (stderr," :r");
  fflush (stderr);

  fprintf (stderr,"%c", rc[level % 5]);
  fflush (stderr);
}


/////////////////////////////////////////////////////////////////////////////
/*
 * We come here:
 *   a) from a fault on a protected memory page. A page is protected
 *	when code has been found on it. In this case, len is zero.
 *   b) from a disk read, no matter if the pages were protected or not.
 *	When we read something from disk we must mark as dirty anything
 *	present on the memory we are going to overwrite. In this case,
 *	len is greater than 0.
 * Too bad the smallest memory unit is a 4k page; DOS programs used to
 * be quite small. It is not unusual to find code and data/stack on the
 * same 4k page.
 *
 */
void InvalidateTreePaged (unsigned char *addr, int len)
{
  TNode *G = &CollectTree.root;
  long al, ah;
  hitimer_t t0;

  t0 = GETTSC();
  /* No hope - we have to clean the whole page. Remember that at this
   * point the page has been unprotected, so anything can happen on it.
   * The other way would be to emulate the faulting instruction and
   * reprotect the page immediately - but too many faults could slow
   * down the emulation more than reparsing some code. Of course there
   * is always a worst case.
   */
  al = (long)addr & ~(PAGE_SIZE-1);		/* base of mem page */
  ah = max((al+PAGE_SIZE),((long)addr+len));	/* end of dirty range */
  /* maybe a block can begin before al but extend beyond it - try to trap */
  al = min(al,(long)addr-256);

  /* find nearest (lesser than) node */
  G = G->link[0]; if (G == NULL) goto quit;
  for (;;) {
      if (G->key > al) {
	if (G->link[0]==NULL) break;
	G = G->link[0];
      }
      else if (G->key < al) {
        TNode *G2;
	if (G->rtag == MINUS) break;
	G2 = G->link[1];
	if (G2->key > ah) break; else G = G2;
      }
      else break;
  }

  /* walk tree in ascending, hopefully sorted, address order */
  for (;;) {
      if (G == &CollectTree.root) break;
      if (G->key > ah) break;

      if (G->addr && (G->jcount>0)) {
	if ((G->key+G->cklen) > al) {
	  e_printf("Invalidated node %08lx at %08lx\n",(long)G,G->key);
	  /* do not free the code block, as we can be executing from it */
	  G->jcount = 0;
	  if (G->key==NextKey) NextKey = -1;
	  NodesCleaned++;
	}
      }
      NEXTNODE(G);
  }
quit:
  PrevMp = 0;
  CleanupTime += (GETTSC() - t0);
}


/////////////////////////////////////////////////////////////////////////////


static void CleanIMeta(void)
{
	hitimer_t t0 = GETTSC();
	NextFreeIMeta = 0;
	memset(&InstrMeta[0],0,sizeof(IMeta));
	LastIMeta = NULL;
	ResetCodeBuf();
	CleanupTime += (GETTSC() - t0);
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
		if (d.emu>(DT_LEV+2)) {
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
	int i;
	TNode *G;

	CollectTree.root.link[0] = NULL;
	CollectTree.root.link[1] = &CollectTree.root;
	CollectTree.root.rtag = PLUS;
	CollectTree.count = 0;
	Traverser.init = 0;
	Traverser.p = NULL;
	if (d.emu>1) e_printf("Root tree node at %08lx\n",
		(long)&CollectTree.root);

	G = TNodePool = (TNode *)calloc(NODES_IN_POOL, sizeof(TNode));
	if (d.emu>1) e_printf("TNode pool at %08lx\n",(long)G);
	for (i=0; i<(NODES_IN_POOL-1); i++) {
	    TNode *G1 = G; G++;
	    G1->link[0] = G;
	}
	G->link[0] = TNodePool;

	memset(&InstrMeta[0], 0, sizeof(IMeta));
	NextFreeIMeta = 0;
	ForwIRef = NULL;
	LastIMeta = NULL;
	PrevMp = 0;
	MaxDepth = MaxNodes = 0;
}

void EndGen(void)
{
	CleanIMeta();
	avltr_destroy();
	free(TNodePool);
}

/////////////////////////////////////////////////////////////////////////////

