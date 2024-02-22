/* IRQ latency test based on Generic Timer Physical Timer */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/*
 * Zepyhyr on Arm64 makes use of Virtual Timer as a system timer, so it is
 * safe to use Physical Timer for the application.
 */
MAKE_REG_HELPER(cntpct_el0);
MAKE_REG_HELPER(cntp_ctl_el0);
MAKE_REG_HELPER(cntp_cval_el0);

#define TIMER_CTL_ENABLE  (1U << 0)
#define TIMER_CTL_IMASK   (1U << 1)
#define TIMER_CTL_ISTATUS (1U << 2)

static uint64_t counter;
static uint32_t sys_freq_hz;

static struct {
    int64_t avg;			   /* Moving average k=0.125  */
    uint64_t max_latency;
    uint64_t min_latency;
    uint64_t max_warm_latency; /* Max seen after initial warmup runs */
    uint64_t rounds;
} st = {
    .avg = 0,
    .max_latency = 0,
    .max_warm_latency = 0,
    .min_latency = -1LL,
    .rounds = 0,
};

static inline void timer_rearm(uint64_t delay)
{
    counter += delay;

    write_cntp_cval_el0(counter);
    write_cntp_ctl_el0(TIMER_CTL_ENABLE);
    barrier_isync_fence_full();
}

static inline uint64_t read_counter(void)
{
    uint64_t val;

    barrier_isync_fence_full();
    val = read_cntpct_el0();
    barrier_isync_fence_full();

    return val;
}

static void show_stats(void)
{
    printk("latency (ns): max=%llu warm_max=%llu min=%llu avg=%llu\n",
           st.max_latency, st.max_warm_latency, st.min_latency, st.avg);
}

static void update_stats(uint64_t latency)
{
    bool update = false;
    int64_t diff;
    int64_t prev_avg = st.avg;

    st.rounds++;

    if ( latency > st.max_latency )
    {
        st.max_latency = latency;
        update = true;
    }

    /* Give it 2 rounds to warm up the caches */
    if ( (st.rounds > 2) && (latency > st.max_warm_latency) )
    {
        st.max_warm_latency = latency;
        update = true;
    }

    if ( latency < st.min_latency )
    {
        st.min_latency = latency;
        update = true;
    }

    /* Moving average */
    if ( !st.avg )
    {
        st.avg = latency;
        update = true;
    }
    else
    {
        /* Moving average k=0.125 */
        diff = (int64_t)latency - (int64_t)st.avg;
        st.avg += diff / 8;

        if ( st.avg != prev_avg )
            update = true;
    }

    if ( update )
        show_stats();
}

void phys_timer_isr(const void *data)
{
    uint64_t diff;
    uint64_t now = read_counter();

    write_cntp_ctl_el0(TIMER_CTL_IMASK);
    barrier_isync_fence_full();

    diff = now - counter;
    counter = now;

    timer_rearm(sys_freq_hz / 2);

    update_stats(k_cyc_to_ns_floor64(diff));
}

int main(void)
{
    sys_freq_hz = sys_clock_hw_cycles_per_sec();

    printk("Arm Generic Timer IRQ latency test\n");

    printk("Frequency: %u [Hz]\n", sys_freq_hz);

    irq_connect_dynamic(ARM_TIMER_NON_SECURE_IRQ, ARM_TIMER_NON_SECURE_PRIO,
                        phys_timer_isr, NULL, ARM_TIMER_NON_SECURE_FLAGS);
    irq_enable(ARM_TIMER_NON_SECURE_IRQ);

    counter = read_counter();
    timer_rearm(sys_freq_hz);

    while ( 1 )
    {
        wfi();
    }
}
