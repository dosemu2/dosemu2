/* this is for the XMS support */
#ifndef DPMI_H
#define DPMI_H

#define DPMI_VERSION   		0x00	/* version 0 */
#define DPMI_DRIVER_VERSION	0x09	/* driver version 0.09 */

extern u_char in_dpmi;
extern void dpmi_init();
extern u_char dpmi_test();
extern void leave_dpmi();

#endif /* DPMI_H */
