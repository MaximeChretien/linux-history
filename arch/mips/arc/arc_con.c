/*
 * Wrap-around code for a console using the
 * ARC io-routines.
 *
 * Copyright (c) 1998 Harald Koerfgen
 * Copyright (c) 2001 Ralf Baechle
 * Copyright (c) 2002 Thiemo Seufer
 */
#include <linux/tty.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <asm/sgialib.h>

static void prom_console_write(struct console *co, const char *s,
			       unsigned count)
{
	/* Do each character */
	while (count--) {
		if (*s == '\n')
			prom_putchar('\r');
		prom_putchar(*s++);
	}
}

static kdev_t prom_console_device(struct console *co)
{
	return MKDEV(TTY_MAJOR, 64 + co->index);
}

static int __init prom_console_setup(struct console *co, char *options)
{
	if (prom_flags & PROM_FLAG_USE_AS_CONSOLE)
		return 0;
	else
		return 1;
}

static struct console arc_cons = {
	name:		"ttyS",
	write:		prom_console_write,
	device:		prom_console_device,
	setup:		prom_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

/*
 *    Register console.
 */

void __init arc_console_init(void)
{
	register_console(&arc_cons);
}
