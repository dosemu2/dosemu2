

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
	extern unsigned _dsval;
	REGS regs;
	char path[128];

	


	regs.AX = sizeof(path);
	regs.DX = &path;
	regs.ES = _dsval;  
	sysint(0xe6, &regs, &regs);
	printf("%s\n", path);
	
}

