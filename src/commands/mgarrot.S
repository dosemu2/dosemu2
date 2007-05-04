# 
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


# This is MGARROT v0.1 
#
# (c) Ed Sirett 1995.  (ed@cityscape.co.uk)
# Licensing agreements follow exactly those that apply to the 
# dosemu (Linux Dos Emulator) source.
#
# This is designed to operate with the Linux Dos Emulator.
#
# The purpose of this program is to check the values returned from the mouse
# and thus to dectect when the mouse is taking a break.
# When we are sure the mouse is taking a break we invoke a 
# dosemu internal function which will give up CPU time to the rest of the
# system for a moment (but half eternity for most CPU's). 
# 
# This program must be loaded *after* your mouse device driver is loaded.
# 

#include "macros86.h"

.code16
.text
	.globl	_start16
_start16:

DosInt   	= 0x21#         
MouseInt 	= 0x33#
EmuInt   	= 0xe6#
PrintFun 	= 0x09#	  AH= for print line function.
PollFun  	= 0x03#        AX= for mouse get buttons and posn.
MGarrot  	= 0x28#        AX= for emu mouse helper.
InqSubFun   	= 0x01#     BX= for emu get hogthreshold.
WaitSubFun  	= 0x00#     BX= for emu wait a bit.
SaveMouse   	= 0x3533#     ES:BX is old mouse handler
SetMouse    	= 0x2533#     new mouse handler is DS:DX
ExitFailure 	= 0x4C01#	Function for exit with RC=1
FreeFun	 	= 0x4900# 	Function for deallocate memory.
TSRFun	 	= 0x3100#     Function for go TSR with DX paragraphs retained.

	jmp	check_start # 'jmp near check_start', Go to the initializtion section.

# Storage for the far pointer to the mouse driver.
MouseDriver:	.word 0	     # Offset Storage
MouseDriverSeg: .word 0          # Segment Storage
OldXPos:       .word 0           # Saved values of X and Y mouse pos.
OldYPos:       .word 0	     # 
Trigger:       .word 0           # The number of times we have got consequetive 
HogValue:      .word 0           # calls to the mouse Poll function.
MouseFunction: .word 0           # Saved value of AX before call to MouseDriver


# This routine receives calls from the mouse interrupt (0x33)
OurMouseISR:
		movw  %ax,%cs:MouseFunction	 # Save AX for future reference.
# Call the old interrupt handler (mouse driver). Now this has to be done 
# as if we were interrupting to the old handler so we must push the flags
# (they will be popped by the IRET in the handler). CLI is in force
# and will continue to be so.
		pushf
		lcall	INDIRECT_PTR(%cs:MouseDriver)
# Save DS and set it to our CS
		pushw %ds    # save the old DS 
		pushw %cs	   # This PUSH/POP needed since no MOV DS,CS
		popw  %ds    # DS now same as CS
# Because some programs call various other functions as much as #3
# we just ignore them. 
# We are only interested in looking at the returned values if the 
# mouse function was 3 (Get Buttons and Position)
		cmpw $PollFun,MouseFunction # Is it the Polling function.
		jne Done	# It is not so we go home.
# Firstly the mouse buttons must be all up. 
		test %bx,%bx         # If the buttons are not all up  
		jne Reset	# we have to start over again.
# Has the mouse moved?
		cmpw %cx,OldXPos # Test the X position
		jne Reset	  # Start over again if it has
		cmpw %dx,OldYPos # Test Y Likewise
		jne Reset	  #
		incw Trigger	# Increment the trigger value.
		pushw %ax	           # Save AX register
		movw HogValue, %ax# and load it with the Hogthreshold value.
		cmp Trigger,% ax# Have we reached the threshold?
		jge TakeABreak    # OK we have reached the value. 
		popw %ax		  # We haven't so restore AX and go home.
		jmp Done	  #
TakeABreak: 	pushw %bx		  # Save BX also.
		movw $WaitSubFun, %bx # Request dosemu to  give us a break#
		movw $MGarrot, %ax    # 
		int $EmuInt	  # Take a nap.
		popw %bx		  # Restore saved regs.    
		popw %ax		  # 
		jmp ResetTrigger  # Reset trigger value only.
Reset:  	movw %cx,OldXPos  # Reset by
		movw %dx,OldYPos  # saving current Mouse position
ResetTrigger:	movw $0, Trigger  # Start counting (again).				 			
Done:		popw %ds	         # restore the old DS.
		iret
#---------------------------------------------------------------------------
GoTSR:		int $DosInt#   Do the TSR call function.
EndOfTsrSection:
 
#===========================================================================
NoDosEmuMsg:   .ascii "Not running under the Linux Dos Emulator - Mouse Garrot not installed.$"
NoHogValue:    .ascii "Hogthreshold Value is 0 in dosemu.conf  - Mouse Garrot no installed.$"
InstallMsg:    .ascii "Mouse Garrot v 0.1 installed.$"

/* In the Setup section we do the following: 
#  1) Check that we are running in dosemu not some other real or emulated 
#  environment. 
#  2) Obtain the hogthreshold for dosemu and check that it is >0. 
#  3) Install our mouse-interrupt handler.
#  4) Issue message.
#  5) Go TSR.
*/
Start:
	movw $MGarrot,%ax#    Setup to ask dosemu what is the hogthreshold
	movw $InqSubFun,%bx#  dosemu will set *both* AX and BX to the value
	int $EmuInt#        that way we can be sure that dosemu has responded.
# Check for dosemu.
	movw $NoDosEmuMsg, %dx
# Move message in case of failure.
	cmp %bx,%ax	    # If AX<>BX then no dosemu.
	jne RunDown
	movw %ax, HogValue     # Save the actual hog value.
        movw $NoHogValue, %dx  # Move message in case of failure.
        test %ax,%ax           # Complain if dosemu not configured with hog value.
	jz  RunDown         # 
# To save the old handler we can use DOS function 35
        movw $SaveMouse,%ax
        int $DosInt            # Now ES:BX holds the mouse driver.
        movw %bx, MouseDriver  # Save for future reference.
        movw %es, MouseDriverSeg# 
# Now set the handler to us.
	movw $SetMouse, %ax    # Set the handler to DS:DX (DS already correct)
	movw $OurMouseISR, %dx
	int $DosInt	   # 
# Time to tell the user that all is well
	movb $PrintFun,%ah
	movw $InstallMsg,%dx
	int $DosInt#
# Deallocate Program's environment block.
	movw 0x2C,%ax    #  Apparently the environment block is stored
        movw %ax, %es        # at this location which is now in ES.
	movw $FreeFun, %ax   # 
	int $DosInt	  # 
# Now we are about to go TSR but we need to retain the TSR code.
	movw	$TSRFun, %ax	  # 
	movw	$EndOfTsrSection, %dx
	addw	$15,%dx
	shrw	$4,%dx	# Number of Paras to retain.
	jmp	GoTSR   # I have a suspicion that we have to go TSR from
			  # within the memory that will be retained.
			  # Hence this jump to within the retained part.
#============================================================================
RunDown: movb $PrintFun, %ah # 
	 int $DosInt#
	 movw $ExitFailure, %ax
	 int $DosInt#

check_start:
#include "detect.h"
	jmp	Start
