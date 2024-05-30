#ifndef EVTIMER_H
#define EVTIMER_H

#include <stdint.h>

#define NANOSECONDS_PER_SECOND 1000000000LL

#define SCALE_MS 1000000LL
#define SCALE_US 1000LL
#define SCALE_NS 1LL

struct evtimer_ops {
  void *(*create)(void (*cbk)(int ticks, void *), void *arg);
  void (*delete)(void *tmr);
  void (*set_rel)(void *tmr, uint64_t ns, int periodic);
  uint64_t (*gettime)(void *tmr);
  void (*stop)(void *tmr);
  void (*block)(void *tmr);
  void (*unblock)(void *tmr);
};

int register_evtimer(const struct evtimer_ops *ops);

void *evtimer_create(void (*cbk)(int ticks, void *), void *arg);
void evtimer_delete(void *tmr);
void evtimer_set_rel(void *tmr, uint64_t ns, int periodic);
uint64_t evtimer_gettime(void *tmr);
void evtimer_stop(void *tmr);
void evtimer_block(void *tmr);
void evtimer_unblock(void *tmr);

#endif
