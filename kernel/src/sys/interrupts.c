#include "interrupts.h"
#include <stdlib.h>
extern char isr_0[];
__attribute__((used, aligned(0x10))) static volatile struct idt_entry idt[256] = {0};

static interrupt_handler_t dummy = NULL;

/**
 * @brief Set idt entry to handler
 *
 * @param vector index in idt to set
 * @param handler idt handler
 * @param dpl privilege level
 */
void set_idt_entry(uint8_t vector, interrupt_handler_t handler, uint8_t dpl) {
  uint64_t handler_addr = (uint64_t)handler;
  idt_entry_t *entry = &idt[vector];
  entry->address_low = handler_addr & 0xFFFF;
  entry->address_mid = (handler_addr >> 16) & 0xFFFF;
  entry->address_high = (handler_addr >> 32) & 0xffffffff;
  // your code selector may be different!
  entry->selector = 0x0;
  // trap gate + present + DPL
  entry->flags = 0b1110 | ((dpl & 0b11) << 5) | (1 << 7);
  // ist disabled
  entry->ist = 0;
}

__attribute((naked)) void interrupt_dispatch(cpu_status_t *ctx) {
  uint64_t n = ctx->vector_number;
  if (n >= 32 && n <= 39)
    kinfo("pic1 interrupt %llu\n", n);
  else if (n >= 40 && n <= 47)
    kinfo("pic2 interrupt %llu\n", n);
  else
    switch (ctx->vector_number) {
  
    case 13:
      kerror("general protection fault. %llu\n", n);
      break;
    case 14:
      kerror("page fault. %llu\n", n);
      break;
    default:
      kerror("unexpected interrupt. %llu\n", ctx->vector_number);
      break;
    }
  abort();
}

void init_handlers() {
  for (size_t i = 0; i < 256; i++)
    set_idt_entry(i, (void *)((uint64_t)isr_0 + (i * 16)), 0);

  dtr_t *ret = set_idtr(sizeof(idt) - 1, (uint32_t)idt);
  kinfo("IDTR initialised to 0x%08X with length %u\n", ret->base, ret->limit);
}