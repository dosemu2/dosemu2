
%if 0

Example of a DPMI fake child process returning
Public Domain

%endif

;%include "CMMACROS.MAC"
%define _4digitshex(h)	((((h)/1000h)% 10h)+'0' +(('A'-'9'-1)*((((h)/1000h)% 10h)/0Ah))), \
			((((h)/100h)% 10h)+'0' +(('A'-'9'-1)*((((h)/100h)% 10h)/0Ah))), \
			((((h)/10h)% 10h)+'0' +(('A'-'9'-1)*((((h)/10h)% 10h)/0Ah))), \
			(((h)% 10h)+'0' +(('A'-'9'-1)*(((h)% 10h)/0Ah)))
		; ASCIZ string
		;
		; %1+ = Optional string
	%imacro asciz 0-1+.nolist
%if %0 >= 1
		db %1
%endif
		db 0
	%endmacro


	cpu 386
	org 100h
	bits 16
start:
	pop ax			; get word saved on stack for COM files
	mov bx, sp
	shr bx, 4
	jnz .smallstack
	mov bx, 1000h		; it was a full 64 KiB stack
.smallstack:
	mov ah, 4Ah		; free unused memory
	int 21h
	xor ax, ax
	xchg ax, word [2Ch]
	mov es, ax
	mov ah, 49h
	int 21h			; free environment if any

	mov bx, 80h >> 4
	mov ah, 48h
	int 21h			; allocate memory for child process
	jc nomemory

		; ax:0-> DOS memory allocated for child process
	mov es, ax
	xor di, di		; es:di-> 
	xor si, si		; cs:si = ds:si -> our PSP
	mov cx, 128 >> 1	; PSP size
	rep movsw		; copy process segment prefix into newly allocated block
	dec ax
	mov ds, ax		; ds = MCB of allocated block
	inc ax			; ax = allocated block!
	mov word [ 8 ], "DP"
	mov word [ 8+2 ], "MI"
	mov word [ 8+4 ], "PS"
	mov word [ 8+6 ], "PC"	; Force MCB string to "DPMIPSPC"
	; mov word [ 1 ], ax	; Set owner to itself
	; (leave owner as us)

	mov es, ax		; es = new PSP
	mov dx, cs
	mov ds, dx		; ds = old PSP
	mov di, 18h
	mov cx, 20
	mov word [ es:32h ], cx
	mov word [ es:34h ], di
	mov word [ es:34h+2 ], ax	; fix the new PSP's PHT pointer
	mov word [ es:0Ah ], childterminated
	mov word [ es:0Ah+2 ], dx	; set termination address
	mov word [ es:16h ], dx	; set parent PSP to current one

				; The stack address set on the child's termination
				; is that of the last Int21 call to a usual function
				; (such as Int21.48 or .45) from the parent process.
				; All registers will be lost though, because we don't
				; build a fake stack frame.

	push di
	mov al, -1
	rep stosb		; initialize new PHT with empty entries
	pop di
	mov cx, word [ 32h ]	; = number of current process handles
	jcxz .phtdone		; (weird, but handle this correctly)
	cmp cx, 20
	jbe .normalpht
	mov cx, 20
.normalpht:
	lds si, [ 34h ]		; -> current process handle table
	xor bx, bx
.phtloop:
	mov dl, -1
	cmp byte [si+bx], dl	; source PHT handle used?
	je .phtnext		; no -->
	mov ah, 45h
	int 21h			; DUP
	jc nohandle
	xchg ax, bx
	xchg dl, byte [si+bx]	; get DUPed handle and kill PHT entry
	xchg ax, bx
	mov byte [es:di+bx], dl	; write into PHT of new process
.phtnext:
	inc bx			; next handle
	loop .phtloop
.phtdone:
	push cs
	pop ds

	mov bx, es
	mov ah, 50h
	int 21h			; set current PSP to new
	; mov ah, 1Ah
	; mov ds, 
	; mov dx, 80h
	; int 21h		; set DTA

		; Now executing in the child process's environment.
		; Terminating will return to label childterminated.

	mov ax, cs
	mov word [rmcallstruc0.cs], ax
	mov word [rmcallstruc0.ds], ax
	mov word [rmcallstruc0.es], ax
	mov word [rmcallstruc0.ss], ax
	pushf
	pop word [rmcallstruc0.flags]	; setup with RM values

	mov ax, 1687h
	int 2Fh
	test ax, ax		; DPMI host installed?
	jnz nohost
	push es			; save DPMI entry address
	push di
	test si, si		; host requires client-specific DOS memory?
	jz .nomemneeded		; no -->
	mov bx, si
	mov ah, 48h		; alloc DOS memory
	int 21h
	jc nomemory
	mov es, ax
.nomemneeded:
	mov si, msg.debuginfo
	call printstring
	int3
	mov bp, sp
	mov ax, 0001h		; start a 32-bit client
	call far [bp]		; initial switch to protected-mode
	jnc initsuccessful
initfailed:
	mov si, msg.initfailed
	jmp short rmerror
nohandle:
	cmp cx, 20
	je .no_handles_duplicated_yet
			; In this case, there may be some already duplicated
			;  file handles. Switch to the child process and then
			;  terminate it, in order to close the handles.
	mov word [ es:0Ah ], .half_initialised_child_terminated
				; set Parent Return Address to ours
	mov bx, es
	mov ah, 50h
	int 21h
	mov ax, 4C00h
	int 21h

.half_initialised_child_terminated:
	cld
.no_handles_duplicated_yet:
	push cs
	pop ds
	mov si, msg.nohandle
	jmp short rmerror
nohost:
	mov si, msg.nohost
	jmp short rmerror
nomemory:
	mov si, msg.nomemory
rmerror:
	call printstring
	mov ax, 4CFFh
	int 21h

initsuccessful:
initsuccessful_ofs equ (initsuccessful-$$+100h)
			; now in protected mode
	mov bx, cs
	mov cx, cs
	lar cx, cx
	shr cx, 8
	or ch, 40h		; make a 32-bit cs
	mov ax, 9
	int 31h
			; now in 32-bit PM
bits 32
now32bit:
now32bit_ofs equ (now32bit-$$+100h)
	mov word [ data.pspsel ], es
	push ds
	pop es

	mov esi, msg.welcome
	call printstring

	mov ax, 0303h
	push cs
	pop ds
	mov esi, callback	; ds:esi-> called procedure
	mov edi, rmcallstruc1	; es:edi-> (inreentrant) real mode call structure
	int 31h			; allocate RM callback
	push ss
	pop ds
	jc nocallback
	mov dword [callrm.callback], edx
	mov word [callrm.callback+2], cx

	mov ax, 0302h
	xor ebx, ebx
	xor ecx, ecx
	mov edi, rmcallstruc0
callingrm:
callingrm_ofs equ (callingrm-$$+100h)
	int 31h			; call RM procedure with interrupt stack
	jc callrmfailed

	mov ecx, dword [callrm.callback+2]
	mov edx, dword [callrm.callback]
	mov ax, 0304h
	int 31h			; free RM callback
	mov esi, msg.bye
	call printstring
	mov ax, 4C00h		; normal client exit
	int 21h

callrmfailed:
	mov esi, msg.callrmfailed
	jmp short pmerror
nocallback:
	mov esi, msg.nocallback
pmerror:
	call printstring
	movzx ecx, word [callrm.callback+2]
	movzx edx, word [callrm.callback]
	mov eax, edx
	and eax, ecx
	inc ax
	jnz .done
	mov ax, 0304h
	int 31h			; free RM callback
.done:
	mov ax, 4CFFh
	int 21h

		; This is a RM callback. Called from RM, executed in PM.
		;
		; INP:	es:edi-> RM call structure
		;	ds:esi-> RM stack
		;	ss:esp-> DPMI host internal stack
		; CHG:	all (?) except es:edi
		;	read/write RM call structure to communicate with RM code
callback:
	push dword [esi]
	pop dword [es:edi+2Ah]	; set RM cs:ip
	add word [es:edi+2Eh], byte 4	; pop 4 byte from the stack
		; The int3 trap used not to work here in the then-current
		; lDebugX development version. int 03h however did. Note that
		; both might crash the DPMI host if no debugger is installed.
	int strict byte 03h
	nop
	int3
	nop
	iret

bits 16

callrm:
	; int3			; int 03h or int3 traps (in real or V86 mode) don't cause any problems.
	int strict byte 03h
	xor ax, ax
	push ax
	push ax
	call -1:-1
.callback equ $-4
	pop ax
	pop ax
	; int3
	int strict byte 03h
	iret


		; Print a string with simple instructions. Don't use
		; pointers or instructions depending on the default operation
		; size, this is called in both 16- and 32-bit modes.
printstring.next:
	mov dl, al
	mov ah, 2
	int 21h
printstring:
	lodsb
	test al, al
	jnz .next
	retn


		; The (DPMIed) child process has terminated.
childterminated:
childterminated_ofs equ (childterminated-$$+100h)
	cld
	push cs
	pop ds
	mov ah, 4Dh
	int 21h
	mov si, msg.backsuccess
	test al, al
	jz .success
	mov si, msg.backerror
.success:
	call printstring
	mov ax, 4C00h
	int 21h			; terminate DOS process


	align 2
data:
.pspsel:	dw 0

	align 4
		; This one is used to call down to our RM part
		; from the 32-bit PM code. All values are filled
		; in either here or in our RM initialization.
rmcallstruc0:
.edi:		dd 0
.esi:		dd 0
.ebp:		dd 0
		dd 0
.ebx:		dd 0
.edx:		dd 0
.ecx:		dd 0
.eax:		dd 0
.flags:		dw 0
.es:		dw 0
.ds:		dw 0
.fs:		dw 0
.gs:		dw 0
.ip:		dw callrm
.cs:		dw 0
.sp:		dw rmcallsp
.ss:		dw 0

	align 4
		; This one is utilized by the DPMI host and
		; our RM callback when called from RM. All
		; values are filled in by the DPMI host.
rmcallstruc1:
.edi:		dd 0
.esi:		dd 0
.ebp:		dd 0
		dd 0
.ebx:		dd 0
.edx:		dd 0
.ecx:		dd 0
.eax:		dd 0
.flags:		dw 0
.es:		dw 0
.ds:		dw 0
.fs:		dw 0
.gs:		dw 0
.ip:		dw 0
.cs:		dw 0
.sp:		dw 0
.ss:		dw 0

msg:
.callrmfailed:	asciz "Calling real mode procedure failed.",13,10
.bye:		asciz "Calling real mode procedure which called callback successful.",13,10
.backsuccess:	asciz "Child process terminated okay, back in real mode.",13,10
.backerror:	asciz "Child process terminated with error, back in real mode.",13,10
.nocallback:	asciz "Could not allocate real mode callback.",13,10
.nohost:	asciz "No DPMI host installed.",13,10
.nohandle:	asciz "No free DOS file handle for child process creation.",13,10
.nomemory:	asciz "Not enough DOS memory for child process creation or client initialization.",13,10
.initfailed:	asciz "DPMI initialization failed.",13,10
.debuginfo:	db "Protected mode breakpoint at ",_4digitshex(initsuccessful_ofs),"h.",13,10
		db "32-bit code segment breakpoint at ",_4digitshex(now32bit_ofs),"h.",13,10
		db "Real mode procedure called at ",_4digitshex(callingrm_ofs),"h.",13,10
		db "Return from child process at ",_4digitshex(childterminated_ofs),"h.",13,10
		asciz 13,10
.welcome:	asciz "Welcome in 32-bit protected mode.",13,10

	align 2
rmcallsp:
