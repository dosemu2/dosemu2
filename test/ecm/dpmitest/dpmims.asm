
%if 0

Example of using DPMI raw-mode switching
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


%assign _AUXBUFFSIZE 8192

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

	mov word [pspdbg], cs

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
	mov word [ data.pspsel ], es
	push ds
	pop es

	mov si, msg.welcome
	call printstring

	mov word [code_sel], cs
	mov word [dssel], ds

	xor edi, edi
	mov ax, 0305h		; get raw mode-switch save state addresses
	int 31h
	jc .cannotswitch
	cmp ax, _AUXBUFFSIZE	; fits into auxbuff ?
	ja .cannotswitch	; no -->
	test ax, ax
	jz .nobuffer
	mov word [dpmi_rmsav+0], cx
	mov word [dpmi_rmsav+2], bx
	mov dword [dpmi_pmsav], edi
	mov word [dpmi_pmsav+4], si
	jmp .gotbuffer

.nobuffer:
	mov word [dpmi_rmsav+0], ..@retf
	push word [pspdbg]
	pop word [dpmi_rmsav+2]
	mov dword [dpmi_pmsav+0], ..@retf
	mov word [dpmi_pmsav+4], cs

.gotbuffer:
	xor edi, edi		; clear edih
	mov ax, 0306h		; get raw mode-switch addresses
	int 31h
	jc .cannotswitch
	mov word [dpmi_rm2pm+0], cx
	mov word [dpmi_rm2pm+2], bx
	mov dword [dpmi_pm2rm], edi
	mov word [dpmi_pm2rm+4], si

	mov al, 0
	call sr_state_pm
	call switchmode_pm2rm

	mov si, msg.rm
	call printstring

	call switchmode_rm2pm
	mov al, 1
	call sr_state_pm

	mov si, msg.pmagain
	call printstring

.cannotswitch:
	mov ax, 4C00h
	int 21h			; normal client exit (terminates DOS process too)

..@retf:
	retf


switchmode_pm2rm:
;--- raw switch:
;--- si:e/di: new cs:e/ip
;--- dx:e/bx: new ss:e/sp
;--- ax:      new ds
;--- cx:      new es
	xor ebx, ebx		; clear ebxh
	mov bx, sp
	xor edi, edi		; clear edih
	mov di, .back_after_switch

	mov ax, [pspdbg]	; switch pm -> rm
	mov si, ax
	mov dx, ax
	mov cx, ax
	jmp far dword [dpmi_pm2rm]

.back_after_switch:
	retn


switchmode_rm2pm:
	xor ebx, ebx		; clear ebxh
	mov bx, sp
	xor edi, edi		; clear edih
	mov di, .back_after_switch

	mov ax, [dssel]		; switch rm -> pm
	mov si, [code_sel]
	mov dx, ax
	mov cx, ax
	jmp far [dpmi_rm2pm]

.back_after_switch:
	retn


sr_state_rm:
	mov edi, auxbuff
	push ds
	pop es
	call far [dpmi_rmsav]
	retn


sr_state_pm:
	mov edi, auxbuff
	push ds
	pop es
	call far dword [dpmi_pmsav]
	retn


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

	align 4
data:
.pspsel:	dw 0
dssel:		dw 0
code_sel:	dw 0
pspdbg:		dw 0
dpmi_rm2pm:	dd 0
dpmi_rmsav:	dd 0
dpmi_pm2rm:	times 3 dw 0
dpmi_pmsav:	times 3 dw 0

	align 16
auxbuff:	times _AUXBUFFSIZE db 0

msg:
.nohost:	asciz "No DPMI host installed.",13,10
.nomemory:	asciz "Not enough DOS memory for child process creation or client initialization.",13,10
.initfailed:	asciz "DPMI initialization failed.",13,10
.debuginfo:	db "Protected mode breakpoint at ",_4digitshex(initsuccessful_ofs),"h.",13,10
		asciz 13,10
.welcome:	asciz "Welcome in protected mode.",13,10
.rm:		asciz "Mode-switched to real mode.",13,10
.pmagain:	asciz "In protected mode again.",13,10
