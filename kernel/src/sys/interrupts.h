#ifndef _INTERRUPTS_H
#define _INTERRUPTS_H
#include <stdlib.h>

/**
 * @brief an entry in the interrupt descriptor table
 * 
 */
typedef struct idt_entry {
  union {
    uint16_t offset_1;
    uint16_t address_low;
  }; // offset bits 0..15
  uint16_t selector; // a code segment selector in GDT or LDT
  uint8_t
      ist; // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
  union {
    uint8_t type_attributes;
    uint8_t flags;
  }; // gate type, dpl, and p fields
  union {
    uint16_t offset_2;
    uint16_t address_mid;
  }; // offset bits 16..31
  union {
    uint16_t offset_3;
    uint16_t address_high;
  }; // offset bits 32..63
  uint32_t zero; // reserved
} idt_entry_t __attribute__((packed));

#define IDT_ENTRY(offset, _selector, flags)                                    \
  ((struct idt_entry){                                                         \
      .offset_1 = (uint16_t)((uintptr_t)(offset) & 0xFFFF),                    \
      .selector = (_selector),                                                 \
      .ist = 0,                                                                \
      .type_attributes = (flags),                                              \
      .offset_2 = (uint16_t)(((uintptr_t)(offset) >> 16) & 0xFFFF),            \
      .offset_3 = (uint16_t)(((uintptr_t)(offset) >> 32) & 0xFFFFFFFF)})
typedef struct iret_frame {
  uint64_t ip;
  uint64_t cs;
  uint64_t flags;
  uint64_t sp;
  uint64_t ss;
} __attribute__((packed));
typedef struct cpu_status_t
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector_number;
    uint64_t error_code;

    struct iret_frame iret;
} __attribute__((packed)) cpu_status_t;


// 3 - 0 	Type 	In long mode there are two types of descriptors we can
// put here: trap gates and interrupt gates. The difference is explained below.
// 4 	Reserved 	Set to zero.
// 6 - 5 	DPL 	The Descriptor Privilege Level determines the highest
// cpu ring that can trigger this interrupt via software. A default of zero is
// fine. 7 	Present 	If zero, means this descriptor is not valid and
// we don't support handling this vector. Set this bit to tell the cpu we
// support this vector, and that the handler address is valid.

#define IDT_INTERRUPT_GATE 0xE
#define IDT_TRAP_GATE 0xF
#define IDT_RING0 (0 << 5)
#define IDT_RING3 (3 << 5)
#define IDT_PRESENT (1 << 7)



typedef struct dtr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) dtr_t;

typedef void (*interrupt_handler_t)(cpu_status_t *);
void init_handlers(void);
/**
/**
 * @brief Set the idt with `lidt` instruction
 *
 * @param limit size of table in bytes
 * @param base linear address containing idt
 * @return descriptor_table_reg* pointer to idtr
 */
extern dtr_t *set_idtr(uint16_t limit, uint32_t base);
extern dtr_t *get_gdtr(void);
#endif // _INTERRUPTS_H