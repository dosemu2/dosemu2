
/*
 * ZALLOC.H
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef unsigned int iaddr_t;	/* unsigned int same size as pointer	*/
typedef int saddr_t;		/* signed int same size as pointer	*/


/* mem.h */

typedef struct MemNode {
    struct MemNode	*mr_Next;
    iaddr_t		mr_Bytes;
} MemNode;

typedef struct MemPool {
    const char 		*mp_Ident;
    void		*mp_Base;  
    void		*mp_End;
    MemNode		*mp_First; 
    void		(*mp_Panic)(const char *ctl, ...);
    int			(*mp_Reclaim)(struct MemPool *memPool, iaddr_t bytes);
    iaddr_t		mp_Size;
    iaddr_t		mp_Used;
} MemPool;

#define MEMNODE_SIZE_MASK       ((sizeof(MemNode) <= 8) ? 7 : 15)

#define INITPOOL(name,panic,reclaim)	{ name, NULL, NULL, NULL, panic, reclaim }

#define ZNOTE_FREE	0
#define ZNOTE_REUSE	1

/* End of mem.h */


#define Prototype extern
#define Library extern

#ifndef NULL
#define NULL	((void *)0)
#endif

/*
 * block extension for sbrk()
 */

#define BLKEXTEND	(64 * 1024)
#define BLKEXTENDMASK	(BLKEXTEND - 1)

/*
 * required malloc alignment.  Use sizeof(long double) for architecture
 * independance.
 *
 * Note: if we implement a more sophisticated realloc, we should ensure that
 * MALLOCALIGN is at least as large as MemNode.
 */

typedef struct Guard {
    size_t	ga_Bytes;
    size_t	ga_Magic;	/* must be at least 32 bits */
} Guard;

#define MATYPE		long double
#define MALLOCALIGN	((sizeof(MATYPE) > sizeof(Guard)) ? sizeof(MATYPE) : sizeof(Guard))
#define GAMAGIC		0x55FF44FD


/* protos.h */

Prototype struct MemPool *DummyStructMemPool;
Library void *znalloc(struct MemPool *mpool, iaddr_t bytes);
Library void *zalloc(struct MemPool *mpool, iaddr_t bytes);
Library void *zallocAlign(struct MemPool *mpool, iaddr_t bytes, iaddr_t align);
Library void *zxalloc(struct MemPool *mp, void *addr1, void *addr2, iaddr_t bytes);
Library void *znxalloc(struct MemPool *mp, void *addr1, void *addr2, iaddr_t bytes);
Library char *zallocStr(struct MemPool *mpool, const char *s, int slen);
Library void zfree(struct MemPool *mpool, void *ptr, iaddr_t bytes);
Library void zfreeStr(struct MemPool *mpool, char *s);
Library void zinitPool(struct MemPool *mp, const char *id, void (*fpanic)(const char *ctl, ...), int (*freclaim)(struct MemPool *memPool, iaddr_t bytes), void *pBase, iaddr_t pSize);
Library void zextendPool(MemPool *mp, void *base, iaddr_t bytes);
Library void zclearPool(struct MemPool *mp);
Library void znop(const char *ctl, ...);
Library int znot(struct MemPool *memPool, iaddr_t bytes);
Library void zallocstats(struct MemPool *mp);

/* End of protos.h */
