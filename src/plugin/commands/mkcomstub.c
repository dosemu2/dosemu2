/* 25.02.2002, Grigory Batalov <bga@altlinux.ru>

   This program is intended to replace original
   perl-script from Dosemu Team, so it removes
   dependence from perl-base while packaging.
*/
#include <stdio.h>
#include <string.h>
#define MAXBUFSIZE 512
int
main(int argc, char **argv)
{
  char buf[MAXBUFSIZE];
  unsigned short fsize=0;
  FILE *tmpl,*fout;


  if (argc < 3)
     { printf("USAGE\n\
  mkcomstub template filename buildin_progname [stacksize [heapsize [flags]]]\n\
\n\
where is\n\
\n\
  filename     the name of the file to be generated with .com extension\n\
  buildin_progname   name of the builtin program to make a stub for.\n\
                     (the file generated will get .com extension)\n\
  stacksize    Number, space between top of stack and heapend\n\
               (default MIN_STACK_HEAP_PADDING = 1024)\n\
  heapsize     Number, size of the lowmem heap\n\
               (default: what's left in 64K)\n\
               granularity is LOWMEM_HEAP_GRAN = 128\n\
  flags        Number, will be copied to ctcb->mode_flags\n\
               (1 = print some debug info via -D+c dosemu debugging)\n");

        return 1;
       }
       
  if ((tmpl = fopen(argv[1],"r")) == NULL)
     { printf("unable to read %s",argv[1]);
       return 1;
      }
  fseek(tmpl,6,SEEK_SET);
  fread(&fsize,2,1,tmpl);
  fsize -= 256;
  fseek(tmpl,0,SEEK_SET);
  if ((fout = fopen(argv[2],"w")) == NULL)
     { printf("unable to write %s",argv[2]);
       fclose(tmpl);
       return 1;
      }
  if (fsize < MAXBUFSIZE)
     { fread(buf,1,fsize,tmpl);
       fwrite(buf,1,fsize,fout);
      }
  else
     { while((fsize = fread(buf,1,MAXBUFSIZE,tmpl)) == MAXBUFSIZE)
	  fwrite(buf,1,fsize,fout);
       if (fsize > 0)
	  fwrite(buf,1,fsize,fout);
      }
  
  if (argc > 4)
    { if ((fsize = strlen(argv[4]) + 2) < MAXBUFSIZE)
       { strcpy(buf,argv[4]);
         strcat(buf,",");
	}
     }
   else strcpy(buf,",");
  if (argc > 5)
    if ((fsize += strlen(argv[5]) + 1) < MAXBUFSIZE)
       strcat(buf,argv[5]);
  strcat(buf,",");
  if (argc > 6)
    if ((fsize += strlen(argv[6]) + 1) < MAXBUFSIZE)
       strcat(buf,argv[6]);
   strcat(buf,",");
  if (argc > 3)
    if ((fsize += strlen(argv[3]) + 1) < MAXBUFSIZE)
       strcat(buf,argv[3]);

  if ((fsize = strlen(buf)) > 255)
     fsize = 255;
  fwrite(buf,1,fsize+1,fout);
  fclose(fout);
  fclose(tmpl);
  return 0;
}
