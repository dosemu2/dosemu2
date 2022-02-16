#ifndef VIRQ_H
#define VIRQ_H

enum { VIRQ_MOUSE, VIRQ_PKT, VIRQ_IPX, VIRQ_MAX };
enum VirqHwRet { VIRQ_HWRET_DONE, VIRQ_HWRET_CONT };
enum VirqSwRet { VIRQ_SWRET_DONE, VIRQ_SWRET_BH };

void virq_init(void);
void virq_reset(void);
void virq_setup(void);
void virq_raise(int virq_num);
void virq_register(int virq_num, enum VirqHwRet (*hw_handler)(void *),
        enum VirqSwRet (*sw_handler)(void *), void *arg);
void virq_unregister(int virq_num);

#endif
