From popserver Mon Jan  8 23:18:36 GMT 1995
Received: from post.demon.co.uk (post.demon.co.uk [158.152.1.72]) by Fox.nstn.ns.ca (8.6.8.1/8.6.6) with SMTP id TAA17225 for <jmaclean@fox.nstn.ns.ca>; Sun, 8 Jan 1995 19:20:16 -0400
Received: from fairlite.demon.co.uk by post.demon.co.uk id aa23265;
          8 Jan 95 23:19 GMT
Received: by fairlite.demon.co.uk (Smail3.1.28.1 #1)
	id m0rR6t5-000SCSC; Sun, 8 Jan 95 23:19 GMT
Message-Id: <m0rR6t5-000SCSC@fairlite.demon.co.uk>
From: Alan Hourihane <alanh@fairlite.demon.co.uk>
Subject: MOUSE.ASM !
To: jmaclean@fox.nstn.ns.ca
Date: Sun, 8 Jan 1995 23:19:02 +0000 (GMT)
X-Mailer: ELM [version 2.4 PL23]
Content-Type: text
Content-Length: 3989      

Sorry, 

	Here it is...

	Sorry about the Control-M, Damn DOS files you see !


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
 
;;;;;

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
	 jmp rpcmmode

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
