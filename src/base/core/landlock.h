#ifndef LANDLOCK_H
#define LANDLOCK_H

int landlock_init(void);
int landlock_allow(const char *path, int ro);
int landlock_allow_fd(int fd, int ro);
int landlock_allow_file(const char *path, int ro);
int landlock_lock(void);

#endif
