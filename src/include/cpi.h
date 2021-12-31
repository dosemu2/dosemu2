#ifndef CPI_H
#define CPI_H

#include <stdint.h>

uint8_t *cpi_load_font(const char *path, uint16_t cp,
	uint8_t w, uint8_t h, int *r_size);

#endif
