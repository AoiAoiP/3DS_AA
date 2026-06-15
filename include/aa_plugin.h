/**
 * @file   aa_plugin.h
 * @brief  3DS Anti-Aliasing Plugin - Master Header
 *
 * Central configuration and shared declarations for the Luma3DS runtime
 * anti-aliasing plugin. Defines performance budgets, screen dimensions,
 * and global state shared across all modules.
 */

#ifndef AA_PLUGIN_H
#define AA_PLUGIN_H

#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Screen / Framebuffer Constants
 *---------------------------------------------------------------------------*/

/** Top screen width in pixels (3DS top LCD) */
#define SCREEN_TOP_WIDTH    400
/** Top screen height in pixels */
#define SCREEN_TOP_HEIGHT   240
/** Total pixels in one top-screen frame */
#define SCREEN_TOP_PIXELS   (SCREEN_TOP_WIDTH * SCREEN_TOP_HEIGHT)

/** Bottom screen width in pixels (only top is processed) */
#define SCREEN_BOTTOM_WIDTH  320
/** Bottom screen height */
#define SCREEN_BOTTOM_HEIGHT 240

/*---------------------------------------------------------------------------
 * Performance Budgets (all in microseconds)
 *---------------------------------------------------------------------------*/

/** Target max processing time per frame */
#define AA_TIME_BUDGET_US   2500
/** Hard cutoff - if processing exceeds this, AA is auto-disabled */
#define AA_TIME_CUTOFF_US   4000
/** Minimum CPU frequency required to enable AA (in MHz) */
#define CPU_FREQ_MIN_MHZ    800

/*---------------------------------------------------------------------------
 * Anti-Aliasing Tunables
 *---------------------------------------------------------------------------*/

/** FXAA edge detection contrast threshold (fixed-point 8.8) */
#define FXAA_EDGE_THRESHOLD_MIN    0x0010  /* 0.0625 — minimum contrast */
#define FXAA_EDGE_THRESHOLD_MAX    0x0080  /* 0.5000 — maximum contrast */
#define FXAA_EDGE_THRESHOLD_DEF    0x0033  /* ~0.2000 — default */

/** Sub-pixel AA quality: 0=off, 1=low, 2=medium, 3=high */
#define FXAA_QUALITY_DEF    2

/** Maximum edge search steps (higher = smoother long edges, slower) */
#define FXAA_MAX_STEPS_DEF  8

/*---------------------------------------------------------------------------
 * Global Plugin State
 *---------------------------------------------------------------------------*/

typedef enum {
    AA_STATE_OFF = 0,       /**< AA fully disabled */
    AA_STATE_ENABLED,       /**< AA active, processing every frame */
    AA_STATE_THROTTLED,     /**< AA temporarily paused (thermal / battery) */
    AA_STATE_ERROR          /**< Fatal error, plugin unloaded */
} aa_state_t;

/**
 * @brief Master plugin context — single global instance.
 */
typedef struct {
    aa_state_t      state;              /**< Current plugin state */
    bool            toggle_request;     /**< User requested AA on/off toggle */
    bool            show_stats;         /**< Show performance overlay */
    bool            force_enable;       /**< Force AA even below freq threshold */

    /* Timing */
    uint64_t        last_frame_cycles;  /**< Last frame processing time (cycles) */
    uint64_t        total_cycles;       /**< Total cycles spent in AA */
    uint32_t        frame_count;        /**< Total frames processed */
    uint32_t        skipped_frames;     /**< Frames skipped (over budget) */

    /* Buffers */
    u16*            back_buffer;        /**< Double-buffer for AA processing */
    u32             back_buffer_size;   /**< Size of back buffer in bytes */
    bool            back_buffer_ready;  /**< Back buffer contains valid data */

    /* FXAA parameters (runtime adjustable) */
    uint16_t        fxaa_edge_threshold;    /**< Fixed-point 8.8 */
    uint8_t         fxaa_quality;           /**< Quality preset 0-3 */
    uint8_t         fxaa_max_steps;         /**< Max edge search steps */

    /* CPU info */
    uint16_t        cpu_freq_mhz;       /**< Current CPU frequency */
    bool            is_new_3ds;         /**< Detected New 3DS hardware */
} aa_context_t;

/** Global plugin context */
extern aa_context_t g_aa_ctx;

/*---------------------------------------------------------------------------
 * Plugin Entry / Exit (declared in main.c)
 *---------------------------------------------------------------------------*/

/**
 * @brief Luma3DS plugin entry point.
 *
 * Called by Luma's plugin loader when the game process starts.
 * Must initialize all subsystems and register frame hooks.
 */
void plugin_main(void);

/*---------------------------------------------------------------------------
 * Utility Macros
 *---------------------------------------------------------------------------*/

/** Fixed-point 8.8: convert float to fixed */
#define FLOAT_TO_FX8_8(f)   ((uint16_t)((f) * 256.0f))
/** Fixed-point 8.8: multiply two values */
#define FX8_8_MUL(a, b)     ((uint32_t)((uint32_t)(a) * (uint32_t)(b)) >> 8)
/** Fixed point 8.8: divide */
#define FX8_8_DIV(a, b)     ((uint32_t)(((uint32_t)(a) << 8) / (uint32_t)(b)))

/** RGB565 channel extraction */
#define RGB565_R(rgb)       (((rgb) >> 11) & 0x1F)
#define RGB565_G(rgb)       (((rgb) >> 5)  & 0x3F)
#define RGB565_B(rgb)       ((rgb)         & 0x1F)

/** RGB565 channel packing */
#define RGB565_PACK(r, g, b)  (((uint16_t)(r) << 11) | \
                               ((uint16_t)(g) << 5)  | \
                               ((uint16_t)(b)))

/** Luminance from RGB565 (fixed-point 8.8, ITU-R BT.601 weights) */
#define RGB565_LUMA_FX(rgb)  ( \
    ( (uint32_t)RGB565_R(rgb) * 76  +   /* 0.299 * 256 ≈ 76  */ \
      (uint32_t)RGB565_G(rgb) * 150 +   /* 0.587 * 256 ≈ 150 */ \
      (uint32_t)RGB565_B(rgb) * 29  )   /* 0.114 * 256 ≈ 29  */ \
    >> 3  /* divide by 8 to keep range in 0-4095 for 12-bit precision */ \
)

/** Clamp value x to [lo, hi] */
#define CLAMP(x, lo, hi)    (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

/** Minimum of two values */
#define MIN(a, b)           (((a) < (b)) ? (a) : (b))
/** Maximum of two values */
#define MAX(a, b)           (((a) > (b)) ? (a) : (b))
/** Absolute value */
#define ABS(x)              (((x) < 0) ? (-(x)) : (x))

#endif /* AA_PLUGIN_H */
