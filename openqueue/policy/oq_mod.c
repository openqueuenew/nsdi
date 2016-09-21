/*
 * oq_mod.c  OpenQueue Policy Module.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/pkt_sched.h>

#include "../include/sch_openqueue.h"

struct Qdisc;
struct sk_buff;

/* Apply policy */

int oq_mod_apply(struct Qdisc *sch, struct sk_buff *skb)
{
	struct openqueue_priv *priv;

        priv = qdisc_priv(sch);
	if (priv->admn_q_len <= 200)
		return 0;

	return -1;
}

/* Initialize policy */

static int __init oq_mod_init(void)
{
	printk(KERN_INFO "Registered openqueue policy module\n");
	
	return register_openqueue_policy("oq_mod", oq_mod_apply);
}

/* Exit policy */

static void __exit oq_mod_exit(void)
{
    printk(KERN_INFO "Unregistered openqueue policy module\n");
}

module_init(oq_mod_init);
module_exit(oq_mod_exit);
MODULE_LICENSE("GPL");
