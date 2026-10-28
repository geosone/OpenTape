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
%ifndef EXPECTED_MARKS
%define EXPECTED_MARKS 0
%endif
%ifndef EXPECTED_FLUSHES
%define EXPECTED_FLUSHES 0
%endif
%endif
%ifdef EXPECTED_MARKS
    cmp word [srb+10],EXPECTED_MARKS
    jne failed
%endif
%ifdef EXPECTED_FLUSHES
    cmp word [srb+12],EXPECTED_FLUSHES
    jne failed
%endif
%ifdef EXPECTED_SETMARKS
    cmp word [srb+14],EXPECTED_SETMARKS
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
%ifndef RESET
    mov dx,actual
    mov ah,9
    int 21h
    mov ax,[srb+4]
    call hexword
    mov dx,expected
    mov ah,9
    int 21h
    mov ax,EXPECTED_CALLS
    call hexword
    mov dx,bad_count
    mov ah,9
    int 21h
    mov ax,[srb+6]
    call hexword
    mov dx,newline
    mov ah,9
    int 21h
%endif
    mov ax,4c01h
    int 21h
%ifndef RESET
; Diagnostics only; never issue a second manager request to measure failure.
hexword:
    push ax
    push bx
    push cx
    push dx
    mov bx,ax
    mov cx,4
.next:
    rol bx,4
    mov dx,bx
    and dl,0fh
    add dl,'0'
    cmp dl,'9'
    jbe .digit
    add dl,7
.digit:
    mov ah,2
    int 21h
    loop .next
    pop dx
    pop cx
    pop bx
    pop ax
    ret
actual db 'Calls actual=0x','$'
expected db ' expected=0x','$'
bad_count db ' faults=0x','$'
newline db 13,10,'$'
%endif
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
