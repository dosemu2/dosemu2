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

        push cs         ! Display string
        pop ds
        mov dx, #string
        add dx, #0x100
        mov ah, #0x09
        int 0x21

        xor dx,dx       ! Clean registers
        xor cx,cx
        xor bx,bx
        xor ax,ax


read_first:
        mov ah, #0x08   ! Read the first character
        int 0x21
        mov cl,al

        mov bl, #0x0d   ! Is it a carriage return?
        cmp al,bl
        je not_changed

        mov bl, #0x3a   ! Is it >9?
        cmp al,bl
        jge read_first

        mov bl, #0x2f   ! Is it <0?
        cmp bl,al
        jge read_first

        mov dl,al       ! Display the first character
        mov ah, #0x02
        int 0x21

read_second:
        xor bx,bx
        mov ah, #0x08   ! Read the second character
        int 0x21
        mov bl,al

        mov dl, #0x0d   ! Is it a CR?
        cmp al,dl
        je is_carriage_return

back:
        mov dl, #0x3a   ! Is it >9?
        cmp bl,dl
        jge read_second

        mov dl, #0x2f   ! Is it <0?
        cmp dl,bl
        jge read_second

        mov dl,al       ! Display second character
        mov ah, #0x02
        int 0x21

value:
        sub cl, #0x30   ! Calculate the HogThreshold value
        sub bl, #0x30
        mov al,cl
        mov cl, #0xa
        mul cl
        add bx,ax


HogThreshold:
        xor ax,ax       ! Alter the HogThreshold value
        mov al, #0x12
        int 0xe6

back_to_dos:
        xor ax,ax       ! Leave the program
        mov ah, #0x4c
        int 0x21

is_carriage_return:
        mov bl,cl
        mov cl, #0x30
        jmp back

not_changed:
        mov dx, #string_not_changed
        add dx, #0x100
        mov ah, #0x09
        int 0x21
        jmp back_to_dos

string:
        .ascii "Enter the new HogThreshold value (0-99): $"
string_not_changed:
        .byte 0x0d, 0x0a
        .ascii "HogThreshold value left unchanged.$"

