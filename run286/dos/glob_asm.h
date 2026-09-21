__ASM(unsigned, gate_ds32) SEMIC		/* our own 32bit DS */
__ASM(unsigned, gate_stk_esp) SEMIC		/* handler stack, offset */
__ASM(unsigned, gate_stk_ss) SEMIC		/* handler stack, selector */
__ASM(unsigned, gate_index) SEMIC		/* import index of the call */
__ASM(unsigned, gate_cli_ss) SEMIC		/* program SS at the call */
__ASM(unsigned, gate_cli_esp) SEMIC		/* program ESP at the call */
__ASM(unsigned, gate_exit_code) SEMIC
__ASM_FUNC(gate_entry) SEMIC			/* the int 0x66 handler */
__ASM_FUNC(gate_stack_end) SEMIC		/* top of the handler stack */
__ASM_ARR(uint16_t, gate_gdt, 4) SEMIC		/* sgdt output */
__ASM_ARR(uint16_t, gate_idt, 4) SEMIC		/* sidt output */
__ASM(unsigned, gate_ldt_sel) SEMIC		/* sldt output */
__ASM(unsigned, gate_ldt_alias) SEMIC		/* int 2Fh AX=1688h */
__ASM(unsigned, gate_thunk_err) SEMIC		/* THUNK_16_32x refused? */
__ASM(unsigned, gate_cs32) SEMIC		/* our own 32bit CS */
__ASM(unsigned, gate_exc_ss) SEMIC		/* stack the exception came on */
__ASM(unsigned, gate_exc_esp) SEMIC
__ASM(unsigned, gate_exc_stk_esp) SEMIC		/* our exception stack */
__ASM(unsigned, gate_exc_stk_ss) SEMIC
__ASM(unsigned, int_stk_esp) SEMIC		/* the handlers' 16bit stack */
__ASM(unsigned, int_stk_ss) SEMIC
__ASM(unsigned, int_ret_sel) SEMIC		/* 16bit cs holding int_ret16 */
__ASM(unsigned, int_ds) SEMIC			/* the program's own DGROUP */
__ASM(unsigned, int_entered) SEMIC	/* how many reached the stub at all */
__ASM(unsigned, int_taken) SEMIC		/* how many have arrived */
__ASM_ARR(unsigned, int_count, 12) SEMIC		/* and per vector */
__ASM(unsigned, int_last) SEMIC			/* and where the last one did */
__ASM(unsigned, int_last_ss) SEMIC
__ASM(unsigned, int_last_esp) SEMIC
__ASM_ARR(uint16_t, int_target, 32) SEMIC	/* INT_SLOTS pairs */
__ASM_FUNC(int_stubs) SEMIC			/* one entry per hooked vector */
__ASM_FUNC(int_ret16) SEMIC
__ASM_FUNC(int_stack) SEMIC
__ASM_FUNC(int_stack_end) SEMIC
__ASM_FUNC(exc_stubs) SEMIC			/* one entry per exception */
__ASM_FUNC(exc_stack_end) SEMIC
