/*
 * oq_mod_2.c  OpenQueue Policy Module 2.
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

int oq_mod_2_apply(struct Qdisc *sch, struct sk_buff *skb)
{
	struct openqueue_priv *priv;

        priv = qdisc_priv(sch);
	if (priv->admn_q_len <= 50)
		return 0;

	return -1;
}

/* Initialize policy */

static int __init oq_mod_2_init(void)
{
	printk(KERN_INFO "Registered openqueue policy module 2\n");
	
	return register_openqueue_policy("policy_b", oq_mod_2_apply);
}

/* Exit policy */

static void __exit oq_mod_2_exit(void)
{
    printk(KERN_INFO "Unregistered openqueue policy module 2\n");
}

module_init(oq_mod_2_init);
module_exit(oq_mod_2_exit);
MODULE_LICENSE("GPL");
