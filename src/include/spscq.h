#ifndef SPSCQ_H
#define SPSCQ_H

void *spscq_init(unsigned size);
void spscq_done(void *arg);
void *spscq_write_area(void *arg, unsigned *r_len);
void spscq_commit_write(void *arg, unsigned len);
int spscq_read(void *arg, void *buf, unsigned len);

#endif
