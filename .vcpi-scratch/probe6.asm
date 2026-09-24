; Watch what the DOS mouse driver reports while the harness injects known
; motions: reset, show the cursor, then sample the position for 90 seconds and
; write one 8-byte record per sample (tick, x, y, buttons) to MPOS.BIN.
        org 0x100
start:
        xor ax, ax                  ; int33 AX=0000 reset
        int 0x33
        mov [cs:mres], ax
        mov [cs:mbut], bx
        mov ax, 1                   ; show cursor
        int 0x33
        mov cx, 900                 ; 900 samples
.loop:
        push cx
        mov ax, 3                   ; read position and buttons
        int 0x33
        mov di, [cs:wptr]
        mov ax, [cs:ticks]
        mov [di], ax
        mov ax, 3
        int 0x33
        mov [di+2], cx
        mov [di+4], dx
        mov [di+6], bx
        add word [cs:wptr], 8
        ; wait about 100 ms using the BIOS tick counter (18.2 Hz)
        push es
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6c]
        mov [cs:ticks], ax
.wait:
        mov bx, [es:0x6c]
        sub bx, ax
        cmp bx, 2
        jb .wait
        pop es
        pop cx
        loop .loop
        ; write it out
        mov ah, 0x3c
        xor cx, cx
        mov dx, fname
        int 0x21
        jc done
        mov bx, ax
        mov ah, 0x40
        mov cx, 900*8
        mov dx, buf
        int 0x21
        mov ah, 0x3e
        int 0x21
done:
        mov ax, 0x4c00
        int 0x21

fname:  db 'MPOS.BIN', 0
mres:   dw 0
mbut:   dw 0
ticks:  dw 0
wptr:   dw buf
buf:
