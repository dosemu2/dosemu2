/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * mkfatimage.c: Make a FAT hdimage pre-loaded with files.
 * 
 * Copyright (C) 1995 by Pasi Eronen.
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * Note:
 * - This program doesn't support name mangling and does very little
 *   checking for non-DOS filenames.
 * - Disk full condition isn't detected (and probably causes
 *   erratic behaviour).
 * - Duplicate files aren't detected.
 * - There's no real boot sector code, just "dosemu exit" call.
 * 
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "bootsect.h"


/* These can be changed -- at least in theory. In practise, it doesn't
 * seem to work very well (I don't know why). 
 */
#define SECTORS_PER_TRACK 17
#define HEADS 4
#define TRACKS 36
#define SECTORS_PER_FAT 1
#define ROOT_DIRECTORY_ENTRIES 512
#define SECTORS_PER_CLUSTER 8

/* I wouldn't touch these. First, the partition information. */
/* Track 0, head 0 is reserved. */
#define P_STARTING_HEAD 1
#define P_STARTING_TRACK 0
#define P_STARTING_SECTOR 1
#define P_STARTING_ABSOLUTE_SECTOR SECTORS_PER_TRACK
#define P_ENDING_HEAD (HEADS-1)
#define P_ENDING_TRACK (TRACKS-1)
#define P_ENDING_SECTOR SECTORS_PER_TRACK
/* Minus 1 for head 0, track 0 (partition table plus empty space). */
#define P_SECTORS ((HEADS*TRACKS-1)*SECTORS_PER_TRACK)
/* Partition status is bootable. */
#define P_STATUS 0x80
/* File system: 12-bit FAT */
#define P_TYPE 0x01

#define BYTES_PER_SECTOR 512
#define MEDIA_DESCRIPTOR 0xf8
#define FAT_COPIES 2
#define RESERVED_SECTORS 1
#define HIDDEN_SECTORS SECTORS_PER_TRACK
#define BYTES_PER_CLUSTER (SECTORS_PER_CLUSTER*BYTES_PER_SECTOR)
#define SECTORS_PER_ROOT_DIRECTORY ((ROOT_DIRECTORY_ENTRIES*32)/BYTES_PER_SECTOR)


struct input_file 
{
  char *filename;
  char dos_filename[12];
  long size;
  time_t mtime;
  int starting_cluster, ending_cluster;
  int size_in_clusters;
};

static struct input_file input_files[ROOT_DIRECTORY_ENTRIES];
static int input_file_count = 0;
static char *volume_label = "";
static unsigned char buffer[BYTES_PER_SECTOR];
static unsigned char fat[SECTORS_PER_FAT*BYTES_PER_SECTOR];
static unsigned char root_directory[SECTORS_PER_ROOT_DIRECTORY*BYTES_PER_SECTOR];
/* Where to place next input file. */
static int first_available_cluster = 2;

/* Some macros for little-endian data access. */
#define put_word(buffer, data) \
 (((buffer)[0] = ((data) >> 0) & 0xff),\
  ((buffer)[1] = ((data) >> 8) & 0xff))
#define put_dword(buffer, data) \
 (((buffer)[0] = ((data) >> 0) & 0xff),\
  ((buffer)[1] = ((data) >> 8) & 0xff),\
  ((buffer)[2] = ((data) >> 16) & 0xff),\
  ((buffer)[3] = ((data) >> 24) & 0xff))


/* Zero the sector buffer. */
static void clear_buffer(void) 
{
  memset(buffer, 0, BYTES_PER_SECTOR);
}

/* Write the sector buffer to disc. */
static void write_buffer(void)
{
  fwrite(buffer, 1, BYTES_PER_SECTOR, stdout);
}

/* Set a FAT entry 'n' to value 'data'. */
static void put_fat(int n, int value)
{
  if (n & 1) 
  {
    fat[(n/2)*3+1] = (fat[(n/2)*3+1] & ~0xf0) | ((value & 0x0f) << 4);
    fat[(n/2)*3+2] = value >> 4;
  }
  else 
  {
    fat[(n/2)*3] = value & 0xff;
    fat[(n/2)*3+1] = (fat[n/2*3+1] & ~0x0f) | (value >> 8);
  }
}

/* Set root directory entry 'n'. */
static void put_root_directory(int n, struct input_file *f)
{
  unsigned char *p = &root_directory[n*32];
  struct tm *tm;
  tm = localtime(&f->mtime);
  put_word(&p[24], (((tm->tm_year - 80) & 0x1f) << 9) |
    (((tm->tm_mon + 1) & 0xf) << 5) |
    (tm->tm_mday & 0x1f));
  put_word(&p[22], ((tm->tm_hour & 0x1f) << 11) |
    ((tm->tm_min & 0x3f) << 5) |
    ((tm->tm_sec>>1) & 0x1f));
  memmove(p, f->dos_filename, 11);
  put_word(&p[26], f->starting_cluster);
  put_dword(&p[28], f->size);
}


/* Get input file information, create DOS filename, and add it to
 * input_files array. 
 */
static void add_input_file(char *filename)
{
  char tmp[14], *base, *ext, *p;
  struct stat s;
  struct input_file *i = &input_files[input_file_count];

  /* Check that the root directory isn't full (also count volume label). */
  if (input_file_count >= (ROOT_DIRECTORY_ENTRIES-1)) 
  {
    fprintf(stderr, "mkfatimage: Root directory full!\n");
    return;
  }

  /* Create DOS file name. */
  if ((base = strrchr(filename, '/')) == NULL)
    base = filename;
  else
    base++;
  ext=NULL;
  if (strlen(base) <= 12) 
  {
    strcpy(tmp, base);
    base = tmp;
    if ((ext = strchr(tmp, '.')) != NULL)
      *ext++ = '\0';
    else
      ext = base + strlen(base);
  }
  if ((strlen(base) == 0) || (strlen(base) > 8) ||
    (strlen(ext) > 3) || (strchr(ext, '.') != NULL)) 
  {
    fprintf(stderr, "%s: File name is not DOS-compatible\n", filename);
    return;
  }
  sprintf(i->dos_filename, "%-8s%-3s", base, ext);
  for (p = i->dos_filename; (*p != '\0'); p++)
    *p = toupper(*p);
  i->filename = strdup(filename);

  /* Get the rest of file information. */
  if (stat(filename, &s) != 0)
  {
    fprintf(stderr, "%s: %s\n", filename, strerror(errno));
    return;
  }
  i->size = s.st_size;
  i->mtime = s.st_mtime;     
  if (i->size == 0)
  {
    i->size_in_clusters = 0;
    i->starting_cluster = i->ending_cluster = 0;
  }
  else
  {
    i->size_in_clusters = (s.st_size+BYTES_PER_CLUSTER-1)/BYTES_PER_CLUSTER;
    i->starting_cluster = first_available_cluster;
    i->ending_cluster = i->starting_cluster + i->size_in_clusters - 1;
    first_available_cluster += i->size_in_clusters;
  }
  input_file_count++;
}


static void usage(void)
{
  fprintf(stderr, "Usage: mkfatimage [-l volume-label] [file...]\n");
}


int main(int argc, char *argv[])
{
  int n, m;
  
  /* Parse command line. */
  if ((argc <= 1) && isatty(STDOUT_FILENO))
  {
    usage();
    exit(1);
  }
  while ((n = getopt(argc, argv, "l:")) != EOF)
  {
    switch (n)
    {
    case 'l':
      volume_label = strdup(optarg);
      for (n = 0; (volume_label[n] != '\0'); n++)
        volume_label[n] = toupper(volume_label[n]);
      if (n > 11)
        volume_label[11] = '\0';
      break;
      
    default:
      usage();
      exit(1);
    }
  }
  while (optind < argc)
    add_input_file(argv[optind++]);


  /* Write dosemu image header. */
  clear_buffer();
  memmove(&buffer[0], "DOSEMU", 7);
  put_dword(&buffer[7], HEADS);
  put_dword(&buffer[11], SECTORS_PER_TRACK);
  put_dword(&buffer[15], TRACKS);
  put_dword(&buffer[19], 128);
  fwrite(buffer, 1, 128, stdout);

  /* Write our master boot record */
  clear_buffer();
  buffer[0] = 0xeb;                     /* Jump to dosemu exit code. */
  buffer[1] = 0x3c;                     /* (jmp 62; nop) */
  buffer[2] = 0x90; 
  buffer[62] = 0xb8;                    /* Exec MBR. */
  buffer[63] = 0xfe;                    /* (mov ax,0xfffe; int 0xe6) */
  buffer[64] = 0xff;
  buffer[65] = 0xcd;
  buffer[66] = 0xe6;
  buffer[446+0] = P_STATUS;
  buffer[446+1] = P_STARTING_HEAD;
  buffer[446+2] = ((P_STARTING_TRACK >> 2) & 0xc0) | P_STARTING_SECTOR;
  buffer[446+3] = P_STARTING_TRACK & 0xff;
  buffer[446+4] = P_TYPE;
  buffer[446+5] = P_ENDING_HEAD;
  buffer[446+6] = ((P_ENDING_TRACK >> 2) & 0xc0) | P_ENDING_SECTOR;
  buffer[446+7] = P_ENDING_TRACK & 0xff;
  put_dword(&buffer[446+8], P_STARTING_ABSOLUTE_SECTOR);
  put_dword(&buffer[446+12], P_SECTORS);
  put_word(&buffer[510], 0xaa55);
  write_buffer();
  
  /* Write empty sectors to fill track 0. */
  clear_buffer();
  for (n = 2; (n <= P_STARTING_ABSOLUTE_SECTOR); n++)
    write_buffer();

  /* Write partition boot sector. */
  clear_buffer();
  memcpy(buffer, boot_sect, boot_sect_end - boot_sect);

  put_word(&buffer[11], BYTES_PER_SECTOR);
  buffer[13] = SECTORS_PER_CLUSTER;
  put_word(&buffer[14], RESERVED_SECTORS);
  buffer[16] = FAT_COPIES;
  put_word(&buffer[17], ROOT_DIRECTORY_ENTRIES);
  put_word(&buffer[19], P_SECTORS);
  buffer[21] = MEDIA_DESCRIPTOR;
  put_word(&buffer[22], SECTORS_PER_FAT);
  put_word(&buffer[24], SECTORS_PER_TRACK);
  put_word(&buffer[26], HEADS);
  put_word(&buffer[28], HIDDEN_SECTORS);
  put_dword(&buffer[32], 0); 
  buffer[36] = 0x80;
  buffer[38] = 0x29;
  put_dword(&buffer[39], 0x12345678);   /* Serial number */
  memmove(&buffer[43], "           ", 11);
  memmove(&buffer[43], volume_label, strlen(volume_label));
  memmove(&buffer[54], "FAT12   ", 8);
  put_word(&buffer[510], 0xaa55);
  write_buffer();
  
  /* Write FATs. */
  memset(fat, 0, sizeof(fat));
  put_fat(0, 0xff8);
  put_fat(1, 0xfff);
  for (n = 0; (n < input_file_count); n++) 
  {
    for (m = input_files[n].starting_cluster; (m < input_files[n].ending_cluster); m++)
      put_fat(m, m+1);
    put_fat(m, 0xfff);
  }
  for (n = 1; (n <= FAT_COPIES); n++)
    fwrite(fat, 1, SECTORS_PER_FAT*BYTES_PER_SECTOR, stdout);
  
  /* Write root directory. */
  memset(root_directory, 0, sizeof(root_directory));
  m = 0;
  /* If there's a volume label, add it first. */
  if (strlen(volume_label) > 0) 
  {
    unsigned char *p = &root_directory[m*32];
    sprintf(p, "%-11s", volume_label);
    p[11] = 0x08;
    m++;
  }
  for (n = 0; (n < input_file_count); n++)
  {
    put_root_directory(m, &input_files[n]);
    m++;
  }
  fwrite(root_directory, 1, SECTORS_PER_ROOT_DIRECTORY*BYTES_PER_SECTOR, stdout);
  
  /* Write data area. */
  for (n = 0; (n < input_file_count); n++) 
  {
    FILE *f = fopen(input_files[n].filename, "rb");
    if (f == NULL) 
    {
      fprintf(stderr, "%s: %s\n", input_files[n].filename, strerror(errno));
      continue;
    }
    for (m = 0; (m < (input_files[n].size_in_clusters*SECTORS_PER_CLUSTER)); m++) 
    {
      clear_buffer();
      fread(buffer, 1, BYTES_PER_SECTOR, f);
      write_buffer();
    }
    fclose(f);
  }
  return(0);
}
