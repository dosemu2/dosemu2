#ifndef DJDEV64_H
#define DJDEV64_H

int djdev64_open(const char *path);
int djdev64_call(int handle, int libid, int fn, unsigned char *sp);
int djdev64_ctrl(int handle, int libid, int fn, unsigned char *sp);
void djdev64_close(int handle);

#endif
