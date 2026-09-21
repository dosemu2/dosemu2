__ASM(unsigned, gate_ds32) SEMIC		/* our own 32bit DS */
__ASM(unsigned, gate_stk_esp) SEMIC		/* handler stack, offset */
__ASM(unsigned, gate_stk_ss) SEMIC		/* handler stack, selector */
__ASM(unsigned, gate_index) SEMIC		/* import index of the call */
__ASM(unsigned, gate_cli_ss) SEMIC		/* program SS at the call */
__ASM(unsigned, gate_cli_esp) SEMIC		/* program ESP at the call */
__ASM(unsigned, gate_exit_code) SEMIC
__ASM_FUNC(gate_entry) SEMIC			/* the int 0x66 handler */
__ASM_FUNC(gate_stack_end) SEMIC		/* top of the handler stack */
