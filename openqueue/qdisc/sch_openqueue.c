/*
 * sch_openqueue.c	OpenQueue language implementation.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <net/pkt_sched.h>
#include <linux/fs.h>
#include <linux/ip.h>

#include "../include/sch_openqueue.h"

/* Skb container */

struct skb_container {
	unsigned long other_key; /* Key on the other tree (admission/processing) */
	struct sk_buff *skb;
	struct skb_container *next;
	struct skb_container *prev;
};

static struct kmem_cache *skb_container_cache;
static mempool_t *skb_container_mempool;

void *skb_container_alloc(gfp_t gfp_mask, void *pool_data)
{
	return kmem_cache_alloc(skb_container_cache, gfp_mask);
}

void skb_container_free(void *element, void *pool_data)
{
	kmem_cache_free(skb_container_cache, element);
}

/* Skb container list head */

struct skb_cont_list {
	struct skb_container *head;
};

static struct kmem_cache *skb_cont_list_cache;
static mempool_t *skb_cont_list_mempool;

void *skb_cont_list_alloc(gfp_t gfp_mask, void *pool_data)
{
	return kmem_cache_alloc(skb_cont_list_cache, gfp_mask);
}

void skb_cont_list_free(void *element, void *pool_data)
{
	kmem_cache_free(skb_cont_list_cache, element);
}

/* Policy container */

struct openqueue_policy_container {
	char name[TCQ_OPEN_QUEUE_POLICY_LEN + 1];
	openqueue_policy policy;
	struct openqueue_policy_container *next;
	struct openqueue_policy_container *prev;
};

static DEFINE_RWLOCK(openqueue_policy_lock);
static struct openqueue_policy_container *openqueue_policy_base = NULL;

int do_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct openqueue_priv *priv;
	struct iphdr *ip_hdr;
	unsigned long admn_key, proc_key;
	struct skb_cont_list *admn_cont_list, *proc_cont_list;
	struct skb_container *admn_container, *proc_container;

	priv = qdisc_priv(sch);
	ip_hdr = (struct iphdr *)skb_header_pointer(skb, 0, 0, NULL);
	if (NULL == ip_hdr)
		return -EINVAL;

	admn_key = ip_hdr->tos; /* We use TOS for admission priority for the time being */
	proc_key = skb->len;    /* We use packet len for processing priority for the time being */ 

	/* Add to admission queue */
	admn_cont_list = (struct skb_cont_list *)btree_lookup(&priv->admn_q, &btree_geo64, &admn_key);
	if (NULL == admn_cont_list) {
		admn_container = (struct skb_container *)mempool_alloc(skb_container_mempool, GFP_KERNEL);
		admn_container->next = admn_container;
		admn_container->prev = admn_container;
		
		admn_cont_list = (struct skb_cont_list *)mempool_alloc(skb_cont_list_mempool, GFP_KERNEL);
		admn_cont_list->head = admn_container;
		
		btree_insert(&priv->admn_q, &btree_geo64, &admn_key, (void *)admn_cont_list, GFP_KERNEL);
	} else {
		struct skb_container *head_container, *tail_container;

		head_container = admn_cont_list->head;
		tail_container = head_container->prev;

		admn_container = (struct skb_container *)mempool_alloc(skb_container_mempool, GFP_KERNEL);
		admn_container->prev = tail_container;
		tail_container->next = admn_container;
		head_container->prev = admn_container;
		admn_container->next = head_container;
	}

	admn_container->other_key = proc_key;
	admn_container->skb = skb;

	/* Add to processing queue */
	proc_cont_list = (struct skb_cont_list *)btree_lookup(&priv->proc_q, &btree_geo64, &proc_key);
	if (NULL == proc_cont_list) {
		proc_container = (struct skb_container *)mempool_alloc(skb_container_mempool, GFP_KERNEL);
		proc_container->next = proc_container;
		proc_container->prev = proc_container;

		proc_cont_list = (struct skb_cont_list *)mempool_alloc(skb_cont_list_mempool, GFP_KERNEL);
		proc_cont_list->head = proc_container;

		btree_insert(&priv->proc_q, &btree_geo64, &proc_key, (void *)proc_cont_list, GFP_KERNEL);
	} else {
		struct skb_container *head_container, *tail_container;

		head_container = proc_cont_list->head;
		tail_container = head_container->prev;

		proc_container = (struct skb_container *)mempool_alloc(skb_container_mempool, GFP_KERNEL);
		proc_container->prev = tail_container;
		tail_container->next = proc_container;
		head_container->prev = proc_container;
		proc_container->next = head_container;
	}

	proc_container->other_key = admn_key;
	proc_container->skb = skb;

	priv->admn_q_len++;
	
	return NET_XMIT_SUCCESS;
}

int do_reshape(struct sk_buff *skb, struct Qdisc *sch)
{
	struct openqueue_priv *priv;
	unsigned long admn_key;
	unsigned long proc_key;
	struct sk_buff *drop_skb;
	struct skb_cont_list *admn_cont_list, *proc_cont_list;
	struct skb_container *admn_container, *proc_container, *container;

	priv = qdisc_priv(sch);

	/* Drop oldest packet with the largest TOS value */
	/* Admission */
	admn_cont_list = (struct skb_cont_list *)btree_last(&priv->admn_q, &btree_geo64, &admn_key);
	if (NULL == admn_cont_list)
		return -EINVAL;

	admn_container = admn_cont_list->head; /* FIFO */

	proc_key = admn_container->other_key;
	drop_skb = admn_container->skb;

	if (admn_container->next == admn_container) { /* Last skb */
		btree_remove(&priv->admn_q, &btree_geo64, &admn_key);
		mempool_free(admn_cont_list, skb_cont_list_mempool);
	} else {
		struct skb_container *next_head, *tail;

		next_head = admn_container->next;
		tail = admn_container->prev;
		
		next_head->prev = tail;
		tail->next = next_head;

		admn_cont_list->head = next_head;
	}

	/* Processing */
	proc_cont_list = (struct skb_cont_list *)btree_lookup(&priv->proc_q, &btree_geo64, &proc_key);
	if (NULL == proc_cont_list) /* Not likely though */
		return -EINVAL;

	proc_container = NULL;
	container = proc_cont_list->head;
	do {
		if (container->skb == skb) {
			proc_container = container;
			break;
		}
	
		container = container->next;
	} while (container != proc_cont_list->head);

	if (NULL == proc_container)
		return -EINVAL;
	
	if (proc_container->next == proc_container) { /* Last skb */
		btree_remove(&priv->proc_q, &btree_geo64, &proc_key);
		mempool_free(proc_cont_list, skb_cont_list_mempool);
	} else {
		proc_container->next->prev = proc_container->prev;
		proc_container->prev->next = proc_container->next;

		if (proc_container == proc_cont_list->head) /* Remove head */
			proc_cont_list->head = proc_container->next;
	}

	kfree_skb(drop_skb);
	mempool_free(admn_container, skb_container_mempool);
	mempool_free(proc_container, skb_container_mempool);

	/* Enqueue new packet */
	return do_enqueue(skb, sch);
}

static int openqueue_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct openqueue_priv *priv;

	priv = qdisc_priv(sch);

	if (likely(priv->admn_policy(sch, skb) == 0))
		return do_enqueue(skb, sch);

	return do_reshape(skb, sch);
}

static struct sk_buff *openqueue_dequeue(struct Qdisc *sch)
{
	struct openqueue_priv *priv;
	unsigned long proc_key;
	unsigned long admn_key;
	struct sk_buff *skb;
	struct skb_cont_list *proc_cont_list, *admn_cont_list;
	struct skb_container *proc_container, *admn_container, *container;

	priv = qdisc_priv(sch);

	/* Dequeue the largest packet (Processing is based on pkt len for the time being) */
	/* Processing */
	proc_cont_list = (struct skb_cont_list *)btree_last(&priv->proc_q, &btree_geo64, &proc_key);
	if (NULL == proc_cont_list)
		return NULL;

	proc_container = proc_cont_list->head; /* FIFO */

	admn_key = proc_container->other_key;
	skb = proc_container->skb;

	if (proc_container->next == proc_container) { /* Last skb */
		btree_remove(&priv->proc_q, &btree_geo64, &proc_key);
		mempool_free(proc_cont_list, skb_cont_list_mempool);
	} else {
		struct skb_container *next_head, *tail;

		next_head = proc_container->next;
		tail = proc_container->prev;
		
		next_head->prev = tail;
		tail->next = next_head;

		proc_cont_list->head = next_head;
	}

	/* Admission */
	admn_cont_list = (struct skb_cont_list *)btree_lookup(&priv->admn_q, &btree_geo64, &admn_key);
	if (NULL == admn_cont_list) /* Not likely though */
		return NULL;

	admn_container = NULL;
	container = admn_cont_list->head;
	do {
		if (container->skb == skb) {
			admn_container = container;
			break;
		}
	
		container = container->next;
	} while (container != admn_cont_list->head);

	if (NULL == admn_container)
		return NULL;
	
	if (admn_container->next == admn_container) { /* Last skb */
		btree_remove(&priv->admn_q, &btree_geo64, &admn_key);
		mempool_free(admn_cont_list, skb_cont_list_mempool);
	} else {
		admn_container->next->prev = admn_container->prev;
		admn_container->prev->next = admn_container->next;

		if (admn_container == admn_cont_list->head) /* Remove head */
			admn_cont_list->head = admn_container->next;
	}

	mempool_free(proc_container, skb_container_mempool);
	mempool_free(admn_container, skb_container_mempool);

	priv->admn_q_len--;
	
	return skb;
}

static int openqueue_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct openqueue_priv *priv;

	priv = qdisc_priv(sch);
	priv->admn_q_len = 0;
	priv->admn_policy = NULL;
	if ((btree_init(&priv->admn_q) != 0) || (btree_init(&priv->proc_q) != 0))
		return -ENOMEM;

	if (opt != NULL) {
		struct tc_openqueue_qopt *ctl;
		struct openqueue_policy_container *container;

		ctl = nla_data(opt);
		if (nla_len(opt) < sizeof(*ctl))
			return -EINVAL;

		read_lock(&openqueue_policy_lock);

		container = openqueue_policy_base;
		do {
			if (strcmp(container->name, ctl->policy) == 0) {
				priv->admn_policy = container->policy;
				strncpy(priv->admn_policy_name, container->name, TCQ_OPEN_QUEUE_POLICY_LEN);
				break;
			}

			container = container->next;
		} while (container != openqueue_policy_base);

		read_unlock(&openqueue_policy_lock);
	}

	if (priv->admn_policy == NULL)
		return -EINVAL;

	/* Initialize queues */
	skb_container_cache = kmem_cache_create("skb_container_cache", sizeof(struct skb_container), 0, 
		SLAB_HWCACHE_ALIGN, NULL);
	skb_container_mempool = mempool_create(0, skb_container_alloc, skb_container_free, NULL);
        if (!skb_container_cache || !skb_container_mempool)
                return -ENOMEM;

	skb_cont_list_cache = kmem_cache_create("skb_cont_list_cache", sizeof(struct skb_cont_list), 0, 
		SLAB_HWCACHE_ALIGN, NULL);
	skb_cont_list_mempool = mempool_create(0, skb_cont_list_alloc, skb_cont_list_free, NULL);
        if (!skb_cont_list_cache || !skb_cont_list_mempool)
                return -ENOMEM;

	return 0;
}

static int openqueue_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct openqueue_priv *priv;
	struct tc_openqueue_qopt opt;
	
	priv = qdisc_priv(sch);
	strncpy(opt.policy, priv->admn_policy_name, TCQ_OPEN_QUEUE_POLICY_LEN);

	if (nla_put(skb, TCA_OPTIONS, sizeof(opt), &opt))
		goto nla_put_failure;
	return skb->len;

nla_put_failure:
	return -1;
}

struct Qdisc_ops openqueue_qdisc_ops __read_mostly = {
	.id		=	"openqueue",
	.priv_size	=	sizeof(struct openqueue_priv),
	.enqueue	=	openqueue_enqueue,
	.dequeue	=	openqueue_dequeue,
	.peek		=	qdisc_peek_head,
	.drop		=	qdisc_queue_drop,
	.init		=	openqueue_init,
	.reset		=	qdisc_reset_queue,
	.change		=	openqueue_init,
	.dump		=	openqueue_dump,
	.owner		=	THIS_MODULE,
};
EXPORT_SYMBOL(openqueue_qdisc_ops);

static int __init openqueue_module_init(void)
{
        return register_qdisc(&openqueue_qdisc_ops);
}

static void __exit openqueue_module_exit(void)
{
	unregister_qdisc(&openqueue_qdisc_ops);
}

/* Register/unregister openqueue policies */

int register_openqueue_policy(const char* name, openqueue_policy policy)
{
	struct openqueue_policy_container *container;

	container = (struct openqueue_policy_container *)kmalloc(sizeof(struct openqueue_policy_container), GFP_KERNEL);
	strncpy(container->name, name, TCQ_OPEN_QUEUE_POLICY_LEN);
	container->policy = policy;

	write_lock(&openqueue_policy_lock);

	if (NULL == openqueue_policy_base) {
		container->next = container;
		container->prev = container;

		openqueue_policy_base = container;
	} else {
		struct openqueue_policy_container *head_container, *tail_container;

		head_container = openqueue_policy_base;
		tail_container = head_container->prev;

		container->prev = tail_container;
		tail_container->next = container;
		head_container->prev = container;
		container->next = head_container;
	}

	write_unlock(&openqueue_policy_lock);
	
	return 0;
}
EXPORT_SYMBOL(register_openqueue_policy);

void unregister_openqueue_policy(openqueue_policy policy)
{
	struct openqueue_policy_container *container;

	write_lock(&openqueue_policy_lock);

	container = openqueue_policy_base;
	do {
		if (container->policy == policy) {
			container->next->prev = container->prev;
			container->prev->next = container->next;

			if (container == openqueue_policy_base) /* Remove head */
				openqueue_policy_base = NULL;
		
			/* TODO : Invaildate policy in qdiscs */
			
			kfree(container);
			break;
		}
	
		container = container->next;
	} while (container != openqueue_policy_base);

	write_unlock(&openqueue_policy_lock);
}
EXPORT_SYMBOL(unregister_openqueue_policy);

module_init(openqueue_module_init)
module_exit(openqueue_module_exit)
MODULE_LICENSE("GPL");
