; dump the first 32 bytes of f000: and of f800:
        org 0x100
start:
        push cs
        pop es
        mov ax, 0xf000
        mov ds, ax
        xor si, si
        mov di, buf0
        mov cx, 16
        rep movsw
        mov ax, 0xf800
        mov ds, ax
        xor si, si
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
        mov dx, buf0
        mov cx, 64
        mov ah, 0x40
        int 0x21
        mov ah, 0x3e
        int 0x21
        mov ax, 0x4c00
        int 0x21
fname   db 'PROBE4.BIN',0
buf0    times 32 db 0
buf1    times 32 db 0
