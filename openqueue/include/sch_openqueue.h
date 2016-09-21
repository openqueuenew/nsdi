/*
 * sch_openqueue.h  OpenQueue type declaration.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#ifndef __LINUX_SCH_OPEN_QUEUE_H
#define __LINUX_SCH_OPEN_QUEUE_H

#include <linux/btree.h>

#define TCQ_OPEN_QUEUE_POLICY_LEN 32

/* TC options*/

struct tc_openqueue_qopt {
        char	policy[TCQ_OPEN_QUEUE_POLICY_LEN + 1];  /* OPEN_QUEUE policy name */
};

/* Policy function */

struct Qdisc;
struct sk_buff;

typedef int (*openqueue_policy)(struct Qdisc *sch, struct sk_buff *skb);

/* Private data */

struct openqueue_priv {
        struct btree_head admn_q;
        struct btree_head proc_q;
        int admn_q_len;
        char admn_policy_name[TCQ_OPEN_QUEUE_POLICY_LEN + 1];
        openqueue_policy admn_policy;
};

/* Interface for register/unregister policies */

int register_openqueue_policy(const char* name, openqueue_policy policy);
void unregister_openqueue_policy(openqueue_policy policy);

#endif
