/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/timeout_q.h>
#include <zephyr/zephyr.h>
#include <zephyr/timeout_q.h>
#include <string.h>

#define asm __asm__
#define ssize_t size_t
#define mb dmb
#include "xen.h"

static struct _timeout timeout;
static struct evtchn_send send;
static char *shared_mem = (char *)0x7fe01000;

static void timer_expired(struct _timeout *t)
{
	uint32_t timestamp = k_cycle_get_32();

	memcpy(shared_mem + 8, &timestamp, sizeof(timestamp));
	dmb();
	xen_hypercall(EVTCHNOP_send, (unsigned long)&send,
			0, 0, HYPERVISOR_event_channel_op);

	z_add_timeout(&timeout, timer_expired, Z_TIMEOUT_MS(5000));
}

void main(void)
{
	int ret = 0;
	uint16_t remote_domid = 2;
	uint16_t remote_port;
	struct evtchn_bind_interdomain bind;

	/* wait for readiness signal */
	while (1) {
		if (strcmp(shared_mem, "go") == 0)
			break;
		dmb();
	}
	dmb();
	/* read port number of the other domain */
	memcpy(&remote_port, shared_mem + 4, sizeof(remote_port));

	bind.remote_dom = remote_domid;
	bind.remote_port = remote_port;
	bind.local_port = 0;
	ret = xen_hypercall(EVTCHNOP_bind_interdomain, (unsigned long)&bind,
			0, 0, HYPERVISOR_event_channel_op);
	dmb();
	if (ret)
		return;
	send.port = bind.local_port;

	z_add_timeout(&timeout, timer_expired, Z_TIMEOUT_MS(5000));
}
