! 
! All modifications in this file to the original code are
! (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


; This is MGARROT v0.1 
;
; (c) Ed Sirett 1995.  (ed@cityscape.co.uk)
; Licensing agreements follow exactly those that apply to the 
; dosemu (Linux Dos Emulator) source.
;
; This is designed to operate with the Linux Dos Emulator.
;
; The purpose of this program is to check the values returned from the mouse
; and thus to dectect when the mouse is taking a break.
; When we are sure the mouse is taking a break we invoke a 
; dosemu internal function which will give up CPU time to the rest of the
; system for a moment (but half eternity for most CPU's). 
; 
; This program must be loaded *after* your mouse device driver is loaded.
; 

use16

.text
.bss
.data

.align 0
	.globl	_main	; just to make ld86 happy
_main:

	.org	0x100	; we have a com file


DosInt   	= 0x21;         
MouseInt 	= 0x33;
EmuInt   	= 0xe6;
PrintFun 	= 0x09;	  AH= for print line function.
PollFun  	= 0x03;        AX= for mouse get buttons and posn.
MGarrot  	= 0x28;        AX= for emu mouse helper.
InqSubFun   	= 0x01;     BX= for emu get hogthreshold.
WaitSubFun  	= 0x00;     BX= for emu wait a bit.
SaveMouse   	= 0x3533;     ES:BX is old mouse handler
SetMouse    	= 0x2533;     new mouse handler is DS:DX
ExitFailure 	= 0x4C01;	Function for exit with RC=1
FreeFun	 	= 0x4900; 	Function for deallocate memory.
TSRFun	 	= 0x3100;     Function for go TSR with DX paragraphs retained.

	br	check_start ;'jmp near check_start', Go to the initializtion section.

; Storage for the far pointer to the mouse driver.
MouseDriver:	.word 0	     ; Offset Storage
MouseDriverSeg: .word 0          ; Segment Storage
OldXPos:       .word 0           ; Saved values of X and Y mouse pos.
OldYPos:       .word 0	     ; 
Trigger:       .word 0           ; The number of times we have got consequetive 
HogValue:      .word 0           ; calls to the mouse Poll function.
MouseFunction: .word 0           ; Saved value of AX before call to MouseDriver


; This routine receives calls from the mouse interrupt (0x33)
OurMouseISR:
		seg cs
		MOV  MouseFunction,AX	 ; Save AX for future reference.
; Call the old interrupt handler (mouse driver). Now this has to be done 
; as if we were interrupting to the old handler so we must push the flags
; (they will be popped by the IRET in the handler). CLI is in force
; and will continue to be so.
		PUSHF
		seg cs
		call	far ptr * MouseDriver
; Save DS and set it to our CS
		PUSH DS    ; save the old DS 
		PUSH CS	   ; This PUSH/POP needed since no MOV DS,CS
		POP  DS    ; DS now same as CS
; Because some programs call various other functions as much as #3
; we just ignore them. 
; We are only interested in looking at the returned values if the 
; mouse function was 3 (Get Buttons and Position)
		CMP MouseFunction,#PollFun ; Is it the Polling function.
		JNE Done	; It is not so we go home.
; Firstly the mouse buttons must be all up. 
		TEST BX,bx         ; If the buttons are not all up  
		JNE Reset	; we have to start over again.
; Has the mouse moved?
		CMP OldXPos,CX ; Test the X position
		JNE Reset	  ; Start over again if it has
		CMP OldYPos,DX ; Test Y Likewise
		JNE Reset	  ;
		INC word ptr Trigger; Increment the trigger value.
		PUSH AX	           ; Save AX register
		MOV AX, HogValue; and load it with the Hogthreshold value.
		CMP AX, Trigger ; Have we reached the threshold?
		JGE TakeABreak    ; OK we have reached the value. 
		POP AX		  ; We haven't so restore AX and go home.
		JMP Done	  ;
TakeABreak: 	PUSH BX		  ; Save BX also.
		MOV BX,#WaitSubFun ; Request dosemu to  give us a break;
		MOV AX,#MGarrot    ; 
		INT EmuInt	  ; Take a nap.
		POP BX		  ; Restore saved regs.    
		POP AX		  ; 
		JMP ResetTrigger  ; Reset trigger value only.
Reset:  	MOV OldXPos,CX  ; Reset by
		MOV OldYPos,DX  ; saving current Mouse position
ResetTrigger:	MOV word ptr Trigger,#0; Start counting (again).				 			
Done:		POP DS	         ; restore the old DS.
		IRET
;---------------------------------------------------------------------------
GoTSR:		INT #DosInt;   Do the TSR call function.
EndOfTsrSection:
 
;===========================================================================
NoDosEmuMsg:   .ascii "Not running under the Linux Dos Emulator - Mouse Garrot not installed.$"
NoHogValue:    .ascii "Hogthreshold Value is 0 in dosemu.conf  - Mouse Garrot no installed.$"
InstallMsg:    .ascii "Mouse Garrot v 0.1 installed.$"

; In the Setup section we do the following: 
;  1) Check that we are running in dosemu not some other real or emulated 
;  environment. 
;  2) Obtain the hogthreshold for dosemu and check that it is >0. 
;  3) Install our mouse-interrupt handler.
;  4) Issue message.
;  5) Go TSR.
;
Start:
	MOV AX,#MGarrot;    Setup to ask dosemu what is the hogthreshold
	MOV BX,#InqSubFun;  dosemu will set *both* AX and BX to the value
	INT #EmuInt;        that way we can be sure that dosemu has responded.
; Check for dosemu.
	MOV DX, #NoDosEmuMsg
; Move message in case of failure.
	CMP AX,BX	    ; If AX<>BX then no dosemu.
	JNE RunDown
	MOV WORD PTR HogValue,AX ; Save the actual hog value.
        MOV DX, #NoHogValue  ; Move message in case of failure.
        TEST AX,ax             ; Complain if dosemu not configured with hog value.
	JZ  RunDown         ; 
; To save the old handler we can use DOS function 35
        MOV AX,#SaveMouse
        INT #DosInt            ; Now ES:BX holds the mouse driver.
        MOV MouseDriver   , BX; Save for future reference.
        MOV MouseDriverSeg, ES; 
; Now set the handler to us.
	MOV AX,#SetMouse    ; Set the handler to DS:DX (DS already correct)
	MOV DX,#OurMouseISR
	INT #DosInt	   ; 
; Time to tell the user that all is well
	MOV AH,#PrintFun
	MOV DX,#InstallMsg
	INT #DosInt;
; Deallocate Program's environment block.
	seg ds
	MOV AX, [0x2C]  ;  Apparently the environment block is stored
        MOV ES, AX        ; at this location which is now in ES.
	MOV AX, #FreeFun   ; 
	INT #DosInt	  ; 
; Now we are about to go TSR but we need to retain the TSR code.
	MOV AX, #TSRFun	  ; 
	mov	dx,#EndOfTsrSection
	add	dx,#15
	shr	dx,#4	; Number of Paras to retain.
	br	GoTSR   ; I have a suspicion that we have to go TSR from
			  ; within the memory that will be retained.
			  ; Hence this jump to within the retained part.
;============================================================================
RunDown: MOV AH, #PrintFun ; 
	 INT #DosInt;
	 MOV AX,#ExitFailure
	 INT #DosInt;

check_start:
#include "detect.h"
	JMPL	Start

