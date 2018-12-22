/* mkhdimage.c, for the Linux DOS emulator
 *
 * cheesy program to create an hdimage header
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef __linux__
#include <getopt.h>
#endif
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "disks.h"


static void usage(void)
{
  fprintf(stderr, "mkhdimage [-h <heads>] [-s <sectors>] [-c|-t <cylinders> -f <outfile>]\n");
}

int
main(int argc, char **argv)
{
  int c;
  int ret = 0;
  int fdout = STDOUT_FILENO;
  struct image_header *hdr;

  hdr = malloc(HEADER_SIZE);
  assert(hdr != NULL);
  memset(hdr, 0, HEADER_SIZE);

  strncpy(hdr->sig, "DOSEMU", sizeof(hdr->sig));
  hdr->cylinders = 40;
  hdr->heads = 4;
  hdr->sectors = 17;
  hdr->header_end = HEADER_SIZE;

  while ((c = getopt(argc, argv, "h:s:t:c:s:f:")) != EOF) {
    switch (c) {
    case 'h':
      hdr->heads = atoi(optarg);
      break;
    case 's':
      hdr->sectors = atoi(optarg);
      break;
    case 'c':			/* cylinders */
    case 't':			/* tracks */
      hdr->cylinders = atoi(optarg);
      break;
    case 'f':
      fdout = open(optarg, O_CREAT|O_WRONLY|O_TRUNC, 0644);
      if(fdout < 0) {
        fprintf(stderr, "Could not open file '%s' for writing\n", optarg);
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Unknown option '%c'\n", c);
      usage();
      exit(1);
    }
  }

  ret = write(fdout, hdr, HEADER_SIZE);
  if (ret != HEADER_SIZE) {
    fprintf(stderr, "Failed to write header\n");
    exit(1);
  }

  if(fdout != STDOUT_FILENO) {
    /* Write hole */
    lseek(fdout, (hdr->cylinders*hdr->heads*hdr->sectors*512) - 1, SEEK_CUR);
    ret = write(fdout, "", 1);
    if (ret != 1) {
      fprintf(stderr, "Failed to write disk blocks\n");
      exit(2);
    }
    close(fdout);
  }

  free(hdr);

  return 0;
}
