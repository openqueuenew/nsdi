#ifndef __LINUX_SCH_OPEN_QUEUE_H
#define __LINUX_SCH_OPEN_QUEUE_H

#define TCQ_OPEN_QUEUE_POLICY_LEN 32

struct tc_openqueue_qopt {
        char	policy[TCQ_OPEN_QUEUE_POLICY_LEN + 1];  /* OPEN_QUEUE policy name */
};

#endif
