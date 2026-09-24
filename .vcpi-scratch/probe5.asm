; ask XMS about the HMA: version, "HMA exists", then request it twice
        org 0x100
start:
        mov ax, 0x4300
        int 0x2f
        mov [cs:r4300], al
        cmp al, 0x80
        jne done
        mov ax, 0x4310
        int 0x2f
        mov [cs:xoff], bx
        mov [cs:xseg], es
        xor ah, ah                  ; AH=00 get version
        call far [cs:xoff]
        mov [cs:ver], ax
        mov [cs:hmaex], dx
        mov ah, 1                   ; AH=01 request HMA
        mov dx, 0xffff
        call far [cs:xoff]
        mov [cs:r1ax], ax
        mov [cs:r1bl], bl
        mov ah, 1                   ; ask again: must be refused now
        mov dx, 0xffff
        call far [cs:xoff]
        mov [cs:r2ax], ax
        mov [cs:r2bl], bl
done:
        push cs
        pop ds
        push cs
        pop es
        mov dx, fname
        xor cx, cx
        mov ah, 0x3c
        int 0x21
        mov bx, ax
        mov dx, r4300
        mov cx, 14
        mov ah, 0x40
        int 0x21
        mov ah, 0x3e
        int 0x21
        mov ax, 0x4c00
        int 0x21
fname   db 'PROBE5.BIN',0
xoff    dw 0
xseg    dw 0
r4300   db 0
pad0    db 0
ver     dw 0
hmaex   dw 0
r1ax    dw 0
r1bl    db 0
pad1    db 0
r2ax    dw 0
r2bl    db 0
pad2    db 0
