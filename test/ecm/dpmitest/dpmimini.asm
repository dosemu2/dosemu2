
%if 0

Example of entering 32-bit protected mode using DPMI
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

	mov ax, 1687h
	int 2Fh
	or ax, ax		; DPMI host installed?
	jnz nohost
	push es			; save DPMI entry address
	push di
	or si, si		; host requires client-specific DOS memory?
	jz .nomemneeded		; no -->
	mov bx, si
	mov ah, 48h
	int 21h			; allocate memory
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

	mov ax, 4C00h
	int 21h			; normal client exit (terminates DOS process too)

		; Print a string with simple instructions. Don't use
		; pointers or instructions depending on the default operation
		; size, this is called in both 16- and 32-bit modes.
printstring.next:
	mov dl, al
	mov ah, 2
	int 21h
printstring:
	lodsb
	or al, al
	jnz .next
	retn

	align 2
data:
.pspsel:	dw 0

msg:
.nohost:	asciz "No DPMI host installed.",13,10
.nomemory:	asciz "Not enough DOS memory for child process creation or client initialization.",13,10
.initfailed:	asciz "DPMI initialization failed.",13,10
.debuginfo:	db "Protected mode breakpoint at ",_4digitshex(initsuccessful_ofs),"h.",13,10
		db "32-bit code segment breakpoint at ",_4digitshex(now32bit_ofs),"h.",13,10
		asciz 13,10
.welcome:	asciz "Welcome in 32-bit protected mode.",13,10
