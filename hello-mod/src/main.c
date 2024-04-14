/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>

int main(void)
{
    int i;

    for (i=0; i < 100; i++) {
	printf("Hello World! %s %d\n", CONFIG_BOARD, i);
	k_sleep(K_MSEC(5000));
    }
}
