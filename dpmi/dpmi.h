/* this is for the DPMI support */
#ifndef DPMI_H
#define DPMI_H

#define DPMI_VERSION   		0x00	/* major version 0 */
#define DPMI_DRIVER_VERSION	0x5a	/* minor version 0.90 */

#define DPMI_private_paragraphs	0x0100	/* private data for DPMI server */

#define UCODESEL 0x23
#define UDATASEL 0x2b

extern u_char in_dpmi;
extern u_char in_dpmi_dos_int;
extern u_char DPMIclient_is_32;
extern us DPMI_SavedDOS_ss;
extern us DPMI_SEL;
extern us LDT_ALIAS;
extern unsigned long DPMI_SavedDOS_esp;
extern us DPMI_private_data_segment;
extern void CallToInit(unsigned long, us, unsigned long, us, us, us);
extern void dpmi_get_entry_point();
extern void dpmi_control();
extern void do_int31(int);
extern u_char win31ldt(unsigned char *);

extern void ReturnFrom_dpmi_control();

us AllocateDescriptors(int);
int FreeDescriptor(us);
us ConvertSegmentToDescriptor(us);
us GetNextSelectorIncrementValue(void);
unsigned long GetSegmentBaseAddress(us);
u_char SetSegmentBaseAddress(us, unsigned long);
u_char SetSegmentLimit(us, unsigned long);
u_char SetDescriptorAccessRights(us, us);
us CreateCSAlias(us);
u_char GetDescriptor(us, unsigned long *);
u_char SetDescriptor(us, unsigned long *);
u_char AllocateSpecificDescriptor(us);
void GetFreeMemoryInformation(unsigned long *);

typedef struct segment_descriptor_s
{
    unsigned long	base_addr;	/* Pointer to segment in flat memory */
    unsigned long	limit;		/* Limit of Segment */
    unsigned char	type;
    unsigned char	is_32;		/* one for is 32-bit Segment */
    unsigned char	is_big;		/* Granularity */
    unsigned char	used;		/* Segment in use */
} SEGDESC;

#define MAX_SELECTORS	0x1800

extern SEGDESC Segments[];

#endif /* DPMI_H */
