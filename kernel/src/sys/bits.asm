format ELF64

section '.text' executable align 16
public outb ; dest, val
public inb  ; src
outb:
  mov edx, edi ; destination
  mov eax, esi ; value
  out dx, eax
  xor eax, eax
  ret
inb:
  xor eax, eax
  mov dx, di
  in eax, dx
  ret