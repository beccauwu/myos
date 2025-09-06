format ELF64

; ARGUMENT ORDER:
; rdi, rsi, rdx, rcx, r8d
; return value in rax

;   /----------------------------\
;   | Size    | Define | Reserve |
;   | (bytes) | data   | data    |
;   |=========|========|=========|
;   | 1       | db     | rb      |
;   |         | file   |         |
;   |---------|--------|---------|
;   | 2       | dw     | rw      |
;   |         | du     |         |
;   |---------|--------|---------|
;   | 4       | dd     | rd      |
;   |---------|--------|---------|
;   | 6       | dp     | rp      |
;   |         | df     | rf      |
;   |---------|--------|---------|
;   | 8       | dq     | rq      |
;   |---------|--------|---------|
;   | 10      | dt     | rt      |
;   \----------------------------/

struc dtr_t limit,base ; structure for both gdtr & idtr
{
  .limit dw limit ; length
  .base dd base   ; address
}

struc interrupt_frame n
{
  .n dq n ;
  .rax dq ;
  .rbx dq ;
  .rcx dq ;
  .
}

section '.text' executable align 16
extrn interrupt_dispatch
public set_idtr
public interrupt_stub
public get_gdtr
  set_idtr:
    mov [idtr.limit], di
    mov [idtr.base], esi
    lidt [idtr]
    lea rax, [idtr]
    ret
  get_gdtr:
    sgdt [gdtr]
    lea rax, [gdtr]
    ret
  interrupt_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    rept 8 n:8
    {
    push r#n
    }
    mov rdi, rsp
    call interrupt_dispatch
    rept 8 n:8
    {
    reverse pop r#n
    }
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iret


rept 256 n:0
{
public isr_#n
}
macro isr n
{
align 16
isr_#n:
  if n in <8,10,11,12,13,14,17>
  push QWORD n
  else
  push QWORD 0
  push QWORD n
  end if
  jmp interrupt_stub
}
rept 256 n:0 
{
isr n
}
section '.data' writeable
idtr dtr_t 0, 0
gdtr dtr_t 0, 0