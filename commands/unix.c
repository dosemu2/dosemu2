

typedef struct {
	unsigned AX;
	unsigned BX;
	unsigned CX;
	unsigned DX;
	int SI;
        int DI;
        int DS;
        int ES;
} REGS;



main(int argc, char **argv)
{
	static char command_line[256] = "";
	extern unsigned _dsval;
	REGS regs;

	while(*argv) {
		strcat(command_line, *argv);
		strcat(command_line, " ");
		argv++;
	}


	regs.AX = 0xfe;
	regs.DX = &command_line;
	regs.ES = _dsval;  
	sysint(0xe6, &regs, &regs);
	
}

