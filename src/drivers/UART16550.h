#pragma once

void uart16550_init(void);
char uart16550_getc(void);
void uart16550_putc(char c);

struct trap_frame* uart16550_irq_handler(struct trap_frame *tf);
