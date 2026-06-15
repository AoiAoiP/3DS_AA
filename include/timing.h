/**
 * @file   timing.h
 * @brief  Performance Timing & CPU Frequency Detection
 *
 * Provides cycle-accurate timing for the AA processing pipeline using
 * the ARM11 Performance Monitor Unit (PMU) cycle counter. Also includes
 * CPU frequency detection to enforce performance guarantees.
 */

#ifndef TIMING_H
#define TIMING_H

#include "aa_plugin.h"

/*---------------------------------------------------------------------------
 * ARM11 PMU Constants
 *---------------------------------------------------------------------------*/

/** PMU enable bit (PMCR) */
#define PMU_PMCR_E      (1 << 0)
/** Cycle counter enable bit (PMCNTENSET) */
#define PMU_CCNT_EN     (1U << 31)

/** PMU registers (co-processor 15) */
#define PMU_REG_PMCR        0   /* Performance Monitor Control Register */
#define PMU_REG_PMCNTENSET  1   /* PM Interrupt Enable Set Register */
#define PMU_REG_CCNT        9   /* Cycle Counter Register */

/*---------------------------------------------------------------------------
 * Timing Statistics
 *---------------------------------------------------------------------------*/

/** Maximum number of samples to retain for statistics */
#define TIMING_MAX_SAMPLES  1024

/**
 * @brief Rolling timing statistics for performance monitoring.
 */
typedef struct {
    uint64_t    cycles_min;         /**< Minimum cycles observed */
    uint64_t    cycles_max;         /**< Maximum cycles observed */
    uint64_t    cycles_total;       /**< Running sum of cycles */
    uint32_t    sample_count;       /**< Number of samples collected */
    uint64_t    cycles_ring[TIMING_MAX_SAMPLES]; /**< Ring buffer of samples */
    uint32_t    ring_index;         /**< Current position in ring buffer */
} timing_stats_t;

/*---------------------------------------------------------------------------
 * API
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize the ARM11 PMU and enable cycle counting.
 *
 * Must be called once at plugin startup. Configures the PMU
 * to allow user-mode access to the cycle counter.
 */
void timing_init(void);

/**
 * @brief Shutdown timing subsystem (disable PMU).
 */
void timing_exit(void);

/**
 * @brief Read the current cycle counter value.
 *
 * Returns the number of CPU cycles elapsed since the PMU was enabled.
 * On New 3DS at 804 MHz: 1 cycle ≈ 1.24 ns.
 *
 * @return Current cycle counter value (64-bit).
 */
static inline uint64_t timing_get_cycles(void)
{
    uint32_t lo, hi;
    /* Read cycle counter from CP15 */
    __asm__ volatile (
        "mcr p15, 0, %[pmcr], c9, c12, 0\n"   /* Select CCNT */
        "mrc p15, 0, %[lo], c9, c13, 0\n"     /* Read low 32 bits */
        "mrc p15, 0, %[hi], c9, c13, 2\n"     /* Read high 32 bits */
        : [lo] "=r" (lo), [hi] "=r" (hi)
        : [pmcr] "r" (PMU_REG_CCNT)
    );
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Convert cycles to microseconds based on current CPU frequency.
 *
 * @param cycles   Cycle count from timing_get_cycles().
 * @param freq_mhz Current CPU frequency in MHz.
 * @return         Time in microseconds.
 */
static inline uint32_t timing_cycles_to_us(uint64_t cycles, uint32_t freq_mhz)
{
    return (uint32_t)(cycles / freq_mhz);
}

/**
 * @brief Detect the current ARM11 CPU frequency.
 *
 * Reads the CFGU register to determine if the system is running
 * at 804 MHz (New 3DS high-clock mode) or 268 MHz (standard mode).
 *
 * @return CPU frequency in MHz.
 */
uint16_t timing_detect_cpu_freq(void);

/**
 * @brief Check if this is a New 3DS (has L2 cache, 804MHz capable).
 *
 * @return true if running on New 3DS hardware.
 */
bool timing_is_new_3ds(void);

/**
 * @brief Record a timing sample.
 *
 * Adds a measurement to the running statistics. Automatically
 * updates min, max, and average.
 *
 * @param stats   Statistics accumulator.
 * @param cycles  Cycle count to record.
 */
void timing_record_sample(timing_stats_t* stats, uint64_t cycles);

/**
 * @brief Reset timing statistics to zero.
 *
 * @param stats  Statistics to reset.
 */
void timing_reset_stats(timing_stats_t* stats);

/**
 * @brief Get the 99th percentile cycle count.
 *
 * Estimates the 99th percentile from the rolling ring buffer
 * by sorting the last N samples.
 *
 * @param stats  Statistics with collected samples.
 * @return       Estimated 99th percentile in cycles.
 */
uint64_t timing_get_p99(const timing_stats_t* stats);

/**
 * @brief Format timing stats as a human-readable string.
 *
 * Writes a summary line like "min:1200 avg:1850 max:3400 p99:3200"
 * into the provided buffer.
 *
 * @param stats     Statistics to format.
 * @param freq_mhz  CPU frequency for microsecond conversion.
 * @param buf       Output buffer.
 * @param buf_size  Size of output buffer.
 * @return          Number of characters written.
 */
int timing_format_stats(const timing_stats_t* stats,
                        uint32_t freq_mhz,
                        char* buf, size_t buf_size);

#endif /* TIMING_H */
