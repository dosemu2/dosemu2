/* dos emulator, Matthias Lautner */
#define DISKS_C 1
/* Extensions by Robert Sanders, 1992-93 
 *
 * this file is way too ugly for my tastes, but I'll go ahead
 * and release it like this...I'll make it pretty when I have the
 * time.
 */
/* floppy disks, dos partitions or their images (files) (maximum 8 heads) */

#include "emu.h"

#define DI_5  {"diskimage", 0, 15, 2, 80, FIVE_INCH_FLOPPY}  /* type 0 */
#define DI_3  {"diskimage", 0, 18, 2, 80, THREE_INCH_FLOPPY} /* type 1 */
#define FD0_5 {"/dev/fd0", 0, 0, 0, 0, FIVE_INCH_FLOPPY}     /* type 2 */
#define FD0_3 {"/dev/fd0", 0, 0, 0, 0, THREE_INCH_FLOPPY}    /* type 3 */
#define FD1_5 {"/dev/fd1", 0, 0, 0, 0, FIVE_INCH_FLOPPY}     /* type 4 */
#define FD1_3 {"/dev/fd1", 0, 0, 0, 0, THREE_INCH_FLOPPY}    /* type 5 */

/* the emulator only uses 2 floppies by default (the first 2).  you may
 * enable the third by giving dos the parameter "-F 3", but having
 * more than 3 floppies has caused LINUX.EXE to fail for me 
 */

struct disk disktab[DISKS] = {
#if FLOPPY_A == 0
DI_5,
#elif FLOPPY_A == 1
DI_3,
#elif FLOPPY_A == 2
FD0_5,
#elif FLOPPY_A == 3
FD0_3,
#elif FLOPPY_A == 4
FD1_5,
#elif FLOPPY_A == 5
FD1_3,
#else
#error "You chose a bad type for FLOPPY_A"
#endif

#if FLOPPY_B == 0
DI_5,
#elif FLOPPY_B == 1
DI_3,
#elif FLOPPY_B == 2
FD0_5,
#elif FLOPPY_B == 3
FD0_3,
#elif FLOPPY_B == 4
FD1_5,
#elif FLOPPY_B == 5
FD1_3,
#else
#error "You chose a bad type for FLOPPY_B"
#endif

#if EXTRA_FLOPPY == 0
DI_5
#elif EXTRA_FLOPPY == 1
DI_3
#elif EXTRA_FLOPPY == 2
FD0_5
#elif EXTRA_FLOPPY == 3
FD0_3
#elif EXTRA_FLOPPY == 4
FD1_5
#elif EXTRA_FLOPPY == 5
FD1_3
#else
#error "You chose a bad type for EXTRA_FLOPPY"
#endif
};

/*  to get a 1.2MB, 5 1/4" diskimage, change it's entry line to this:
 *      {"diskimage",0,15,2,80,FIVE_INCH_FLOPPY},
 *  You will need to do this if you are booting from a diskimage created
 *  by dd'ing a real bootable 5.25 disk, i.e.
 *      dd if=/dev/fd0 of=diskimage bs=1024 count=1200
 *
 * alternately, you might want to try switching the order of the entries
 * in disktab[] so that the "/dev/fd0" entry comes first, allowing you to
 * boot from your A: drive.  Make sure you set the FIVE/THREE_INCH_FLOPPY
 * correctly 
 */

/* whole hard disks, dos extended partitions (containing one or
 * more partitions)  or their images (files) 
 *
 * I suggest that if you are using direct access to your /dev/hda or
 * /dev/hdb drive that you first try it with the readonly flag set to 1
 * for a trial run.
 */

struct disk hdisktab[HDISKS] = {
		{"hdimage", 0, 17, 4, 40},
		{"/dev/hda", 0, 35, 12, 987},  
	      };
/***********************************************************************/
/*****          format of the hard disk entry                      *****/
/***********************************************************************/
/*		{"/dev/hda", 1, 17, 9, 900},  */
/*			     ^ readonly (1=readonly, 0=read/write)	*/
/*				^ sectors 				*/
/*				    ^ heads				*/
/*					^ cylinders			*/
/***********************************************************************/
#undef DISKS_C
