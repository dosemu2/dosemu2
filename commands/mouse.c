#include <dos.h>
#include <stdlib.h>
#include <stdio.h>

#define MHLP(rin, rout) int86(0xe6, &rin, &rout)

main(int __argc, char * * __argv)
{
  int i;
  union REGS regs;

  printf("Linux Internal Mouse Driver v7.00\n");

  regs.x.ax = 0x0033;
  regs.x.bx = 0x00ff;
  MHLP(regs, regs);
  if (regs.x.ax == 0xffff) {
        printf("ERROR! InternalDriver option not set, enable internal driver in /etc/dosemu.conf.\n");
        exit(1);
  }

  i = 1;

        switch(__argv[i][0]) {
                case 'a':
                        printf("Ignoring Speed settings.\n");
                        regs.h.cl = 0x0001;
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0006;
                        MHLP(regs, regs);
                        printf("Done.\n");
                        break;
                case 'b':
                        printf("Taking Speed settings.\n");
                        regs.h.cl = 0x0000;
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0006;
                        MHLP(regs, regs);
                        printf("Done.\n");
                        break;
                case 'r':
                        printf("Resetting iret.\n");
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0000;
                        MHLP(regs, regs);
                        break;
                case 'i':
                        printf("Inquiry:\n");
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0003;
                        MHLP(regs, regs);
                        if (regs.h.bh == 0x10)
                                printf("        Microsoft Mode (2 Buttons).\n");
                        else 
                                printf("        PC Mouse Mode (3 Buttons).\n");
                        printf("        Vertical Speed   (Y) - %d\n", regs.h.ch);
                        printf("        Horizontal Speed (X) - %d\n", regs.h.cl);
                        printf("        Speed Setting        - %s\n", regs.h.dl ? "Ignoring" : "Taking");
                        break;
                case 'p':
                        printf("Selecting PC Mouse Mode (3 Buttons).\n");
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0002;
                        MHLP(regs, regs);
                        if (regs.h.al == 0xff) {
                                printf("ERROR! Cannot select PC Mouse Mode, emulate3buttons not set in /etc/dosemu.conf.\n");
                                printf("e.g. 'mouse { ps2 /dev/mouse internaldriver emulate3buttons }\n");
                        } else {
                                printf("Done.\n");
                        }
                        break;
                case 'm':
                        printf("Selecting Microsoft Mode (2 Buttons).\n");
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0001;
                        MHLP(regs, regs);
                        printf("Done.\n");
                        break;
                case 'y':
                        printf("Selecting vertical speed to %d.\n",atoi(__argv[i+1])*8);
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0004;
                        regs.h.cl = atoi(__argv[i+1]);
                        MHLP(regs, regs);
                        if (regs.x.ax == 1)
                                printf("ERROR! Selected speed is out of range. Unable to set speed.\n");
                        else
                                printf("Done.\n");
                        i++;
                        break;
                case 'x':
                        printf("Selecting horizontal speed to %d.\n",atoi(__argv[i+1])*8);
                        regs.x.ax = 0x0033;
                        regs.x.bx = 0x0005;
                        regs.h.cl = atoi(__argv[i+1]);
                        MHLP(regs, regs);
                        if (regs.x.ax == 1)
                                printf("ERROR! Selected speed is out of range. Unable to set speed.\n");
                        else
                                printf("Done.\n");
                        i++;
                        break;
             
                default:
                        printf("Help:   r       - Reset IRET.\n");
                        printf("        m       - Select Microsoft Mode (2 Buttons).\n");
                        printf("        p       - Select PC Mouse Mode (3 Buttons).\n");
                        printf("        i       - Inquire Current Configuration.\n");
                        printf("        y value - Set vertical speed, where value is a multiple of 8.\n");
                        printf("        x value - Set horizontal speed, where value is a multiple of 8.\n");
                        printf("        a       - Ignore Application's setting of vert/horz speed settings.\n");
                        printf("        b       - Take Application's settings of vert/horz speed settings.\n");
        }
  
}
