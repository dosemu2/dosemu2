; dump the int 1 vector, the first 32 bytes of the segment it names,
; and 32 bytes at the handler itself
        org 0x100
start:
        xor ax, ax
        mov ds, ax
        mov ax, [4]
        mov [cs:v1off], ax
        mov ax, [6]
        mov [cs:v1seg], ax
        mov ax, [12]
        mov [cs:v3off], ax
        mov ax, [14]
        mov [cs:v3seg], ax
        push cs
        pop es
        mov ax, [cs:v1seg]
        mov ds, ax
        xor si, si
        mov di, buf0
        mov cx, 16
        rep movsw
        mov ax, [cs:v1seg]
        mov ds, ax
        mov si, [cs:v1off]
        mov di, buf1
        mov cx, 16
        rep movsw
        push cs
        pop ds
        mov dx, fname
        xor cx, cx
        mov ah, 0x3c
        int 0x21
        mov bx, ax
        mov dx, v1off
        mov cx, 72
        mov ah, 0x40
        int 0x21
        mov ah, 0x3e
        int 0x21
        mov ax, 0x4c00
        int 0x21
fname   db 'PROBE3.BIN',0
v1off   dw 0
v1seg   dw 0
v3off   dw 0
v3seg   dw 0
buf0    times 32 db 0
buf1    times 32 db 0
