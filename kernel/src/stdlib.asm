format ELF64

section '.text' executable

 public hcf ; Halt and catch fire function.
 hcf:
  .start:
   hlt
   jmp .start