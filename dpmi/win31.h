
#if 0
#define genuine_WIN31
#endif

#if 1
#define windebug
#endif

static char *ldt_buffer;
unsigned short LDT_ALIAS = 0;
int in_win31 = 0;	/* Set to 1 when running MS-Windows 3.1 */

static inline int ConvertSegmentToDescriptor(unsigned short);
static inline unsigned long GetSegmentBaseAddress(unsigned short);

#ifdef windebug
static inline int win_int41(struct sigcontext_struct *scp)
{
  if (_HI(ax)!=0)
    return 0;
  switch(_LO(ax)) {
	case 0x00:	/* OUTPUT CHARACTER FOR USER */
		D_printf("WIN31: OUTPUT CHARACTER FOR USER\n");
		return 1;
	case 0x01:	/* INPUT CHARACTER */
		D_printf("WIN31: INPUT CHARACTER\n");
		return 1;
	case 0x0d:	/* TASK GOING OUT */
		D_printf("WIN31: ASK GOING OUT\n");
		return 1;
	case 0x0e:	/* TASK COMING IN */
		D_printf("WIN31: TASK COMING IN\n");
		return 1;
	case 0x12:	/* OutputDebugString */
		D_printf("WIN31: OutputDebugString\n");
		D_printf("       %s\n", (char *) (GetSegmentBaseAddress(_es) + _LWORD(esi)));
		return 1;
	case 0x4f:	/* DEBUGGER INSTALLATION CHECK */
		_LWORD(eax) = 0x0f386;
		D_printf("WIN31: Windows debugger installation check\n");
		return 1;
	case 0x50:	/* DefineDebugSegment */
		D_printf("WIN31: DefineDebugSegment\n");
		D_printf("       segment number in executable (0-based): 0x%04x\n",_LWORD(ebx));
		D_printf("       selector: 0x%04x\n",_LWORD(ecx));
		D_printf("       instance handle: 0x%04x\n",_LWORD(edx));
		D_printf("       segment flags (0=code, 1=data): 0x%04x\n",_LWORD(esi));
		D_printf("       module name of owner: %s\n",
					(char *) (GetSegmentBaseAddress(_es) + _LWORD(edi)));
		return 1;
	case 0x51:	/* MOVE SEGMENT */
		D_printf("WIN31: MOVE SEGMENT\n");
		return 1;
	case 0x52:	/* FREE SEGMENT */
		D_printf("WIN31: FREE SEGMENT\n");
		D_printf("       freed selector: 0x%04x\n",_LWORD(ebx));
		return 1;
	case 0x59:	/* LOAD TASK */
		D_printf("WIN31: LOAD TASK\n");
		return 1;
	case 0x5c:	/* FREE INITIAL SEGMENT */
		D_printf("WIN31: FREE INITIAL SEGMENT\n");
		D_printf("       freed selector: 0x%04x\n",_LWORD(ebx));
		return 1;
	case 0x60:	/* END OF SEGMENT LOAD */
		D_printf("WIN31: END OF SEGMENT LOAD\n");
		return 1;
	case 0x61:	/* END OF SEGMENT DISCARD */
		D_printf("WIN31: END OF SEGMENT DISCARD\n");
		return 1;
	case 0x62:	/* APPLICATION TERMINATING */
		D_printf("WIN31: APPLICATION TERMINATING\n");
		return 1;
	case 0x63:	/* ASYNCHRONOUS STOP (Ctrl-Alt-SysReq) */
		D_printf("WIN31: ASYNCHRONOUS STOP (Ctrl-Alt-SysReq)\n");
		return 1;
	case 0x64:	/* DLL LOADED */
		D_printf("WIN31: DLL LOADED\n");
		return 1;
	case 0x65:	/* MODULE REMOVED */
		D_printf("WIN31: MODULE REMOVED\n");
		return 1;
	default:
		return 0;
  }
}
#endif /* windebug */

static inline int win31_pre_extender(struct sigcontext_struct *scp, int intr)
{
  switch(intr) {
    case 0x21:
      switch(_HI(ax)) {
	case 0x35:	/* Get Interrupt Vector */
		_es = Interrupt_Table[current_client][_LO(bx)].selector;
		_ebx = Interrupt_Table[current_client][_LO(bx)].offset;
		return 1;
	default:
		return 0;
      }
#ifdef windebug
    case 0x41:
      return win_int41(scp);
#endif /* windebug */
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
		if (!(dpmi_stack_frame[current_client].es =
			 ConvertSegmentToDescriptor(dpmi_stack_frame[current_client].es)))
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

