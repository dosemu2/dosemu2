#include <stdio.h>

#define LIBSTART 	0x400000

void (*dosemu)();
char dummy[1088*1024]; /* make sure that the lower 1MB+64K is unused */

int main(int argc, char **argv)
{
	if (uselib("/lib/libemu") != 0) {
		printf("cannot load shared lib\n");
		exit(1);
	}
	dosemu = (void *) LIBSTART;
	dosemu(argc, argv);
}
