#include <string.h>
#include <stdio.h>
#include <dos.h>



int main(int argc, char **argv)
{
    static char command_line[256] = "";
    struct REGPACK preg;

    argv++;

    while(*argv)
    {
	strcat(command_line, *argv);
	strcat(command_line, " ");
	argv++;
    }

    printf("Effective commandline: %s\n", command_line);

    preg.r_ax = 0xfe;
    preg.r_dx = &command_line;
    preg.r_es = _DS;
    intr(0xe6, &preg);

    return(0);
}
