#ifndef UFFD_H
#define UFFD_H

int uffd_open(int sock);
int uffd_attach(void);
int uffd_reattach(void *addr, size_t len);
int uffd_init(int sock);
int uffd_reinit(void *addr, size_t len);
int uffd_wp(int idx, void *addr, size_t len, int prot);

#endif
