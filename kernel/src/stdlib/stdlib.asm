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
section '.text' executable
 public hcf ; Halt and catch fire function.
 hcf:
  .start:
   hlt
