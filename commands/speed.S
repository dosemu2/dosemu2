! ***********************************************
! File: speed.S -> speed.com
! Program for Linux Dosemu
! Written by Tim Van der Linden <timvdl@innet.be>
! Changes the HogThreshold value during execution
! ***********************************************

.text
.data
.bss

.align 0
.globl _main

_main:

        .org 0x100

        push cs
        pop ds
        mov dx, #string
        add dx, #0x100
        mov ah, #0x09
        int 0x21

        xor dx,dx
        xor cx,cx
        xor bx,bx
        xor ax,ax

start_loop:
	mov ah, #0x8
	int 0x21

	cmp al, #0xd
	je is_carriage_return

	cmp al, #0x3a
	jge start_loop

	cmp al, #0x2f
	jbe start_loop

	mov dl,al
	mov ah, #0x02
	int 0x21

	sub al, #0x30

	push ax
	mov ax,bx
	mov dx, #0x0a
	mul dx
	mov bx,ax
	pop ax
	xor ah,ah
	add bx,ax

	inc cx
	cmp cx, #0x4
	jb start_loop

HogThreshold:
        mov al, #0x12
        int 0xe6
	jmp back_to_dos

is_carriage_return:
	cmp cx, #0x0
	jne HogThreshold

not_changed:
        mov dx, #string_not_changed
        add dx, #0x100
        mov ah, #0x09
        int 0x21

back_to_dos:
        mov ah, #0x4c
        int 0x21

string:
        .ascii "Enter the new HogThreshold value (0-9999): $"
string_not_changed:
        .byte 0x0d, 0x0a
        .ascii "HogThreshold value left unchanged.$"
