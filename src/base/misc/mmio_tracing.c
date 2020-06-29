#include "mmio_tracing.h"

struct mmio_address_range {
  dosaddr_t start, stop;
};

struct mmio_tracing_config {
  struct mmio_address_range address_ranges[MMIO_TRACING_MAX_REGIONS];
  unsigned valid_ranges;
  dosaddr_t min_addr, max_addr;
};

static struct mmio_tracing_config mmio_tracing_config;


static void mmio_tracing_scrub(void)
{
  if ((config.cpuemu != 4) ||
      (config.cpu_vm != 2) || (config.cpu_vm_dpmi != 2))
    error("MMIO: tracing is only only working for fully simulated cpu. "
          "Config must be set to '$_cpu_vm=\"emulated\"', '$_cpu_vm_dpmi=\"emulated\"' and '$_cpu_emu=\"fullsim\"'\n");
}

void register_mmio_tracing(dosaddr_t startaddr, dosaddr_t stopaddr)
{
  if (stopaddr < startaddr) {
    error("MMIO: address order wrong.");
    return;
  }

  if (mmio_tracing_config.valid_ranges < MMIO_TRACING_MAX_REGIONS - 1) {
    if (mmio_tracing_config.valid_ranges == 0) {
      mmio_tracing_config.min_addr = startaddr;
      mmio_tracing_config.max_addr = stopaddr;
      register_config_scrub(mmio_tracing_scrub);
    } else {
      if (startaddr < mmio_tracing_config.min_addr)
        mmio_tracing_config.min_addr = startaddr;
      if (stopaddr > mmio_tracing_config.max_addr)
        mmio_tracing_config.max_addr = stopaddr;
    }
    mmio_tracing_config.address_ranges[mmio_tracing_config.valid_ranges].
        start = startaddr;
    mmio_tracing_config.address_ranges[mmio_tracing_config.valid_ranges].
        stop = stopaddr;
    mmio_tracing_config.valid_ranges++;
  } else
    error
        ("MMIO: Too many address regions to trace. Increase MMIO_TRACING_MAX_REGIONS to allow some more...");
}

bool mmio_check(dosaddr_t addr)
{
  /* to not slow down too much for any other memory access (not in tracing region,
     MMIO is usually in some distance to RAM) */
  if ((addr >= mmio_tracing_config.min_addr)
      && (addr <= mmio_tracing_config.max_addr)) {
    for (unsigned k = 0; k < mmio_tracing_config.valid_ranges; k++) {
      if ((addr >= mmio_tracing_config.address_ranges[k].start) &&
          (addr <= mmio_tracing_config.address_ranges[k].stop))
        return true;
    }
  }
  return false;
}

uint8_t mmio_trace_byte(dosaddr_t addr, uint8_t value, uint8_t type)
{
  switch (type) {
    case MMIO_READ:
      F_printf("MMIO: Reading byte at %X: %02X\n", addr, value);
      break;
    case MMIO_WRITE:
      F_printf("MMIO: Writing byte at %X: %02X\n", addr, value);
      break;
    default:
      F_printf("MMIO: Failed. Wrong arguments.");
  }
  return value;
}

uint16_t mmio_trace_word(dosaddr_t addr, uint16_t value, uint8_t type)
{
  switch (type) {
    case MMIO_READ:
      F_printf("MMIO: Reading word at %X: %04X\n", addr, value);
      break;
    case MMIO_WRITE:
      F_printf("MMIO: Writing word at %X: %04X\n", addr, value);
      break;
    default:
      F_printf("MMIO: Failed. Wrong arguments.");
  }
  return value;
}

uint32_t mmio_trace_dword(dosaddr_t addr, uint32_t value, uint8_t type)
{
  switch (type) {
    case MMIO_READ:
      F_printf("MMIO: Reading dword at %X: %08X\n", addr, value);
      break;
    case MMIO_WRITE:
      F_printf("MMIO: Writing dword at %X: %08X\n", addr, value);
      break;
    default:
      F_printf("MMIO: Failed. Wrong arguments.");
  }
  return value;
}

uint64_t mmio_trace_qword(dosaddr_t addr, uint64_t value, uint8_t type)
{
  switch (type) {
    case MMIO_READ:
      F_printf("MMIO: Reading qword at %X: %016lX\n", addr, value);
      break;
    case MMIO_WRITE:
      F_printf("MMIO: Writing qword at %X: %016lX\n", addr, value);
      break;
    default:
      F_printf("MMIO: Failed. Wrong arguments.");
  }
  return value;
}
