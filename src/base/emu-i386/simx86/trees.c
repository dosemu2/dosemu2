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
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
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

#include <stddef.h>  // offsetof
#include <pthread.h>
#include "emu86.h"
#include "codegen.h"
#include "trees.h"

/* Tree structure to store collected code sequences */
static struct fh_bucket bs[FHSIZE];
static fhmap CollectTree;

IMeta	InstrMeta[MAXINODES];
int	CurrIMeta;
unsigned char *BrokenCodePtr;

#if SPEC_PREJIT
static pthread_mutex_t trees_mtx = PTHREAD_MUTEX_INITIALIZER;
#define pthread_mutex_lock_trees() pthread_mutex_lock(&trees_mtx)
#define pthread_mutex_unlock_trees() pthread_mutex_unlock(&trees_mtx)
#else
#define pthread_mutex_lock_trees()
#define pthread_mutex_unlock_trees()
#endif

int NodesParsed = 0;
int NodesExecd = 0;
int NodesPrejitted = 0;
int CleanFreq = 8;
int CreationIndex = 0;

#if PROFILE
int MaxDepth = 0;
int MaxNodes = 0;
int MaxNodeSize = 0;
int TotalNodesParsed = 0;
int TotalNodesExecd = 0;
int PrejitNodesExecd = 0;
int NodesFound = 0;
int NodesNotFound = 0;
int TreeCleanups = 0;
#endif

#ifdef DEBUG_TREE
static void DumpTree (FILE *fd);
#endif

//static int NodeLimit = 10000;

TNode *FindTree_unlocked(int key);

#define RANGE_INTERSECT(al,ah,l,h)	({int _l2=(al);\
	int _h2=(ah); ((_h2 > (l)) && (_l2 < (h))); })
#define ADDR_IN_RANGE(a,l,h)		({typeof(a) _a2=(a);	\
	((_a2 >= (l)) && (_a2 < (h))); })

/////////////////////////////////////////////////////////////////////////////

static TNode *find_node(unsigned a, int l)
{
  TNode *ret;
  int64_t key = m_findnode(a, l);
  if (key == -1)
    return NULL;
  ret = FindTree_unlocked(key);
  assert(ret);
  return ret;
}

static TNode *find_node_start(unsigned a, int l)
{
  TNode *ret;
  int64_t key = m_findnodestart(a);
  if (key == -1)
    return find_node(a, l);
  ret = FindTree_unlocked(key);
  assert(ret);
  return ret;
}

/////////////////////////////////////////////////////////////////////////////

static void avltr_probe(TNode *item)
{
  fh_add(&CollectTree, item->key, &item->fhnode);
}


static void avltr_delete(TNode *p)
{
  fh_del(&CollectTree, &p->fhnode);
#if !defined(SINGLESTEP)&&!defined(SINGLEBLOCK)
  if (debug_level('e')>2) e_printf("Remove node %p\n",p);
#endif
  free(p);
}

/////////////////////////////////////////////////////////////////////////////

static void avltr_init(void)
{
  fh_init(&CollectTree, bs, FHSIZE, ZONE_P2SZ,
      (int)offsetof(TNode, key) - (int)offsetof(TNode, fhnode));

  g_printf("avltr_init\n");
}

void avltr_destroy(void)
{
#if PROFILE >= 2
  hitimer_t t0 = 0;
#endif

#ifdef DEBUG_TREE
  DumpTree (tLog);
#endif
#if PROFILE >= 2
  if (debug_level('e')) t0 = GETTSC();
#endif

  mprot_end();
#if PROFILE
  if (debug_level('e')) {
    TreeCleanups++;
#if PROFILE >= 2
    CleanupTime += (GETTSC() - t0);
#endif
  }
#endif
}


/////////////////////////////////////////////////////////////////////////////

/*
 * Given addr with translated code (from e.g., a fault) find the
 * corresponding original PC. This is slow but it's only used for
 * DPMI exceptions.
 */
unsigned int FindPC(const unsigned char *addr)
{
  TNode *G;
  Addr2Pc *AP;
  int i;

  G = FindTree(TheCPU.key);
  if (!G)
    return 0;
  AP = G->meta;
  for (i = 0; i < G->seqnum + 1; i++) {
    e_printf("     %08x:%p",(G->key+AP->dnpc),G->addr+AP->daddr);
    if (addr < G->addr+AP->daddr) break;
    AP++;
  }
  assert(i > 0 && i <= G->seqnum);
  e_printf("\nFindPC: PC=%x\n", G->key+(AP-1)->dnpc);
  return G->key+(AP-1)->dnpc;
}

/////////////////////////////////////////////////////////////////////////////
/*
 * The node linker.
 *
 * For the x86 target, code sequence can have one of three termination types:
 *
 *	1) straight end (no jump), or unconditional jump or call
 *
 *	key:	|
 *		|
 *		|
 *		mov $next_addr,eax
 *		pop edx (flags)
 *		ret
 *
 *	2) conditional jump or loop
 *
 *	key:	|
 *		|
 *		|
 *		jcond taken
 *		<optional signal check code (CKSIGN)>
 *		mov $not_taken_addr,eax
 *		pop edx (flags)
 *		ret
 *	taken:  <optional signal check code (JB_LINK)>
 *		mov $taken_addr,eax
 *		pop edx (flags)
 *		ret
 *
 *      3) indirect jump or ret
 *	key:	|
 *		| <eax is new IP>
 *		add eax, <CS Base>
 *		pop edx (flags)
 *		ret
 *
 * In the first case, there's only one linking point; in the second, two.
 * Linking means replacing the "mov addr,eax" instruction with a direct
 * jump to the start point of the next code fragment.
 * The parameters used are (t_ means taken, nt_ means not taken):
 *	t_ref,nt_ref	pointers to next node
 *	t_link,nt_link	addresses of the patch point
 *	t_target,nt_target jump targets, also used for unlinking
 * Since a node can be referred from many others, we need to keep
 * "back-references" in a list in order to unlink it.
 */

static unsigned tnode_nrefs(const TNode *G)
{
	unsigned nrefs = 0;
	backref *B;

	for (B=G->bkr; B; B=B->next)
		nrefs++;
	return nrefs;
}

static void linknode(TNode *LG, TNode *G, linkdesc *L, char branch)
{
	backref *B;

	// points to current node, which can't be a forever loop?
	if (L->target!=G->key || !L->link ||
	    (G->mode & MTRAP) || (LG->mode & MTRAP))
		return;

	if (L->ref!=0) {
		dbug_printf("Linker: ref at %08x busy\n",LG->key);
		leavedos_main(0x8102 + (branch == 'N'));
	}
	IGen IG = (IGen){.op = JMP_LINK, .mode = MPATCH|MLINK,
			 .p0 = L->target, .link = G->addr};
	CodeGen(LG->addr + L->link, LG->addr, &IG);
	L->ref = G;
	B = calloc(1,sizeof(backref));
	// head insertion
	B->next = G->bkr;
	G->bkr = B;
	B->ref = LG;
	B->branch = branch;
	if (G==LG) {
		G->flags |= F_SLFL;
		if (debug_level('e')>1) {
			e_printf("Linker: node (%p:%08x:%p) SELF link\n"
				 "\t\ttarget=%08x, %c_ref %d=%p\n",
				 G,G->key,G->addr,
				 L->target, B->branch, tnode_nrefs(G), L->ref);
		}
	}
	else if (debug_level('e')>1) {
		e_printf("Linker: previous node (%p:%08x:%p)\n"
			 "\t\tlinked to (%p:%08x:%p)\n"
			 "\t\ttarget=%08x, %c_ref %d=%p\n",
			 LG,LG->key,LG->addr,
			 G,G->key,G->addr,
			 L->target, B->branch, tnode_nrefs(G), L->ref);
	}
	if (debug_level('e')>8) {
		backref *bk = G->bkr;
#ifdef DEBUG_LINKER
		if (bk==NULL) {
			dbug_printf("bkr null\n");
			leavedos_main(0x8108 + (branch == 'N'));
		}
#endif
		while (bk) { dbug_printf("bkref=%c%p\n",bk->branch,
			bk->ref); bk=bk->next; }
	}
}

void NodeLinker(TNode *LG, TNode *G)
{
#if PROFILE >= 2
	hitimer_t t0 = 0;
#endif

#if !defined(SINGLESTEP)
	if (!UseLinker)
#endif
	    return;

#if PROFILE >= 2
	if (debug_level('e')) t0 = GETTSC();
#endif
	if (debug_level('e')>8 && LG) e_printf("NodeLinker: %08x->%08x\n",LG->key,G->key);

	if (LG) {
		linknode(LG, G, &LG->clink_t, 'T');
		linknode(LG, G, &LG->clink_nt, 'N');
	}
#if PROFILE >= 2
	if (debug_level('e')) LinkTime += (GETTSC() - t0);
#endif
}

static void unlinknode(TNode *G, linkdesc *T, char branch)
{
	if (!T->ref) return;

	TNode *H = T->ref;
	backref **Bq = &H->bkr;
	backref *B  = H->bkr;
	if (debug_level('e')>2) e_printf("Unlink fwd %c ref to node %p(%08x)\n",
					 branch, H, H->key);
	while (B) {
		if (B->ref==G) {
			*Bq = B->next;
			free(B);
			break;
		}
		Bq = &B->next;
		B = B->next;
	}
	if (B==NULL) {	// not found...
		dbug_printf("Unlinker: FW %c ref error\n", branch);
		leavedos_main(0x8111 + (branch == 'N'));
	}
	/* we must unlink the forward ref code since the node we linked to
	   may also get deleted, and we may currently be executing G,
	   where in BreakNodeHook the last instruction is excluded from
	   tail code patching */
	IGen IG = (IGen){.op = JMP_LINK, .mode = MPATCH,
			 .p0 = T->target, .p1 = G->key};
	CodeGen(G->addr + T->link, G->addr, &IG);
	T->ref = NULL;
}

static void NodeUnlinker(TNode *G)
{
	linkdesc *T_t = &G->clink_t;
	linkdesc *T_nt = &G->clink_nt;
	backref *B = G->bkr;
#if PROFILE >= 2
	hitimer_t t0 = 0;
#endif

#if !defined(SINGLESTEP)
	if (!UseLinker)
#endif
	    return;
#if PROFILE >= 2
	if (debug_level('e')) t0 = GETTSC();
#endif
	// unlink backward references (from other nodes to the current
	// node)
	if (debug_level('e')>8)
	    e_printf("Unlinker: bkr=%p\n",B);
	while (B) {
	    if (B->branch=='T' || B->branch=='N') {
		TNode *H = B->ref;
		linkdesc *L;
		L = (B->branch=='T') ? &H->clink_t : &H->clink_nt;
		if (debug_level('e')>2) e_printf("Unlinking %c ref from node %p(%08x) to %08x\n",
			B->branch, H, L->target, G->key);
		if (L->target != G->key) {
		    dbug_printf("Unlinker: BK %c ref error t=%08x k=%08x\n",
			B->branch, L->target, G->key);
		    leavedos_main(0x8110);
		}
		IGen IG = (IGen){.op = JMP_LINK, .mode = MPATCH,
				 .p0 = L->target, .p1 = H->key};
		CodeGen(H->addr + L->link, H->addr, &IG);
		L->ref = NULL;
	    }
	    else {
		e_printf("Invalid unlink [%c] ref %p from node ?(?) to %08x\n",
			B->branch, B->ref, G->key);
		leavedos_main(0x8116);
	    }
	    G->bkr = B->next;
	    free(B);
	    B = G->bkr;
	}

	// unlink forward references (from the current node to other
	// nodes), which are backward refs for the other nodes
	if (debug_level('e')>8)
	    e_printf("Unlinker: refs=T%p N%p\n",T_t->ref,T_nt->ref);
	unlinknode(G, T_t, 'T');
	unlinknode(G, T_nt, 'N');
#if PROFILE >= 2
	if (debug_level('e')) LinkTime += (GETTSC() - t0);
#endif
}

/////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_LINKER
#include "codegen-x86.h"

static void checklink(const TNode *G, const linkdesc *L, char branch)
{
  if (!L->link) return;

  const unsigned char *p = G->addr + L->link;
  if (p[0] == 0x0f) p += CKSIGNSIZE;
  if (L->ref) {
	    const TNode *GL = L->ref;
	    if (debug_level('e')>5)
		e_printf("  %c: ref=%p link=%x\n",
			 branch,GL,L->link);
	    if ((*p!=0xe9)&&(*p!=0xeb)&&(*p!=0x48)) {
		error("bad %c link jmp\n", branch); goto nquit;
	    }
	    if (debug_level('e')>5)
		e_printf("  %c: links to %p at %08x with jmp %08x\n",branch,GL,GL->key,
		*p);
	    const backref *B = GL->bkr;
	    assert(B); // bad backref if NULL
	    int n = 0;
	    int brt = 0;
	    while (B) {
		if (B->ref==G) {
		    n++;
		    brt += B->branch;
		    if (debug_level('e')>5) e_printf("  %c: backref %d from %p\n",branch,n,GL);
		}
		B = B->next;
	    }
	    if (n < 1 || n > 2 || (n == 2 && brt != 'N' + 'T')) {
		error("0 or >1 backrefs1 (%i)\n", n); goto nquit;
	    }
  }
  else {
	    if (*p!=0xb9) {
		error("bad %c link jmp\n", branch); goto nquit;
	    }
  }
  return;
nquit:
  leavedos_main(0x9143);
}

static void CheckLinks(void)
{
  avltr_node *p = &CollectTree.root;
  TNode *G;

  for (;;) {
    /* walk to next node */
    p = NEXTNODE(p);
    if (p == &CollectTree.root) {
	e_printf("DEBUG: node link check ok\n");
	return;
    }
    G = p->data;
    if (debug_level('e')>5) e_printf("Node %p at %08x\n",G,G->key);
    checklink(G, &G->clink_t, 'T');
    checklink(G, &G->clink_nt, 'N');
  }
  leavedos_main(0x9143);
}

#endif // DEBUG_LINKER

/*
 * Add a node to the collector tree.
 */
void Move2Tree(TNode *nG)
{
#if PROFILE >= 2
  hitimer_t t0 = 0;
  if (debug_level('e')) t0 = GETTSC();
#endif

  nG->alive = NODELIFE(G);
  pthread_mutex_lock_trees();
  avltr_probe(nG);
#if !defined(SINGLESTEP)&&!defined(SINGLEBLOCK)
  if (debug_level('e')>2) {
		e_printf("New TNode at=%p key=%08x\n",
			nG,nG->key);
		if (debug_level('e')>3)
			e_printf("Header: len=%d n_ops=%d PC=%08x\n",
				nG->len, nG->seqnum, nG->key);
  }
#endif
#if PROFILE
  if (debug_level('e')) if (nG->len > MaxNodeSize) MaxNodeSize = nG->len;
#endif
#ifdef DEBUG_LINKER
  CheckLinks();
#endif
  pthread_mutex_unlock_trees();
#if PROFILE >= 2
  if (debug_level('e')) AddTime += (GETTSC() - t0);
#endif
}

static TNode *FindTree_tail(int key)
{
  TNode *G;
#if PROFILE >= 2
  hitimer_t t0 = 0;
  if (debug_level('e')) t0 = GETTSC();
#endif

  G = fh_find(&CollectTree, key, TNode, fhnode);
# if 0
  assert(G && G->addr);
#else
  if (!G)
    return NULL;
  assert(G->addr);
#endif
  if (debug_level('e')>3) e_printf("Found key %08x\n",key);
  G->alive = NODELIFE(G);
#if PROFILE
  if (debug_level('e')) {
    NodesFound++;
#if PROFILE >= 2
    SearchTime += (GETTSC() - t0);
#endif
  }
#endif
  return G;
}

TNode *FindTree(int key)
{
  TNode *I;

  if (TheCPU.sigprof_pending) {
	CollectStat();
	TheCPU.sigprof_pending = 0;
  }

  pthread_mutex_lock_trees();
  I = FindTree_tail(key);
  pthread_mutex_unlock_trees();
  return I;
}

TNode *FindTree_unlocked(int key)
{
  TNode *I;

  I = FindTree_tail(key);
  return I;
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

static void BreakNode(TNode *G, unsigned char *eip)
{
  Addr2Pc *A = G->meta;
  int ebase;
  unsigned char *p;
  int i;

  if (eip==0) {
	dbug_printf("Cannot break node %08x, eip=%p\n",G->key,eip);
	leavedos_main(0x7691);
  }

  ebase = eip - G->addr;
  for (i=0; i<G->seqnum; i++) {
    if (A->daddr >= ebase) {		// found following instr
	IGen IG = (IGen){.op = JMP_TAILCODE, .mode = MPATCH,
			 .p0 = G->key + A->dnpc};
	p = G->addr + A->daddr;		// translated IP of following instr
	CodeGen(p, G->addr, &IG);
	if (debug_level('e')>1)
		e_printf("============ Force node closing at %08x(%p)\n",
			 (G->key+A->dnpc),p);
	return;
    }
    A++;
  }
  e_printf("============ Node %08x break failed\n",G->key);
}

static void RemoveNode_unlocked(TNode *G)
{
  e_unmarkpage(G->key, G->seqlen);
  NodeUnlinker(G);
  assert(!G->bkr && !G->clink_t.ref && !G->clink_nt.ref && G->addr);
  avltr_delete(G);
}

void RemoveNode(TNode *G)
{
  pthread_mutex_lock_trees();
  unsigned char *p = G->addr;
  RemoveNode_unlocked(G);
  FreeGenCodeBuf(p);
  pthread_mutex_unlock_trees();
}

static unsigned char *BreakNodeHook(TNode *G, unsigned char *eip)
{
    unsigned char *ahE;
    /* if the current eip is in *any* chunk of code that is deleted
        (not just the one written to)
       then we need to break the node immediately to go back to
       the interpreter; otherwise the remaining chunk (that does
       not officially exist anymore) that the SIGSEGV or patched
       call returns to may write to the current unprotected page.
    */
    ahE = G->addr + G->len;
    e_printf("BreakNodeHook %p ahE=%p eip=%p\n", G->addr, ahE, eip);
    if (ADDR_IN_RANGE(eip,G->addr,ahE)) {
	/* Exclude last instruction, as there is no need to break
	 * node after last instruction (it ends there anyway). */
	unsigned char *ahE1 = G->addr + G->meta[G->seqnum - 1].daddr;
	if (debug_level('e')>1)
	    e_printf("### Node self hit %p->%p..%p\n",
		     eip,G->addr,ahE);
	if (ADDR_IN_RANGE(eip,G->addr,ahE1))
	    BreakNode(G, eip);
	/* return pointer to JIT code that can only be freed in codegen.c
	   because we are returning to it */
	return G->addr;
    }
    return NULL;
}

static unsigned char *DoInvalidateNodeRange(int al, int len, unsigned char *eip,
    unsigned char * (*hook)(TNode *, unsigned char *))
{
  TNode *G;
  int ah;
  unsigned char *rc = NULL;
#if PROFILE >= 2
  hitimer_t t0 = 0;

  if (debug_level('e')) t0 = GETTSC();
#endif
  ah = al + len;
  if (debug_level('e')>1) dbug_printf("Invalidate area %08x..%08x\n",al,ah);

  pthread_mutex_lock_trees();
  /* find crossing-or-in-range node */
  G = find_node_start(al, len);
  if (G == NULL) goto quit;
  if (debug_level('e')>1) e_printf("Invalidate from node %08x on\n",G->key);

  /* walk tree in ascending, hopefully sorted, address order */
  for (;;) {
      int ahG;
      if (!G || G->key >= ah)
        break;
      ahG = G->key + G->seqlen;
      assert (G->addr);
      if (RANGE_INTERSECT(G->key,ahG,al,ah)) {
	    unsigned char *p = NULL;
	    if (debug_level('e')>1)
		dbug_printf("Invalidated node %p at %08x\n",G,G->key);
	    if (eip && hook) {
		p = hook(G, eip);
		if (p)
		    rc = p;
	    }
	    if (debug_level('e')>2) e_printf("Delete node %08x\n",G->key);
	    unsigned char *q = G->addr;
	    RemoveNode_unlocked(G);
	    if (p == NULL)
		FreeGenCodeBuf(q);
      }
      if (ahG >= ah)
        break;
      G = find_node(ahG, ah - ahG);
  }
quit:
  pthread_mutex_unlock_trees();
  if (debug_level('e') && e_querymark(al, len))
    error("simx86: InvalidateNodeRange did not clear all code for %#08x, len=%x\n",
	  al, len);
  return rc;
}

void InvalidateNodeRange(int al, int len)
{
  DoInvalidateNodeRange(al, len, NULL, NULL);
}

void InvalidateNodeRangeFromFault(int al, int len, unsigned char *eip)
{
  BrokenCodePtr = DoInvalidateNodeRange(al, len, eip, BreakNodeHook);
}

/////////////////////////////////////////////////////////////////////////////
static void do_invalidate(unsigned data, int cnt)
{
	cnt = PAGE_ALIGN(data + cnt) - (data & HOST_PAGE_MASK);
	data &= HOST_PAGE_MASK;
	InvalidateNodeRange(data, cnt);
}

void e_invalidate_unlocked(unsigned data, int cnt)
{
	/* nothing to invalidate if there are no page protections */
	if (!e_querymprotrange(data, cnt))
		return;
	if (!e_querymark(data, cnt))
		return;
	// no need to invalidate the whole page here,
	// as the page does not need to be unprotected
	InvalidateNodeRange(data, cnt);
}

void e_invalidate(unsigned data, int cnt)
{
	prejit_lock();
	e_invalidate_unlocked(data, cnt);
	prejit_unlock();
}

void e_invalidate_pa(unsigned pa, int cnt)
{
    dosaddr_t addr = physaddr_to_dosaddr(pa, cnt);
    if (addr == (dosaddr_t)-1)
	return;
    e_invalidate(addr, cnt);
}

void e_invalidate_full_pa(unsigned pa, int cnt)
{
    dosaddr_t addr = physaddr_to_dosaddr(pa, cnt);
    if (addr == (dosaddr_t)-1)
	return;
    e_invalidate_full(addr, cnt);
}

/* invalidate and unprotect even if we hit only data.
 * Needed if we are about to destroy the page protection by other means.
 * Otherwise use e_invalidate() */
static void _e_invalidate_full(unsigned data, int cnt)
{
	/* nothing to invalidate if there are no page protections */
	if (!e_querymprotrange(data, cnt))
		return;
	do_invalidate(data, cnt);
}

void e_invalidate_full(unsigned data, int cnt)
{
	prejit_lock();
	_e_invalidate_full(data, cnt);
	prejit_unlock();
}

int e_invalidate_page_full(unsigned data)
{
	int cnt = HOST_PAGE_SIZE;
	data &= HOST_PAGE_MASK;
	/* nothing to invalidate if there are no page protections */
	if (!e_querymprotrange(data, cnt))
		return 0;
	do_invalidate(data, cnt);
	return 1;
}

/////////////////////////////////////////////////////////////////////////////

int NewIMeta(int npc, int override)
{
	int ret = 0;
#if PROFILE >= 2
	hitimer_t t0 = 0;

	if (debug_level('e')) t0 = GETTSC();
#endif
	/* normally we have max MAXINODES-1 instructions in the buffer, but
	   we allow one more for the instruction following STI/POPss/MOVss */
	if (CurrIMeta < MAXINODES-2+override) {
		// add new opcode metadata
		IMeta *I;

		CurrIMeta++;
		I = &InstrMeta[CurrIMeta];
		I->npc = npc;
		I->ngen = 0;
		I->flags = 0;
		ret = 1;
	}

#if PROFILE >= 2
	if (debug_level('e')) AddTime += (GETTSC() - t0);
#endif
	return ret;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef SHOW_STAT
#define CST_SIZE	4096
static struct {
	long long a;
	int b,c,m,d,s;
} xCST[CST_SIZE];
#else
#define CST_SIZE	4
static int xCST[CST_SIZE];
#endif

static int cstx = 0;
static int xCS1 = 0;

void CollectStat (void)
{
	int i;
	unsigned int m = 0;
#ifdef SHOW_STAT
	int csm = config.CPUSpeedInMhz*1000;
//	xCST[cstx].a = TheCPU.EMUtime;
	xCST[cstx].s = TheCPU.sigprof_pending;
	xCST[cstx].b = ninodes;
	xCST[cstx].c = NodesParsed;
	xCST[cstx].d = NodesExecd;
	for (i=cstx-3; i<=cstx; i++) {
		if (i<0) { if (!xCS1) i=0; else i=CST_SIZE+i; }
		m += xCST[i].c;
	}
	m >>= 2;
	xCST[cstx].m = CLEAN_SPEED(FastLog2(m));
	i = cstx;
	if (debug_level('e')>1)
		e_printf("SIGPROF %04d %8d %8d(%3d) %8d %d\n",i,
			xCST[i].b,xCST[i].c,xCST[i].m,xCST[i].d,xCST[i].s);
	cstx++;
	if (cstx==CST_SIZE) {
	    if (!xCS1) xCS1=1;
	    for (i=0; i<cstx; i++) {
		dbug_printf("%04d %16Ld %8d %8d(%3d) %8d %d\n",i,(xCST[i].a/csm),
		    xCST[i].b,xCST[i].c,xCST[i].m,xCST[i].d,xCST[i].s);
	    }
	    cstx=0;
	}
#else
	xCST[cstx++] = NodesParsed;
	if (cstx==CST_SIZE) {
	    if (!xCS1) xCS1=1;
	    cstx=0;
	}
	if (xCS1) {
	    for (i=0; i<CST_SIZE; i++) m += xCST[i];	/* moving average */
	    m /= CST_SIZE;
	    m = FastLog2(m);	/* take leftmost bit index */
	    CreationIndex = CLEAN_SPEED(m);
	    CleanFreq = (8-m); if (CleanFreq<1) CleanFreq=1;
	}
	if (debug_level('e')>1)
		e_printf("SIGPROF %d p=%8d x=%8d ix=%3d cln=%2d\n",
			TheCPU.sigprof_pending,
			NodesParsed,NodesExecd,CreationIndex,
			CleanFreq);
#endif
	NodesParsed = NodesExecd = 0;
}


/////////////////////////////////////////////////////////////////////////////

void InitTrees(void)
{
	g_printf("InitTrees\n");

	avltr_init();

	NodesParsed = NodesExecd = 0;
	CleanFreq = 8;
	cstx = xCS1 = 0;
	CreationIndex = 0;
#if PROFILE
	if (debug_level('e')) {
	    MaxDepth = MaxNodes = MaxNodeSize = 0;
	    TotalNodesParsed = TotalNodesExecd = 0;
	    NodesFound = NodesNotFound = 0;
	    TreeCleanups = 0;
	}
#endif
}

void EndGen(void)
{
#ifdef SHOW_STAT
	int i;
	int csm = config.CPUSpeedInMhz*1000;
#endif
	avltr_destroy();
#ifdef SHOW_STAT
	for (i=0; i<cstx; i++) {
	    dbug_printf("%04d %16Ld %8d %8d(%3d) %8d %d\n",i,(xCST[i].a/csm),
		xCST[i].b,xCST[i].c,xCST[i].m,xCST[i].d,xCST[i].s);
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////
