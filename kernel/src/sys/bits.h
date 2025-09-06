#ifndef _BITS_H
#define _BITS_H
extern void outb(uint16_t port, uint32_t val);
// Returns the value from the I/O port specified
extern uint32_t inb(uint16_t port);
#endif // _BITS_H