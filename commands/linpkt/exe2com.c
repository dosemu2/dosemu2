/*
    exe2com - exe2bin replacement by Chris Dunford/Cove Software

    usage: exe2com [/I] infile [outfile]

    usage is the same as exe2bin except:
        1. Output defaults to COM rather than BIN
        2. Binary fixup option not supported
        3. Checksum not verified
        4. Provides more useful error messages and a warning if a
           COM file is being created with initial IP != 0x100
        5. /I switch provides EXE file info instead of converting

    Compiler notes:
        This source was written for Microsoft C version 5.0.  It
        should be reasonably portable.  Watch out for fseek();
        what it returns seems to vary widely between compilers.

        To compile with MSC, use:

            cl exe2com.c  (no switches necessary)

        We have checked that the source (as of version 1.04) compiles
        without error under Turbo C 1.5.  It appears to operate correctly,
        but we ran only some quick tests; there may be subtle errors here.

    The original version of this program was knocked together in about
    an hour in response to the removal of EXE2BIN from the standard DOS
    distribution disks.  Improvements/corrections are encouraged, but
    please try to coordinate public releases through me.

    Program donated to the public domain by the author.

    cjd 4/17/87


    Version history
    ---------------
    Version 1.04 03/02/88 (CJD)
        Cleaned up some ugly code from the original quickie. Added
        /I (info) switch.  In previous versions, we defined an
        error code for nonzero CS but didn't actually check it; now
        we do.  Source will now compile under either Microsoft C or
        Turbo C.

    Version 1.03 12/30/87 (CJD)
        C86 version converted to Microsoft C (5.0) by Chris
        Dunford.  Increased size of I/O buffer to 4K to improve
        speed; EXE2COM 1.03 is twice as fast as 1.02 and is now
        slightly faster than EXE2BIN.  The C86 version will no
        longer be distributed.

    Version 1.02 11/22/87
    by Chris Blum (CompuServe 76625,1041)
        Fix for even 512-byte boundary file losing last 512 bytes.
        Also corrected signon per request of Chris Dunford (his name
        was lost in the translation to Turbo C).  Version 1.02
        existed in both Turbo C and C86 versions, although only
        the C86 executable was "officially" distributed.

    Version 1.01 was a Turbo C conversion.

    Version 1.00 04/17/87
        Original C86 version by Chris Dunford

*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Version coding */
#define MAJVER   1
#define MINVER   0
#define REVISION 4

/* Conversion error codes */
#define BADREAD  0
#define BADWRITE 1
#define BADSIG   2
#define HASRELO  3
#define HAS_SS   4
#define HAS_CS   5
#define BAD_IP   6
#define TOO_BIG  7
/* This must be the last code */
#define UNKNOWN  8

/* Define size of console output buffer */
#define CONBUFSIZ 2048

/*
**  Define structure of fixed-format part of EXE file header
*/
struct exe_header {
        char exe_sig[2];    /* EXE file signature: "MZ" */
    unsigned excess,        /* Image size mod 512 (valid bytes in last page) */
             pages,         /* # 512-byte pages in image */
             relo_ct,       /* Count of relocation table entries */
             hdr_size,      /* Size of header, in paragraphs */
             min_mem,       /* Min required memory */
             max_mem,       /* Max required memory */
             ss,            /* Stack seg offset in load module */
             sp,            /* Initial value of SP */
             cksum,         /* File checksum */
             ip,            /* Initial value of IP */
             cs,            /* CS offset in load module */
             relo_start,    /* Offset of first relo item */
             ovl_num;       /* Overlay number */
} xh;

FILE *fi,                   /* Input file stream */
     *fo;                   /* Output file stream */

char fin[129],              /* Input file name */
     fon[129];              /* Output file name */

int  info=0;                /* Nonzero if /I found */

char buf[CONBUFSIZ];        /* printf I/O buffer */

char defext[] = ".com";     /* Default output extension - change if you want */

unsigned long code_start,   /* Offset of program image in EXE file */
              code_size;    /* Size of program image, in bytes */

/* Function prototypes */
void init (unsigned, char *[]);
void read_hdr (void);
void disp_info (void);
void convert (void);
void err_xit (unsigned);
void usage (void);

/*
**  program mainline
*/
main(argc, argv)
unsigned argc;
char *argv[];
{
    init (argc, argv);
    read_hdr ();
    if (info)
        disp_info ();
    else
        convert ();
}


/*
**  Initialize - parse arguments, get filenames, open/create files
*/
void init (argc, argv)
unsigned argc;
char **argv;
{
char c, *cp;
int i;

    /* Set up buffered output, display logo */
    setvbuf (stdout, buf, _IOFBF, CONBUFSIZ);
    printf ("exe2com %u.%u%u by Chris Dunford/The Cove Software Group\n",
             MAJVER, MINVER, REVISION);

    /* Get arguments */
    cp = *(++argv);
    for (i=1; i < argc; i++) {
        while ( (cp = strchr (cp, '/')) != (char *) NULL) {
            *cp++ = '\0';
            c = *cp++;
            switch (toupper (c)) {
                case 'I':
                    info = 1;
                    break;
                default:
                    usage ();
            }
        }

        if (**argv)
            if (fin[0] == '\0')
                strcpy (fin, strlwr (*argv));
            else if (fon[0] == '\0')
                strcpy (fon, strlwr (*argv));
            else
                usage ();

        cp = *(++argv);
    }

    /* Check to ensure that an input filename was found *.
    if (fin[0] == '\0') usage ();

    /* If the input file has no extension, add .EXE */
    if (strchr (fin, '.') == (char *) NULL)
        strcat (fin, ".exe");

    /* Copy input name to output if unspecified */
    if (fon[0] == '\0')
        strcpy (fon, fin);

    /* Check output extension--change EXE to COM, or add COM */
    if ((cp = strchr (fon, '.')) == (char *) NULL)
        strcat (fon, defext);
    else if (strcmp (cp, ".exe") == 0)
        strcpy (cp, defext);

    /* Try to open input file */
    if ((fi = fopen (fin, "rb")) == (FILE *) NULL) {
        fprintf (stderr, "exe2com: can't find input file %s\n", fin);
        exit (1);
    }

    /* Try to create output file, if INFO not requested */
    if (!info)
        if ((fo = fopen (fon, "wb")) == (FILE *) NULL) {
            fprintf (stderr, "exe2com: can't open output file %s\n", fin);
            exit (1);
        }
}


/*
**  usage display
*/
void usage (void)
{
    fprintf (stderr, "usage: exe2com [/I] infile [outfile]\n");
    exit (1);
}


/*
**  Read and check the EXE file header
*/
void read_hdr(void)
{
char *cp;

    /* Read the formatted portion of the header */
    if (!fread (&xh, sizeof (struct exe_header), 1, fi))
        err_xit (BADREAD);

    /* Check for "MZ" signature */
    if (strncmp (xh.exe_sig, "MZ", 2))
        err_xit (BADSIG);

    /* Compute offset of program image in module, and program size.
    **
    ** The program size is computed as follows; it cannot exceed 64K bytes:
    **     512 * (# EXE pages - 1)
    **   + valid bytes in last EXE page
    **   - offset of program image in EXE file
    **
    ** Note that if the IP is nonzero, we will skip the first
    ** IP bytes of the program image, and copy IP bytes fewer
    ** than the actual size.
    */
    code_start = ((unsigned long) xh.hdr_size) << 4;
    code_size = (unsigned long) (xh.pages-1) * 512
        + (xh.excess ? xh.excess : 512)    /* fixed 11/19/87 - CJB */
        - code_start;

    /* Don't check anything else if /I requested */
    if (info) return;

    /* Check header; to be convertible, must have:
    **      -- no relocatable items
    **      -- no stack segment
    **      -- no code segment
    **      -- IP == 0 or 100
    **      -- code size < 65536
    */
    if (xh.relo_ct)
        err_xit (HASRELO);
    if (xh.ss || xh.sp)
        err_xit (HAS_SS);
    if (xh.cs)
        err_xit (HAS_CS);
    if (xh.ip != 0 && xh.ip != 0x100)
        err_xit (BAD_IP);
    if (code_size > 65536L)
        err_xit (TOO_BIG);

    /* Issue a warning if COM file and IP != 0x100 */
    if (!strcmp (strchr (fon, '.'), ".com") && xh.ip != 0x100)
        fprintf (stderr, "exe2com warning: COM file, initial IP not 100H\n");

}


/*
**  /i output: display EXE file info
*/
void disp_info (void)
{
char *cp;
unsigned long k;

    cp = strrchr (fin, '\\');
    if (!cp) cp = strchr (fin, ':');
    cp = cp ? cp++ : fin;
    printf ("\n%-20s          (hex)      (dec)\n",cp);

    k = (unsigned long) (xh.pages-1) * 512 + (xh.excess ? xh.excess : 512);
    printf ("  EXE file size               %5lX    %7lu\n", k, k);

    printf ("  EXE header size (para)       %4X    %7u\n", xh.hdr_size, xh.hdr_size);

    putchar (code_size > 65536L ? '*' : ' ');
    printf (" Program image size (bytes)  %5lX    %7lu\n", code_size, code_size);

    k = (unsigned long) xh.min_mem * 16 + code_size;
    printf ("  Minimum load size (bytes)   %5lX    %7lu\n", k, k);

    printf ("  Min allocation (para)        %4X    %7u\n", xh.min_mem, xh.min_mem);

    printf ("  Max allocation (para)        %4X    %7u\n", xh.max_mem, xh.max_mem);

    putchar (xh.cs || (xh.ip != 0x100) ? '*' : ' ');
    printf (" Initial CS:IP           %04X:%04X\n", xh.cs, xh.ip);

    putchar (xh.ss || xh.sp ? '*' : ' ');
    printf (" Initial SS:SP           %04X:%04X    %7u (stack size)\n", xh.ss, xh.sp, xh.sp);

    putchar (xh.relo_ct ? '*' : ' ');
    printf (" Relocation count             %4X    %7u\n", xh.relo_ct, xh.relo_ct);

    printf ("  Relo table start             %04X    %7u\n", xh.relo_start, xh.relo_start);

    printf ("  EXE file checksum            %04X    %7u\n", xh.cksum, xh.cksum);

    printf ("  Overlay number               %4X    %7u\n", xh.ovl_num, xh.ovl_num);

    printf ("* = this item prevents conversion to BIN/COM\n");
}

/*
**  Convert the file.  Nothing to do, really, other than
**  reading the image (which follows the header), and
**  dumping it back out to disk.
*/
void convert (void)
{
#define BUFSIZE 16384
static char buffer[BUFSIZE];  /* Forces buffer out of program stack */
unsigned bsize;

    /* Seek to start of program image, skipping IP bytes */
    if (fseek (fi, code_start+xh.ip, 0))
        err_xit (BADREAD);

    /* Read blocks and copy to output */
    for (code_size -= xh.ip; code_size; code_size -= bsize) {

        /* Set count of bytes to read/write */
        bsize = code_size > BUFSIZE ? BUFSIZE : code_size;

        /* Read and write block */
        if (!fread (buffer, bsize, 1, fi))
            err_xit (BADREAD);
        if (!fwrite (buffer, bsize, 1, fo))
            err_xit (BADWRITE);
    }

    /* All done, close the two files */
    fclose (fi);
    fclose (fo);
}


/*
**  Display an error message, delete output file, exit.
*/
void err_xit (code)
unsigned code;
{
static char *msg[UNKNOWN+1] = {
        "error reading EXE header",
        "error writing output file",
        "invalid EXE file signature",
        "EXE has relocatable items",
        "EXE has stack segment",
        "EXE has nonzero CS",
        "IP not 0 or 100H",
        "program exceeds 64K",
        "unknown internal error"
};

    if (code > UNKNOWN) code = UNKNOWN;
    fprintf (stderr, "exe2com: %s, can't convert\n", msg[code]);

    /* Close two files and delete partial output */
    fclose (fi);
    fclose (fo);
    unlink (fon);

    /* Exit with errorlevel 1 */
    exit (1);
}
