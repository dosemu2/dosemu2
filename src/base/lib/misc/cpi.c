/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: CPI font loader
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 * based on codepage.c from kbd package.
 * Original copyrights below:
 * Author: Ahmed M. Naas (ahmed@oea.xs4all.nl)
 * Many changes: aeb@cwi.nl  [changed until it would handle all
 *    *.cpi files people have sent me; I have no documentation,
 *    so all this is experimental]
 * Remains to do: DRDOS fonts.
 *
 * Copyright: Public domain.
 */

//#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include "cpi.h"

#define PACKED __attribute__((packed))

struct FontFileHeader {
	unsigned char id0;
	char id[7];
	unsigned char res[8];
	unsigned short pnum;      /* number of pointers */
	unsigned char ptyp;       /* type of pointers */
	uint32_t fih_offset; /* FontInfoHeader offset */
	unsigned short num_codepages;
} PACKED;

struct CPEntryHeader {
	unsigned short size;
	uint32_t off_nexthdr;
	unsigned short device_type; /* screen=1; printer=2 */
	unsigned char device_name[8];
	unsigned short codepage;
	unsigned char res[6];
	uint32_t off_font;
} PACKED;

struct CPInfoHeader {
	unsigned short reserved;
	unsigned short num_fonts;
	unsigned short size;
} PACKED;

struct ScreenFontHeader {
	unsigned char height;
	unsigned char width;
	unsigned short reserved;
	unsigned short num_chars;
} PACKED;

static uint8_t *find_font(uint8_t *data, uint16_t cp,
	uint8_t w, uint8_t h, off_t size, int *r_size)
{
    struct FontFileHeader hdr;
    struct CPEntryHeader cph;
    struct CPInfoHeader cpi;
    struct ScreenFontHeader sf;
    uint8_t *p = data;
    off_t remain = size;

    if (remain < sizeof(hdr))
	return NULL;
    memcpy(&hdr, p, sizeof(hdr));
    p += sizeof(hdr);
    remain -= sizeof(hdr);
    if (strncmp(hdr.id, "FONT", 4) != 0)
	return NULL;
    while (hdr.num_codepages--) {
	if (remain < sizeof(cph))
	    return NULL;
	memcpy(&cph, p, sizeof(cph));
	p += sizeof(cph);
	remain -= sizeof(cph);
	if (cph.device_type != 1)
	    return NULL;
#define NEXT_HDR() \
    p = data + cph.off_nexthdr; \
    remain = size - cph.off_nexthdr \

	if (cph.codepage != cp) {
	    NEXT_HDR();
	    continue;
	}
	p = data + cph.off_font;
	remain = size - cph.off_font;
	if (remain < sizeof(cpi))
	    return NULL;
	memcpy(&cpi, p, sizeof(cpi));
	p += sizeof(cpi);
	remain -= sizeof(cpi);
	while (cpi.num_fonts--) {
	    int flen;
	    if (remain < sizeof(sf))
		return NULL;
	    memcpy(&sf, p, sizeof(sf));
	    p += sizeof(sf);
	    remain -= sizeof(sf);
	    flen = sf.height * sf.num_chars;
	    if (sf.width == w && sf.height == h) {
		*r_size = flen;
		return p;
	    }
	    p += flen;
	    remain -= flen;
	}
	NEXT_HDR();
    }
    return NULL;
}

uint8_t *cpi_load_font(const char *path, uint16_t cp,
	uint8_t w, uint8_t h, int *r_size)
{
    uint8_t *ret = NULL;
    glob_t p;
    char *wild;
    int fd;
    int rc;
    struct stat st;
    uint8_t *file_data = NULL;
    uint8_t *font_data;
    int i;

    asprintf(&wild, "%s/*.cpi", path);
    glob(wild, 0, NULL, &p);
    free(wild);
    for (i = 0; i < p.gl_pathc; i++) {
	fd = open(p.gl_pathv[i], O_RDONLY);
	if (fd == -1)
	    goto err1;
	rc = fstat(fd, &st);
	if (rc == -1)
	    goto err2;
	file_data = malloc(st.st_size);
	rc = read(fd, file_data, st.st_size);
	if (rc != st.st_size)
	    goto err3;
	close(fd);
	font_data = find_font(file_data, cp, w, h, st.st_size, r_size);
	if (font_data) {
	    ret = malloc(*r_size);
	    memcpy(ret, font_data, *r_size);
	}
	free(file_data);
	if (ret)
	    return ret;
    }
    goto err1;

err3:
    free(file_data);
err2:
    close(fd);
err1:
    globfree(&p);
    return NULL;
}

#if 0
int main(int argc, char *argv[])
{
    uint8_t *p;
    int len;
    if (argc != 4)
	return 1;
    p = cpi_load_font(argv[1], atoi(argv[2]), 8, atoi(argv[3]), &len);
    if (p) {
	printf("font found, len %i\n", len);
	free(p);
    }
    return 0;
}
#endif
