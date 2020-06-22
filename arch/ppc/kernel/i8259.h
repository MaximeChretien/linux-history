/*
 * BK Id: SCCS/s.i8259.h 1.8 12/19/01 09:45:54 trini
 */

#ifndef _PPC_KERNEL_i8259_H
#define _PPC_KERNEL_i8259_H

#include "local_irq.h"

extern struct hw_interrupt_type i8259_pic;

void i8259_init(long);
int i8259_irq(void);
int i8259_poll(void);

#endif /* _PPC_KERNEL_i8259_H */
