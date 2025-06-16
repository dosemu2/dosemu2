#ifndef SHLOCK_H
#define SHLOCK_H

void shlock_init(const char *lock_dir);
void *shlock_open(const char *dir, const char *name, int excl, int block);
int shlock_close(void *handle);

#endif
