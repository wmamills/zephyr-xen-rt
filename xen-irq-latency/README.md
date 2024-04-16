# IRQ LATENCY APP

This is a simple application to measure IRQ latency when running as a Xen guest,
based on the Arm Generic Timer Physical Timer interrupt.

## Expected Xen configuration

sched=null - NULL scheduler so that vCPUs are assigned statically to pCPUs

vwfi=native - Do not trap WFI/WFE

The application makes use of Xen HVC console (utilizing console_io hypercall)
which requires using debug version of Xen or passing "guest_loglvl=debug" to
the Xen command line.

## Build app for GICv2 board
$ west build -p -b xenvm <path to zephyr-apps/xen-irq-latency> -DDTC_OVERLAY_FILE=dom0less.overlay

## Build app for GICv3 board
$ west build -p -b xenvm_gicv3 <path to zephyr-apps/xen-irq-latency> -DDTC_OVERLAY_FILE=dom0less.overlay

## ImageBuilder config file (example for a single dom0less domU on ZCU102)

```
MEMORY_START="0x0"
MEMORY_END="0x80000000"

XEN="xen"
XEN_CMD="console=dtuart dtuart=serial0 bootscrub=0 vwfi=native sched=null"

DEVICE_TREE="system.dtb"

DOMU_KERNEL[0]="zephyr.bin"
DOMU_MEM[0]="16"

NUM_DOMUS="1"

UBOOT_SOURCE="boot.source"
UBOOT_SCRIPT="boot.scr"
```

## Results on Xilinx ZCU102

```
(d1) *** Booting Zephyr OS build zephyr-v3.5.0 ***
(d1) Arm Generic Timer IRQ latency test
(d1) Frequency: 99990008 [Hz]
(d1) latency (ns): max=8470 warm_max=0 min=8470 avg=8470
(d1) latency (ns): max=8470 warm_max=0 min=3400 avg=7837
(d1) latency (ns): max=8470 warm_max=2970 min=2970 avg=7229
(d1) latency (ns): max=8470 warm_max=2990 min=2970 avg=6700
(d1) latency (ns): max=8470 warm_max=2990 min=2910 avg=6227
(d1) latency (ns): max=8470 warm_max=2990 min=2860 avg=5807
(d1) latency (ns): max=8470 warm_max=2990 min=2860 avg=5443
(d1) latency (ns): max=8470 warm_max=2990 min=2830 avg=5117
(d1) latency (ns): max=8470 warm_max=2990 min=2830 avg=4842
(d1) latency (ns): max=8470 warm_max=2990 min=2830 avg=4596
```

## Xen modifications

Xen does not make use of the GT physical timer for itself and does not allow
guests to access it directly (it fully emulates it).

Xen does not allow to assign PPIs to guests.

The patch modifies Xen to route physical timer IRQ to itself, but in the IRQ
handler, the IRQ is instead injected to the guest.

See: xen-phys-timer.diff

Note: attempt to destroy IRQ latency guest will most likely result in a failure
