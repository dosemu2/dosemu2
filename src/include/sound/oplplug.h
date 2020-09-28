#ifndef OPLPLUG_H
#define OPLPLUG_H

#include <stdint.h>

struct opl_ops {
    uint8_t (*PortRead)(void *impl, uint16_t port);
    void (*PortWrite)(void *impl, uint16_t port, uint8_t val );
    void *(*Create)(int opl3_rate);
    void (*Generate)(int total, int16_t output[][2]);
};

void opl_register_ops(struct opl_ops *ops);

#endif
