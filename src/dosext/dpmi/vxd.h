/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

void VXD_VMM(struct sigcontext *scp);
void VXD_PageFile(struct sigcontext *scp);
void VXD_Reboot(struct sigcontext *scp);
void VXD_VDD(struct sigcontext *scp);
void VXD_VMD(struct sigcontext *scp);
void VXD_VXDLoader(struct sigcontext *scp);
void VXD_Shell(struct sigcontext *scp);
void VXD_Comm(struct sigcontext *scp);
void VXD_Timer(struct sigcontext *scp);
void VXD_ConfigMG(struct sigcontext *scp);
void VXD_Enable(struct sigcontext *scp);
void VXD_APM(struct sigcontext *scp);
void VXD_TimerAPI (struct sigcontext *scp);
