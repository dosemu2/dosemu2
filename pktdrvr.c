/*
 *
 *     Packet driver emulation for the Linux DOS emulator.
 *
 *
 *     Some modifications by Jason Gorden gorden@jegnixa.hsc.missouri.edu
 *
 */

#include "pktdrvr.h"
#include "cpu.h"
#include "emu.h"
#include "dosipc.h"
#include <sys/time.h>
#include <linux/if_ether.h>

int pd_sock=0, pd_sock_inuse=0, packlen;
us pdipx_seg, *pd_size = (us *) 0xCFFFE;

void
pkt_helper(void)
{
  unsigned char *buf;
  int i;
  struct ipcpkt pkt;

  pd_printf("Int 60 called! ax=%04X\n", REG(ebx)); 
  
  switch (LO(dx))
    {
    case F_ACCESS_TYPE:
      pd_sock=OpenNetworkType(ETH_P_802_3);
      pd_printf("PD: Access type.\n");
      pd_printf("receiver is at ES:DI %04X:%04X\n", (unsigned short)REG(es),
	       (unsigned short)REG(edi));
      pdipx_seg=(us)REG(es);
      break;
    case F_GET_ADDRESS:
      pd_printf("PD: Get address.\n");
      buf=SEG_ADR((unsigned char *), es, di);
      GetDeviceHardwareAddress("eth0",buf);
      break;
    case F_SEND_PKT:
      if(pd_sock)
	{
	  buf=SEG_ADR((unsigned char *), ds, si);
	  /* Fill it in, (ther driver should do this? */
	  if (REG(ecx)<60) 
	    {
	      for (i=REG(ecx); i<60; i++)
		buf[i]=0;
	      packlen=60;
	    }
	  else packlen=REG(ecx);
	  packlen=WriteToNetwork(pd_sock, "eth0", buf, packlen);
	  pd_printf("Sent packet of len: %d on socket: %d\n",
		    packlen, pd_sock);
	  for (i=0; i<packlen; i++) pd_printf("%02X:",(u_char)buf[i]);
	  pd_printf("\n");
	  i=0;
	  while(pd_receive_packet() == -1 && i<100) i++;
	} 
      break;
    case F_RECV_PKT:
      /* receive packet */
      pd_printf("CX: %d DS:SI: %04X:%04X\n", (us) REG(ecx),(us)REG(ds),
		(us)REG(esi));
      pd_printf("CX: %d ES:DI: %04X:%04X\n", (us) REG(ecx),(us)REG(es),
		(us)REG(edi));
      pd_printf("CX i.e. size I sent receiver: %d\n",REG(ecx));
      pd_printf("SEG_ADR: %X\n", SEG_ADR((char *),es,di));
      pd_sock_inuse=0;
      break;
    }

}

int
pd_receive_packet()
{
  char device[32], *buf;
  int size, i;

  pd_printf("receive packet\n");
  
  buf=(char *) ((pdipx_seg << 4) + 0x3B75);
  size=ReadFromNetwork(pd_sock, device, buf, 1600);
  
  if (size > 1 && buf[0] == 0)  /* If not error and not broadcast */
    {
      /* This may be too late. But is should NEVER be */
      pd_sock_inuse=1;

      *pd_size=size;
      pd_printf("RECEIVED PACKET: size: %d, s=%x\n",
		size, pd_sock);
      for(i=0; i<0x30; i++) pd_printf("%02X:",(u_char)buf[i]);
      pd_printf("%s\n",&(buf[0x30]));
      do_hard_int(0x61);
    } 
  else return -1;
}
