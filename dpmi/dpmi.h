/* this is for the DPMI support */
#ifndef DPMI_H
#define DPMI_H

#define DPMI_VERSION   		0x00	/* version 0 */
#define DPMI_DRIVER_VERSION	0x09	/* driver version 0.09 */

#define DPMI_private_paragraphs	0x0100	/* private data for DPMI server */

#define UCODESEL 0x23

extern u_char in_dpmi;
extern u_char in_dpmi_dos_int;
extern u_char DPMIclient_is_32;
extern us DPMI_SavedDOS_ss;
extern unsigned long DPMI_SavedDOS_esp;
extern void dpmi_get_entry_point();
extern void dpmi_control();
extern unsigned long get_base_addr(int);

extern void ReturnFrom_dpmi_control();

#endif /* DPMI_H */
