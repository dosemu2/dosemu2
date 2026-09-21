/*
 * Put run286 in front of a Phar Lap bound program, so the game runs as a
 * DOS program of its own instead of through the extender it was bound to.
 *
 * The result is run286.exe with the whole original file appended to it and
 * an eight byte trailer saying where that copy starts; the loader reads the
 * image back out of its own file. Nothing inside the game's image is
 * rewritten, because an NE counts its segment offsets from the start of the
 * file it lives in, and the copy is whole.
 *
 * This is a host tool, not part of the DOS side.
 *
 * Free software, GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SELF_MAGIC	"R286"
#define CHUNK		65536

static int copy(FILE *out, FILE *in, unsigned long *written)
{
    char buf[CHUNK];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
	if (fwrite(buf, 1, n, out) != n)
	    return -1;
	*written += n;
    }
    return ferror(in) ? -1 : 0;
}

int main(int argc, char **argv)
{
    FILE *ldr, *game, *out;
    unsigned long off = 0, total = 0;
    unsigned char trailer[8];

    if (argc != 4) {
	fprintf(stderr, "usage: %s <run286.exe> <game.exe> <out.exe>\n",
		argv[0]);
	return 2;
    }
    ldr = fopen(argv[1], "rb");
    if (!ldr) {
	perror(argv[1]);
	return 1;
    }
    game = fopen(argv[2], "rb");
    if (!game) {
	perror(argv[2]);
	return 1;
    }
    out = fopen(argv[3], "wb");
    if (!out) {
	perror(argv[3]);
	return 1;
    }
    if (copy(out, ldr, &off) != 0) {
	fprintf(stderr, "%s: cannot copy the loader\n", argv[0]);
	return 1;
    }
    total = off;
    if (copy(out, game, &total) != 0) {
	fprintf(stderr, "%s: cannot copy the program\n", argv[0]);
	return 1;
    }
    memcpy(trailer, SELF_MAGIC, 4);
    trailer[4] = off & 0xff;
    trailer[5] = (off >> 8) & 0xff;
    trailer[6] = (off >> 16) & 0xff;
    trailer[7] = (off >> 24) & 0xff;
    if (fwrite(trailer, 1, sizeof(trailer), out) != sizeof(trailer) ||
	    fclose(out) != 0) {
	fprintf(stderr, "%s: cannot write %s\n", argv[0], argv[3]);
	return 1;
    }
    fclose(ldr);
    fclose(game);
    printf("%s: %lu bytes, the program at %lu\n", argv[3],
	    total + sizeof(trailer), off);
    return 0;
}
