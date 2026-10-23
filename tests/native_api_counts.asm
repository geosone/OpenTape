; Test-only mock manager counters; checks exact SCSI submissions and no faults.
bits 16
org 100h
start:
    mov dx,manager
    mov ax,3d00h
    int 21h
    jc failed
    mov bx,ax
    mov dx,entry
    mov cx,4
    mov ax,4402h
    int 21h
    jc failed
    mov ah,3eh
    int 21h
    push cs
    mov ax,srb
    push ax
    call far [entry]
    add sp,4
    cmp byte [srb+1],1
    jne failed
%ifndef RESET
    cmp word [srb+4],EXPECTED_CALLS
    jne failed
    cmp word [srb+6],0
    jne failed
%ifdef EXPECTED_WRITES
    cmp word [srb+8],EXPECTED_WRITES
    jne failed
    cmp word [srb+10],0              ; no implicit filemarks or flushes
    jne failed
    cmp word [srb+12],0
    jne failed
%endif
%endif
    mov dx,ok
    mov ah,9
    int 21h
    mov ax,4c00h
    int 21h
failed:
    mov dx,bad
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h
entry dd 0
%ifdef RESET
srb db 0f1h
%else
srb db 0f0h
%endif
    times 111 db 0
manager db 'SCSIMGR$',0
%ifdef RESET
ok db 'Native mock counters reset',13,10,'$'
%else
ok db 'Native exact command counts passed',13,10,'$'
%endif
bad db 'Native command counts FAILED',13,10,'$'
