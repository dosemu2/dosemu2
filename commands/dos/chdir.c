

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
	extern int _dsval;
	REGS regs;

	if(argc != 2) {
		printf("Run %s newpath\n", argv[0]);
		exit(1);
	}

	regs.AX = 0x81;
	regs.DX = argv[1];
	regs.ES = _dsval;  
	sysint(0xe6, &regs, &regs);
	exit(regs.AX);
	
}

