#if 0
#define genuine_WIN31/
#endif

static char *ldt_buffer;
unsigned short LDT_ALIAS = 0;
int in_win31 = 0;	/* Set to 1 when running MS-Windows 3.1 */

static inline int ConvertSegmentToDescriptor(unsigned short);

static inline int win31_pre_extender(struct sigcontext_struct *scp, int intr)
{
  switch(intr) {
    case 0x21:
      switch(_HI(ax)) {
	case 0x35:	/* Get Interrupt Vector */
		_es = Interrupt_Table[_LO(bx)].selector;
		_ebx = Interrupt_Table[_LO(bx)].offset;
		return 1;
	default:
		return 0;
      }
    default:
	return 0;
  }
}

static inline void win31_post_extender(int intr)
{
  switch(intr) {
    case 0x21:
      switch(HI(ax)) {
	case 0x34:	/* Get Address of InDOS Flag */
		/* Hmmm, I don't know if it would be better to allocate only a descriptor
		   with one byte limit and offset es:bx */
		if (!(dpmi_stack_frame.es = ConvertSegmentToDescriptor(dpmi_stack_frame.es)))
		  D_printf("DPMI: can't allocate descriptor for InDOS pointer\n");
	default:
		return;
      }
    default:
	return;
  }
}

#ifdef genuine_WIN31
static inline int SetDescriptor(unsigned short, unsigned long *);

/* Simulate direct LDT write access for MS-Windows 3.1 */
static inline int win31ldt(struct sigcontext_struct *scp, unsigned char *csp)
{
  if ((_es==LDT_ALIAS) && *csp==0x26) {
    csp++;
    if ((*csp==0xc7)&&(*(csp+1)==0x04)) {   /* MOV  Word Ptr ES:[SI],#word */
      ldt_buffer[_LWORD(esi)] = *(csp+2);
      ldt_buffer[_LWORD(esi)+1] = *(csp+3);
      _eip +=5;
      SetDescriptor(_LWORD(esi), (unsigned long *)&ldt_buffer[_LWORD(esi)&0xfff8]);
      return 1;
    }
    if ((*csp==0xc7)&&(*(csp+1)==0x05)) {   /* MOV  Word Ptr ES:[DI],#word */
      ldt_buffer[_LWORD(edi)] = *(csp+2);
      ldt_buffer[_LWORD(edi)+1] = *(csp+3);
      _eip +=5;
      SetDescriptor(_LWORD(edi), (unsigned long *)&ldt_buffer[_LWORD(edi)&0xfff8]);
      return 1;
    }
    if ((*csp==0xc7)&&(*(csp+1)==0x44)) {   /* MOV  Word Ptr ES:[SI+#byte],#word */
      ldt_buffer[_LWORD(esi)+*(csp+2)] = *(csp+3);
      ldt_buffer[_LWORD(esi)+1+*(csp+2)] = *(csp+4);
      _eip +=6;
      SetDescriptor(_LWORD(esi)+*(csp+2),
				(unsigned long *)&ldt_buffer[(_LWORD(esi)+*(csp+2))&0xfff8]);
      return 1;
    }
    if ((*csp==0x89)&&(*(csp+1)==0x35)) {   /* MOV  ES:[DI],SI */
      ldt_buffer[_LWORD(esi)] = _LO(si);
      ldt_buffer[_LWORD(esi)+1] = _HI(si);
      _eip +=3;
      SetDescriptor(_LWORD(esi), (unsigned long *)&ldt_buffer[_LWORD(esi)&0xfff8]);
      return 1;
    }
  }
  return 0;
}
#endif /* genuine_WIN31 */

