/* SPDX-License-Identifier: (BSD-3-Clause) */
/*
 * TBM application to send and receive Xen event channels.
 *
 * Written by Stefano Stabellini
 */
#define _MINIC_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "sys.h"

#include "drivers/arm/gic.h"

#include "xen.h"

static uint16_t domid = 0;
static struct shared_info *shared_info = 0;
/* statically configured shared memory at address 0x7fe00000 */
static char* shared_mem = (char *)0x7fe00000;

static void print_event(unsigned int event)
{
    xen_printf("handle_event domid=%u event=%u\n", domid, event);
}

static void irq_handler(struct excp_frame *f)
{
    uint32_t irq;

    irq = gic_ack_irq(GIC_CPU_BASE);

    handle_event_irq(shared_info, print_event);

    gic_end_of_irq(GIC_CPU_BASE, irq);
    gic_deactivate_irq(GIC_CPU_BASE, irq);
    local_cpu_ei();
}

static void gic_init(int irq)
{
    assert(irq < 32);

    /* Disable interrupts while we configure the GIC.  */
    local_cpu_di();

    /* Setup the GIC.  */
    gicd_set_irq_group(GIC_DIST_BASE, irq, 0);
    gicd_set_irq_target(GIC_DIST_BASE, irq, 0);
    gicd_enable_irq(GIC_DIST_BASE, irq);
    gicd_set_irq_group(GIC_DIST_BASE, 5, 0);
    gicd_set_irq_target(GIC_DIST_BASE, 5, 0);
    gicd_enable_irq(GIC_DIST_BASE, 5);

    writel(GIC_DIST_BASE + GICD_CTRL, 3);
    writel(GIC_CPU_BASE + GICC_CTRL, 3);
    writel(GIC_CPU_BASE + GICC_PMR, 0xff);
    mb();
    local_cpu_ei();
}

void debug_get_domid()
{
    register uintptr_t a0 asm("x0");
    __asm__ __volatile__("hvc 0xfffd\n" 
            : "=r" (a0)
            : "0" (a0));
    domid = a0;
}

static void sleep(unsigned long long sec)
{
    unsigned long long i = 0;

    while ( i < sec*1000000000 )
    {
        i++;
        mb();
    }
}

void app_run(void)
{
    int ret = 0;

    /* Setup GIC and interrupt handler for Xen events */
    gic_init(GUEST_EVTCHN_PPI);
    aarch64_set_irq_h(irq_handler);

    /* Register shared_info page */
    shared_info = aligned_alloc(4096, 4096);
    memset(shared_info, 0x0, 4096);
    xen_register_shared_info(shared_info);

    /* Get our domid with debug hypercall */
    debug_get_domid();
    xen_printf("DEBUG domid=%d\n", domid);

    /* If domid == 1 allocate an unbound event to receive notifications */
    if (domid == 1) {
        uint16_t remote_domid = 2;
        struct evtchn_alloc_unbound alloc;

        alloc.dom = DOMID_SELF;
        alloc.remote_dom = remote_domid;
        alloc.port = 0;

        ret = xen_hypercall(EVTCHNOP_alloc_unbound, (unsigned long)&alloc,
                            0, 0, HYPERVISOR_event_channel_op);
        mb();

        xen_printf("DEBUG domid=%d alloc_unbound ret=%d port=%u\n", domid, ret, alloc.port);

        /* first message to signal readiness */
        memcpy(shared_mem, "go", sizeof("go"));
        mb();
        /* send port number to other domain */
        memcpy(shared_mem + 4, &alloc.port, sizeof(alloc.port));

    /* if domid == 2 bind to foreign event channel and send event notifications */
    } else {
        uint16_t remote_domid = 1;
        uint16_t remote_port;
        struct evtchn_bind_interdomain bind;
        struct evtchn_send send;

        /* wait for readiness signal */
        while (1) {
            if (strcmp(shared_mem, "go") == 0)
                break;
            mb();
        }
        mb();
        /* read port number of the other domain */
        memcpy(&remote_port, shared_mem + 4, sizeof(remote_port));

        xen_printf("DEBUG domid=%d remote_port=%u\n", domid, remote_port);

        bind.remote_dom = remote_domid;
        bind.remote_port = remote_port;
        bind.local_port = 0;
        ret = xen_hypercall(EVTCHNOP_bind_interdomain, (unsigned long)&bind,
                            0, 0, HYPERVISOR_event_channel_op);

        xen_printf("DEBUG domid=%d bind_interdomain ret=%d local_port=%u\n", domid, ret, bind.local_port);

        send.port = bind.local_port;
        xen_hypercall(EVTCHNOP_send, (unsigned long)&send,
                      0, 0, HYPERVISOR_event_channel_op);
        sleep(2);
        xen_hypercall(EVTCHNOP_send, (unsigned long)&send,
                      0, 0, HYPERVISOR_event_channel_op);
    }

    while (1)
        ;
}
