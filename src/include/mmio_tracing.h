#ifndef MMIO_TRACING_H
#define MMIO_TRACING_H

#include <stdbool.h>
#include "emu.h"
#include "dosemu_debug.h"

#define MMIO_TRACING_MAX_REGIONS 16
#define MMIO_READ 0x01
#define MMIO_WRITE 0x02

extern void register_mmio_tracing(dosaddr_t startaddr,
                                  dosaddr_t stopaddr);
extern bool mmio_check(dosaddr_t addr);
extern uint8_t mmio_trace_byte(dosaddr_t addr, uint8_t value,
                               uint8_t type);
extern uint16_t mmio_trace_word(dosaddr_t addr, uint16_t value,
                                uint8_t type);
extern uint32_t mmio_trace_dword(dosaddr_t addr, uint32_t value,
                                 uint8_t type);
extern uint64_t mmio_trace_qword(dosaddr_t addr, uint64_t value,
                                 uint8_t type);

#endif                          /* MMIO_TRACING_H */
