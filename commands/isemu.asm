;        Disassembly of file :ISEMU.COM
;
;        Define common ASCII control codes.
;
HT       EQU   9
LF       EQU   10
FF       EQU   12
CR       EQU   13
EOF      EQU   26
;
;        Define MSDOS/PCDOS functions.
;
_CONOUT  EQU   2
_OUTSTR  EQU   9
_EXIT    EQU   76
;
;      Macro to generate MSDOS/PCDOS calls.
;
MSDOS    MACRO  name
         IFDEF  &name
         MOV    AH,&name
         ENDIF
         INT    21H
         ENDM
;
Code_Seg SEGMENT PUBLIC
L0000H   EQU   00000H
L020DH   EQU   0020DH
L35E6H   EQU   035E6H
L4C00H   EQU   04C00H
Code_Seg ENDS
;
;
Code_Seg SEGMENT PUBLIC
;
         ASSUME CS:Code_Seg,DS:Code_Seg
         ORG   00100H
Start:   MOV   AX,OFFSET L35E6H  ;Get interupt vector e6         
         MSDOS
         MOV   AX,ES
         CMP   AX,OFFSET 0F000H
         JLE   Check   
NoEmu:   PUSH  CS
         POP   DS
         MOV   DX,OFFSET notfound
         MSDOS _OUTSTR           ;Dosemu not found               
         MOV   AX,OFFSET L4C00H  ;Terminate a process.
         MSDOS
Check:   MOV   AX,OFFSET L0000H
         INT   0E6H              ;DOSemu-Helper is emu.          
         CMP   AX,OFFSET 0AA55H
         JNZ   NoEmu   
         PUSH  BX
         PUSH  CS
         POP   DS
         MOV   DX,OFFSET Version
         MSDOS _OUTSTR           ;Display Version of DOSemu      
         POP   AX
         PUSH  AX
         MOV   AL,AH
         CALL  DspVerNr
         MOV   DL,OFFSET 2EH
         MSDOS _CONOUT           ;Display character in (DL).
         POP   AX
         PUSH  AX
         CALL  DspVerNr
         MOV   DX,OFFSET Found
         MSDOS _OUTSTR           ;Display string at (DS:DX).
         POP   AX
         MSDOS _EXIT             ;Terminate a process.
         NOP
         NOP
         NOP
         NOP
;
DspVerNr:PUSH  AX
         SHR   AL,1
         SHR   AL,1
         SHR   AL,1
         SHR   AL,1
         AND   AL,0FH
         CBW                     ;Convert byte (AL) to word (AX).
         MOV   SI,AX
         MOV   DL,BYTE PTR [SI]+0180H
         MSDOS _CONOUT           ;Display character in (DL).
         POP   AX
         AND   AL,0FH
         MOV   SI,AX
         MOV   DL,BYTE PTR [SI]+0180H
         MSDOS _CONOUT           ;Display character in (DL).
         RET
         ADD   BYTE PTR [BX+SI],AL
         ADD   BYTE PTR [BX+SI],AL
         ADD   BYTE PTR [BX+SI],AL
         ADD   BYTE PTR [BX+SI],AL
         ADD   BYTE PTR [BX+SI],AL
         ADD   BYTE PTR [BX+SI],AL
         XOR   BYTE PTR [BX+DI],DH
         XOR   DH,BYTE PTR [BP+DI]
         XOR   AL,35H
         AAA                     ;Ascii adjust for addition.
         CMP   BYTE PTR [BX+DI],BH
         INC   CX
         INC   DX
         INC   BX
         INC   SP
         INC   BP
         INC   SI
notfound:DB    'DOSEMU not found'
         DB    CR,LF,'$'
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
L01BBH:  DB    0,0,0,0,0
Version: DB    'DOSEMU version $'
Found:   DB    ' found',CR,LF,'$',0
         DB    0
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
         DB    0,0,0,0,0,0
         DB    0,0
         DB    0,0,0,0,0,EOF,EOF
Code_Seg ENDS
         END   Start   
