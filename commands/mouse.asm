; mouse.asm for dosemu 0.53, 8/18/94
; Just to reset int 33 away from iret!
; Now to add more stuff.... 11/1/95....
; Hopefully adding more functions as we go, to enable more mouse
; stuff... - says Alan Hourihane ... Patience !

cseg    segment para public 'code'
org     100h
exec    proc far

	assume cs:cseg,ds:cseg,ss:nothing,es:nothing

main:   lea dx, mousehi
	mov ah, 9
	int 21h

	mov al, 33h             ; Check for InternalDriver presence
	mov bl, 0ffh              ; Fake Function call
	int 0e6h

	cmp ax, 0ffffh          ; Is internaldriver set ?
	je brkout               ; Nope, then let's get out of here...
 
p000:   mov ah,30h              ; get dos version
	int 21h
	cmp al,0                ; is al zero?
	jnz p010                ; no - it's dos 2.00 or later
	lea dx, baddos
	call prtstr             ; print message
	jmp pend                ; terminate

p010:                           ; check command line
	mov si,80h              ; get command line
p015:                           ; restart entry point
	mov al,[si]
	cmp al,0                ; anything there?
	jnz p020                ; yes
	lea dx, nocomm          ; no
	call prtstr             ; print message
	jmp pend                ; terminate
	
p020:                           ; copy command line
	mov ch,0
	mov cl,al
	inc si                  ; check next char
	mov al,[si]
	cmp al,' '              ; is it a leading space?
	jnz p021                ; no
	inc si                  ; yes - ignore it
	dec cl
       
p021:   
	mov al,[si]             ; get char
       
; Let this happen when we can use 'sensibly' multiple args on cmd line.
;        cmp al, 0Dh             ; Are there any more chars?
;        je pend

	cmp al,'r'              ; Reset iret
	je resiret
rresiret:
	cmp al,'i'              ; Is it an inquiry 
	je inquire
rinquire:
	cmp al,'m'              ; We want Microsoft Mode
	je micmode
rmicmode:
	cmp al,'p'              ; We want PC Mouse Mode
	je pcmmode
rpcmmode:

; Again, use multiple on cmdline.
;        inc si
;        jmp p021

	jmp pend

brkout: lea dx, msebad
	mov ah, 9
	int 21h

pend:   int 20h                 ; exit

	end

prtstr  proc near               ; print message
	mov ah,9
	int 21h
	ret
	endp

inquire: lea dx, inquiry         ; inquire driver
	call prtstr
	mov al, 33h
	mov bl, 3h
	int 0e6h
	cmp bh, 10h             ; Is it Microsoft ?
	jne pcmse
	lea dx, mic
	jmp prtmse
pcmse:  lea dx, pcm
prtmse: call prtstr
	jmp rinquire

resiret: mov al, 33h
	 mov bl, 0h             ; Reset iret please.
	 int 0e6h
	 jmp rresiret

micmode: mov al, 33h
	 mov bl, 1h             ; Microsoft Mode please.
	 int 0e6h
	 jmp rmicmode

pcmmode: mov al, 33h
	 mov bl, 2h             ; PC Mouse Mode please.
	 int 0e6h
	 cmp ax, 0ffh
	 jne out1
	 lea dx, noemul
	 call prtstr
out1:    jmp rpcmmode

	; work area

prog    db 80 dup(0)            ; program name
comm    db 80 dup(0)            ; command line parameters - may be overridden
				; by fcb(s)
fcb1    db 40 dup(0)            ; fcb 1
fcb2    db 40 dup(0)            ; fcb 2

stak    dw 0                    ; save sp
	dw 0                    ; save ss

mousehi db 'Linux DOSEMU Mouse Driver.',0Dh,0Ah,'$'
msebad  db 'Mouse InternalDriver option not set',0Dh,0Ah
	 db 'Cannot install mouse handler, aborting.',0Dh,0Ah,'$'
baddos  db 'Whoops !! Install DOS v2.00 or greater. BYE !',0Dh,0Ah,'$'
noemul  db 'ERROR: Unable to set PC Mouse Mode, Check /etc/dosemu.conf for emulate3buttons.',0Dh,0Ah
	db 'e.g. mouse { ps2 /dev/mouse internaldriver emulate3buttons }',0Dh,0Ah,'$'
inquiry db 'Inquiry:',0Dh,0Ah,'           Emulation Mode:  $'
mic     db 'MicroSoft (2 Buttons)',0Dh,0Ah,'$'
pcm     db 'PC Mouse (3 Buttons)',0Dh,0Ah,'$'
nocomm  db 'Command line parameters are:',0Dh,0Ah
	db '    r - Reset iret. Put "mouse r" in your autoexec first',0Dh,0Ah
	db '    i - Inquire current configuration',0Dh,0Ah
	db '    p - Select PC Mouse Mode, 3 Button',0Dh,0Ah
	db '    m - Select Microsoft Mode, 2 Button',0Dh,0Ah
	db '$'

	; program data area

