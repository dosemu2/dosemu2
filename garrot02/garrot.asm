;   
;   Garrot, a Linux dosemu support driver, releases idle time to system.
;   Copyright (C) 1994  Thomas G. McWilliams
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation; either version 2 of the License, or
;   (at your option) any later version.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;
; author e-mail: tgm@netcom.com, thomas.mcwilliams@f615.n109.z1.fidonet.org
;

.286
TITLE garrot.asm 

PUBLIC	_begin,dvu,nice,flags,flagsend,i2dh_off,helpflg,versflg,queryflg
PUBLIC  termflg,runmsg,calmsg,lvlmsg,EOL,rezend,gvers
EXTRN _print:proc, _prnhx:proc, _getopt:proc, int_2dh:proc, _help:proc, _bogo:proc
EXTRN _gversion:proc, _terms:proc, _query:proc, _prneol:proc

DGROUP	GROUP	_TEXT
	ASSUME cs:DGROUP,ds:DGROUP,ss:DGROUP
_TEXT 	SEGMENT	WORD PUBLIC 'CODE'

KNICE equ 0ffffh
IVER  equ 2
SVER  equ IVER+32

i15h_vec:
        i15h_off	dw	0
        i15h_seg	dw	0

i16h_vec:
        i16hn_off	dw	0
        i16hn_seg	dw	0

i21h_vec:
        i21h_off	dw	0
        i21h_seg	dw	0

i28h_vec:
        i28h_off	dw      0
        i28h_seg	dw	0

i2dh_vec:
        i2dh_off	dw      0
        i2dh_seg	dw	0

vid_mode:
	vidmd_off	dw	449h
	vidmd_seg	dw	0h

vid_base:
	vidb_off	dw	0
	vidb_seg	dw	0b800h

flags:
	dvu 		db	0
	watchdog	db	0
	helpflg		db	0
	versflg		db	0
	queryflg	db	0
	termflg		db	0
	nice		dw	KNICE
flagsend:

int_28h_1 PROC FAR
	mov	al,cs:watchdog
	inc	al
	xor	ah,ah
	cmp	ax,cs:nice
	jb	nbv
        mov     ax,1680h 	 
        int     2fh 		
	xor	al,al
nbv:
	mov	cs:watchdog,al
	jmp dword ptr cs:i28h_vec   
int_28h_1 ENDP


int_16h_1 PROC FAR
	push 	es
	push	ax
	cmp	ah,0h	; trap problem call
	je	y1
	cmp	ah,10h	; trap problem call
	je	y1
	jmp	i16out
y1:
	call	chkchr	; poll for input
	jnz	chkct	; ok, key is ready, exit loop
	mov	ax,1680h  
	int	2fh	; release system
	jmp	y1

	 ;check count
chkct:	mov	al,cs:watchdog
	inc	al
i16out:	
	pop 	ax
	pop	es
	jmp dword ptr cs:i16h_vec
int_16h_1 ENDP



chkchr	PROC NEAR
	pushf
	push	cs
	push	offset c1
	mov	ah,11h		; see if keyboard is ready
	jmp dword ptr cs:i16h_vec
c1:	ret
chkchr	ENDP


int_15h_1 PROC FAR
        cmp     ax,1000h	; DV fake
	je	m1
	jmp dword ptr cs:i15h_vec   
m1:
        push    ax
        mov     ax,1680h 	; release system
        int     2fh 		
        pop     ax
        iret		
int_15h_1 ENDP

      
int_21h_ent   PROC    FAR
	cmp	ah,2ch		 ;check for call to time
	jne	i21out
	mov     ax,1680h 
        int     2fh                                 
	xor	al,al
	mov	cs:watchdog,al
	mov	ah,2ch
i21out:	jmp     dword ptr cs:i21h_vec  
int_21h_ent   ENDP

db   'Garrot, a Linux dosemu support driver, releases idle time to system',0 
db   'Copyright (C) 1994  Thomas G. McWilliams'
EOL	db	13,10,0

rezend:


newvect	PROC near
	push 	cs
	pop  	ds	
	 ;get video mode
	push	es 
	les	bx, dword ptr [vid_mode] 
	mov	al,7h
	cmp	es:[bx],al
	jne	lb1
	mov	ax,0b000h
	mov	vidb_seg,ax	
lb1:	pop	es

         ;get and save original int vector 15h 
        mov     ax,3515h
        int     21h                                
        mov     cs:i15h_off,bx	 ;store old int 15h offset 
        mov     cs:i15h_seg,es	 ;store old int 15h segment

	 ;set replacement  int 15h handler                           
        mov     dx,offset int_15h_1
        mov     ax,2515h 	
        int     21h  

	 ;save pointer to original int 16h handler
        mov     ax,3516h
        int     21h           
        mov     cs:i16hn_off,bx	 
        mov     cs:i16hn_seg,es	

	;set replacement  int 16h handler                           
        mov     dx,offset int_16h_1
        mov     ax,2516h 	
        int     21h  

	 ;save pointer to original int 21h handler
        mov     ax,3521h
        int     21h           
        mov     cs:i21h_off,bx	 ;store offset of old int 21h
        mov     cs:i21h_seg,es	 ;store segment of old int 21h

	 ;set replacement  int 21h handler                           
        mov     dx,offset int_21h_ent
        mov     ax,2521h 	
        int     21h  

	 ;save pointer to original int 28h handler
        mov     ax,3528h
        int     21h           
        mov     cs:i28h_off,bx	 
        mov     cs:i28h_seg,es	

	 ;set replacement  int 28h handler                           
        mov     dx,offset int_28h_1
        mov     ax,2528h 	
        int     21h  

	 ;save pointer to original int 2dh handler
        mov     ax,352dh
        int     21h           
        mov     cs:i2dh_off,bx	 
        mov     cs:i2dh_seg,es	

	 ;set replacement  int 2dh handler                           
        mov     dx,offset int_2dh
        mov     ax,252dh 	
        int     21h  

	ret
newvect ENDP 
        

notin:	db	'Garrot 0.2:  not currently installed',0
banner: db      'Garrot 0.2:  Linux DOSEMU support driver installed below ',0
runmsg  db	'Garrot 0.'
vmsg	db	'2:  running',0
errmsg: db	'Garrot 0.2:  error',0
calmsg:	db	'Garrot 0.2:  please wait, taking a guess . . . ',0
gvers	db	'garrot 0.2',0
lvlmsg: db	'level ',0

; below here is non-resident initialization code 

_begin: call	_getopt
	cmp	cx,0ffffh	; error code for _getopt
	je	err
	mov	al,helpflg
	cmp	al,0
	je	b1
	call	_help		; display help screen
	jmp	fin
b1:
	mov	al,versflg
	cmp	al,0
	je	b2
	call	_gversion
	jmp	fin
b2:
	mov	al,termflg
	cmp	al,0
	je	b3
	call	_terms
	jmp	fin
b3:
	mov     ax,0d300h	;check id, are we alredy installed?
        int     2dh        
        cmp     al,0ffh		 
        jne     doinit

	; already installed ...
	add	cl,30h	
	mov	vmsg,cl
	mov	bl,queryflg
	cmp	bl,0
	je	doupdate ; not a query, do update 

	call	stat
	cmp	al,0h
	je	fin	; exit ok
	jmp	fan	; exit error

doupdate:
	; update flags by passing pointer to int_2d 
	mov	ax,nice
	cmp	ax,KNICE
	jne	b8
	call	_bogo 	; try to guess a good value
b8:
	push	cs
	pop	dx
	mov	bx,offset flags
	mov	ax,0d306h	; update flags command
	int	2dh
	cmp	al,0h
	jne	err	; error

	call	stat	; no error
	jmp	short fin
err:
	push	offset errmsg
	call	_prneol	
	pop	ax

fan:	mov	ax,4c01h	 ;exit with error level 1
	jmp	short endo
fin:	mov	ax,4c00h	 ;exit with error level 0
endo:	int	21h	

doinit:	
	; check for query only
	mov	bl,queryflg
	cmp	bl,0h
	je	b6
	push	offset notin
	call	_prneol
	pop	ax
	jmp	fan	

b6:
	call newvect

        ;display title banner 
	push	offset banner
	call	_print
	pop	ax
	
	;print	location
	push	cs
	push offset rezend
	call	_prnhx
	pop	dx
	pop	dx
	mov	ax,nice
	cmp	ax,KNICE
	jne	b7
	call	_bogo 	; try to guess a good value
b7:	call	stat

	; terminate and stay resident
        mov     dx,offset rezend
	add	dx,15
	mov	cl,4h
	shr	dx,cl
	mov	ax,3100h
	int	21h

stat	PROC	near
	push	cx
	mov	ax,0d307h	; query flags
	int	2dh
	push	ax
	cmp	al,0h
	je	st1
	push	offset errmsg 
	call	_prneol	
	pop	ax	
	jmp	stend
st1:
	call	_query
stend:	pop	ax	
	pop	cx
	ret
stat	ENDP

_TEXT 	ENDS
	END 


