/*
 *  rpcmouse.c  Version 0.1
 *
 *  Copyright (c) 2000 Vojtech Pavlik
 *
 *  Based on the work of:
 *      Russel King
 *
 *  Sponsored by SuSE
 */

/*
 * Acorn RiscPC mouse driver for Linux/ARM
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */


#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/iomd.h>

#define IOMD_MOUSEBTN	0x800C4000

static short rpcmouse_lastx, rpcmouse_lasty;

static struct input_dev rpcmouse_dev = {
        evbit:  {BIT(EV_KEY) | BIT(EV_REL)},
        keybit: {BIT(BTN_LEFT) | BIT(BTN_MIDDLE) | BIT(BTN_RIGHT)},
        relbit: {BIT(REL_X) | BIT(REL_Y)},
};

static void rpcmouse_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	short x, y, dx, dy, b;

	x = (short) inl(IOMD_MOUSEX);
	y = (short) inl(IOMD_MOUSEY);
	b = (short) inl(IOMD_MOUSEBTN);

	dx = x - rpcmouse_lastx;
	dy = y - rpcmouse_lasty; 

	rpcmouse_lastx = x;
	rpcmouse_lasty = y;

	input_report_rel(&rpcmouse_dev, REL_X, dx);
	input_report_rel(&rpcmouse_dev, REL_Y, dy);

	input_report_key(&amimouse_dev, BTN_LEFT,   buttons & 0x10);
	input_report_key(&amimouse_dev, BTN_MIDDLE, buttons & 0x20);
	input_report_key(&amimouse_dev, BTN_RIGHT,  buttons & 0x40);
}

static int __init rpcmouse_init(void)
{
	rpcmouse_lastx = (short) inl(IOMD_MOUSEX);
	rpcmouse_lasty = (short) inl(IOMD_MOUSEY);

	if (request_irq(IRQ_VSYNCPULSE, rpcmouse_irq, SA_SHIRQ, "rpcmouse", NULL)) {
		printk("rpcmouse: unable to allocate VSYNC interrupt\n");
		return -1;
	}

	input_register_device(&rpcmouse_dev);
	printk(KERN_INFO "input%d: Acorn RiscPC mouse irq %d", IRQ_VSYNCPULSE);

	return 0;
}

static void __exit rpcmouse_exit(void)
{
	input_unregister_device(&rpcmouse_dev);
	free_irq(IRQ_VSYNCPULSE, NULL);
}

module_init(rpcmouse_init);
module_exit(rpcmouse_exit);
