/**
 * @file   timing.c
 * @brief  Performance Timing & CPU Frequency Detection
 *
 * Provides cycle-accurate timing via the ARM11 PMU (Performance Monitor
 * Unit) cycle counter. Also detects hardware capabilities (New vs Old 3DS)
 * and current CPU frequency for performance budget enforcement.
 */

#include "timing.h"

/*---------------------------------------------------------------------------
 * Module State
 *---------------------------------------------------------------------------*/

/** Cached CPU frequency (detected once at startup) */
static uint16_t g_cpu_freq_mhz = 0;
/** Cached New 3DS detection result */
static bool g_is_new_3ds = false;
/** Whether the PMU is currently enabled */
static bool g_pmu_enabled = false;

/*---------------------------------------------------------------------------
 * PMU Access Helpers
 *---------------------------------------------------------------------------*/

/**
 * Write a value to a PMU register via CP15.
 * p15, 0, <Rd>, c9, c12, <CRm>
 */
static inline void pmu_write(uint32_t reg, uint32_t value)
{
    switch (reg) {
        case 0: /* PMCR */
            __asm__ volatile ("mcr p15, 0, %0, c9, c12, 0" :: "r"(value));
            break;
        case 1: /* PMCNTENSET */
            __asm__ volatile ("mcr p15, 0, %0, c9, c12, 1" :: "r"(value));
            break;
        case 2: /* PMCNTENCLR */
            __asm__ volatile ("mcr p15, 0, %0, c9, c12, 2" :: "r"(value));
            break;
        case 3: /* PMOVSR */
            __asm__ volatile ("mcr p15, 0, %0, c9, c12, 3" :: "r"(value));
            break;
        default:
            break;
    }
}

/**
 * Read a value from a PMU register via CP15.
 */
static inline uint32_t pmu_read(uint32_t reg)
{
    uint32_t value = 0;
    switch (reg) {
        case 0: /* PMCR */
            __asm__ volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r"(value));
            break;
        case 6: /* PMCEID0 */
            __asm__ volatile ("mrc p15, 0, %0, c9, c12, 6" : "=r"(value));
            break;
        case 7: /* PMCEID1 */
            __asm__ volatile ("mrc p15, 0, %0, c9, c12, 7" : "=r"(value));
            break;
        default:
            break;
    }
    return value;
}

/*---------------------------------------------------------------------------
 * Initialization & Teardown
 *---------------------------------------------------------------------------*/

void timing_init(void)
{
    /*
     * Enable user-mode access to the PMU (PMUSERENR register).
     * This is a CP15 system control register.
     */
    uint32_t pmuserenr;
    __asm__ volatile ("mrc p15, 0, %0, c9, c14, 0" : "=r"(pmuserenr));
    pmuserenr |= 1;  /* Set EN (Enable) bit */
    __asm__ volatile ("mcr p15, 0, %0, c9, c14, 0" :: "r"(pmuserenr));

    /* Enable the PMU */
    uint32_t pmcr = pmu_read(0);
    pmcr |= PMU_PMCR_E;            /* Enable PMU */
    pmcr |= (1 << 2);               /* Reset CCNT */
    pmcr |= (1 << 1);               /* Reset event counters */
    pmu_write(0, pmcr);

    /* Enable the cycle counter */
    pmu_write(1, PMU_CCNT_EN);

    __asm__ volatile ("isb" ::: "memory");

    g_pmu_enabled = true;

    /* Detect hardware */
    g_is_new_3ds   = timing_is_new_3ds();
    g_cpu_freq_mhz = timing_detect_cpu_freq();

    /* Update global context */
    g_aa_ctx.cpu_freq_mhz = g_cpu_freq_mhz;
    g_aa_ctx.is_new_3ds   = g_is_new_3ds;
}

void timing_exit(void)
{
    if (g_pmu_enabled) {
        /* Disable PMU */
        uint32_t pmcr = pmu_read(0);
        pmcr &= ~PMU_PMCR_E;
        pmu_write(0, pmcr);
        g_pmu_enabled = false;
    }
}

/*---------------------------------------------------------------------------
 * CPU Frequency Detection
 *---------------------------------------------------------------------------*/

uint16_t timing_detect_cpu_freq(void)
{
    /*
     * New 3DS can run at 804 MHz (high-clock mode) or 268 MHz (standard).
     * Old 3DS always runs at 268 MHz.
     *
     * We detect the current frequency by reading the CFGU_SOCINFO register,
     * or by calibrating against a known timer if that's unavailable.
     */

    /* Check CFGU register (available on 3DS via libctru) */
    uint8_t is_new_3ds_flag = 0;
    bool ok = R_SUCCEEDED(CFGU_GetModelNintendo2DS(&is_new_3ds_flag));

    /* On New 3DS, check if L2 cache is enabled (indicates 804MHz mode) */
    if (ok && is_new_3ds_flag) {
        /*
         * On New 3DS, the high-clock mode is enabled by Luma3DS.
         * We check the actual clock via a calibration loop against
         * the system tick timer (SYSTICK at 268 MHz reference).
         */

        /*
         * Simple calibration: count cycles over a known time interval.
         * We use svcGetSystemTick() which ticks at a known rate.
         */
        uint64_t tick_start = svcGetSystemTick();
        uint64_t cycle_start = timing_get_cycles();

        /* Wait for ~100ms worth of ticks (100ms * 268MHz / 1000) */
        /* System tick runs at 268,123,480 Hz (New 3DS) or 268,111,856 Hz (Old 3DS) */
        while ((svcGetSystemTick() - tick_start) < 26812348) {
            /* Spin */
        }

        uint64_t cycle_end = timing_get_cycles();
        uint64_t cycles_elapsed = cycle_end - cycle_start;

        /*
         * At 268 MHz: ~26.8M cycles in 100ms
         * At 804 MHz: ~80.4M cycles in 100ms
         *
         * Check which bucket we fall into.
         */
        if (cycles_elapsed > 50000000ULL) {
            return 804;  /* New 3DS high-clock mode */
        } else {
            return 268;  /* New 3DS standard mode (or L2 disabled) */
        }
    }

    return 268;  /* Old 3DS standard frequency */
}

bool timing_is_new_3ds(void)
{
    uint8_t model = 0;
    if (R_SUCCEEDED(CFGU_GetSystemModel(&model))) {
        /*
         * Model values:
         *   0 = O3DS
         *   1 = O3DS XL
         *   2 = N3DS
         *   3 = N3DS XL
         *   4 = O2DS
         *   5 = N2DS XL
         */
        return (model >= 2 && model != 4);
    }

    /* Fallback: check kernel version */
    u32 kernel_ver = osGetKernelVersion();
    return (kernel_ver >= SYSTEM_VERSION(2, 50, 0));
}

/*---------------------------------------------------------------------------
 * Timing Statistics
 *---------------------------------------------------------------------------*/

void timing_record_sample(timing_stats_t* stats, uint64_t cycles)
{
    if (!stats) return;

    /* Update min/max */
    if (stats->sample_count == 0) {
        stats->cycles_min = cycles;
        stats->cycles_max = cycles;
    } else {
        if (cycles < stats->cycles_min) stats->cycles_min = cycles;
        if (cycles > stats->cycles_max) stats->cycles_max = cycles;
    }

    /* Accumulate */
    stats->cycles_total += cycles;
    stats->sample_count++;

    /* Store in ring buffer */
    stats->cycles_ring[stats->ring_index] = cycles;
    stats->ring_index = (stats->ring_index + 1) % TIMING_MAX_SAMPLES;
}

void timing_reset_stats(timing_stats_t* stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(timing_stats_t));
}

uint64_t timing_get_p99(const timing_stats_t* stats)
{
    if (!stats || stats->sample_count == 0) {
        return 0;
    }

    /*
     * Estimate P99 from the ring buffer.
     * Since the buffer is rolling, we need at least some samples.
     */
    uint32_t n = (stats->sample_count < TIMING_MAX_SAMPLES)
                 ? stats->sample_count
                 : TIMING_MAX_SAMPLES;

    if (n < 100) {
        /* Not enough samples for meaningful P99 — return max */
        return stats->cycles_max;
    }

    /*
     * Simple P99 estimation: find the threshold where 99% of samples
     * are below it. Since we can't sort efficiently without stdlib,
     * we use a histogram-based approach.
     */

    /* Build a histogram with 32 buckets covering [min, max] range */
    uint64_t range = stats->cycles_max - stats->cycles_min;
    if (range == 0) return stats->cycles_min;

    uint32_t buckets[32] = {0};

    for (uint32_t i = 0; i < n; i++) {
        uint64_t val = stats->cycles_ring[i];
        uint32_t bucket = (uint32_t)(((val - stats->cycles_min) * 32) / range);
        if (bucket >= 32) bucket = 31;
        buckets[bucket]++;
    }

    /* Walk buckets to find P99 threshold */
    uint32_t cumulative = 0;
    uint32_t target = (n * 99) / 100;
    uint32_t p99_bucket = 0;

    for (uint32_t i = 0; i < 32; i++) {
        cumulative += buckets[i];
        if (cumulative >= target) {
            p99_bucket = i;
            break;
        }
    }

    return stats->cycles_min + (range * (p99_bucket + 1)) / 32;
}

/*---------------------------------------------------------------------------
 * Formatting
 *---------------------------------------------------------------------------*/

int timing_format_stats(const timing_stats_t* stats,
                        uint32_t freq_mhz,
                        char* buf, size_t buf_size)
{
    if (!stats || !buf || buf_size == 0) return 0;

    uint32_t avg = (stats->sample_count > 0)
        ? (uint32_t)(stats->cycles_total / stats->sample_count)
        : 0;

    uint32_t min_us = timing_cycles_to_us(stats->cycles_min, freq_mhz);
    uint32_t avg_us = timing_cycles_to_us(avg, freq_mhz);
    uint32_t max_us = timing_cycles_to_us(stats->cycles_max, freq_mhz);
    uint32_t p99_us = timing_cycles_to_us(timing_get_p99(stats), freq_mhz);

    /*
     * Use a simple snprintf if available, otherwise manual format.
     * For the 3DS toolchain, snprintf from libctru is available.
     */
    return snprintf(buf, buf_size,
                    "min:%luus avg:%luus max:%luus p99:%luus [%lu frames]",
                    (unsigned long)min_us,
                    (unsigned long)avg_us,
                    (unsigned long)max_us,
                    (unsigned long)p99_us,
                    (unsigned long)stats->sample_count);
}
