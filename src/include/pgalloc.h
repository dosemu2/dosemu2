#ifndef PGALLOC_H
#define PGALLOC_H

void *pgainit(unsigned npages);
void pgadone(void *pool);
void pgareset(void *pool);
int pgaalloc(void *pool, unsigned npages, unsigned id);
void pgafree(void *pool, unsigned page);
struct pgrm {
    int id;
    int pgoff;
};
struct pgrm pgarmap(void *pool, unsigned page);

#endif
