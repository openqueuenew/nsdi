/*
 * q_openqueue.c	OpenQueue.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/q_openqueue.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... <openqueue> [ policy NAME ]\n");
}

static int openqueue_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int ok=0;
	struct tc_openqueue_qopt opt;
	memset(&opt, 0, sizeof(opt));

	while (argc > 0) {
		if (strcmp(*argv, "policy") == 0) {
			NEXT_ARG();
			strncpy(opt.policy, *argv, TCQ_OPEN_QUEUE_POLICY_LEN);
			ok++;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "%s: unknown parameter \"%s\"\n", qu->id, *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	if (ok)
		addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt));
	return 0;
}

static int openqueue_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct tc_openqueue_qopt *qopt;

	if (opt == NULL)
		return 0;

	if (RTA_PAYLOAD(opt)  < sizeof(*qopt))
		return -1;
	qopt = RTA_DATA(opt);
	if (strcmp(qu->id, "openqueue") == 0)
		fprintf(f, "policy %s", qopt->policy);
	
	return 0;
}

struct qdisc_util openqueue_qdisc_util = {
	.id = "openqueue",
	.parse_qopt = openqueue_parse_opt,
	.print_qopt = openqueue_print_opt,
};
