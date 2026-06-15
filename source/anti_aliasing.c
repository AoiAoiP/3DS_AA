/**
 * @file   anti_aliasing.c
 * @brief  FXAA 3.11 Subset - Fixed-Point Implementation for ARM11
 *
 * Core anti-aliasing pass. This is a streamlined, integer-only
 * implementation of FXAA 3.11's algorithm:
 *
 * Phase 1: Luminance extraction (via LUT in edge_detect.c)
 * Phase 2: Local contrast check — is this pixel on an edge?
 * Phase 3: Edge direction determination (horizontal or vertical)
 * Phase 4: Endpoint search along the edge
 * Phase 5: Bilinear blend along the edge direction
 *
 * Key FXAA Parameters (from NVIDIA FXAA 3.11 whitepaper):
 *  - EDGE_THRESHOLD: minimum contrast to consider an edge
 *  - EDGE_THRESHOLD_MIN: floor for dark-area edge detection
 *  - SUBPIXEL_QUALITY: sub-pixel AA quality (0.75 default)
 *  - MAX_SEARCH_STEPS: how far to search for edge endpoints
 */

#include "anti_aliasing.h"

/*---------------------------------------------------------------------------
 * Quality Presets
 *---------------------------------------------------------------------------*/

const fxaa_quality_t FXAA_QUALITY_PRESETS[4] = {
    /* Q0: Fastest — minimal search, no subpixel AA */
    { .max_steps = 4,  .subpixel_samples = 0, .early_out = true  },
    /* Q1: Low — moderate search */
    { .max_steps = 8,  .subpixel_samples = 0, .early_out = true  },
    /* Q2: Medium — full search, no subpixel */
    { .max_steps = 12, .subpixel_samples = 0, .early_out = false },
    /* Q3: High — full search with subpixel AA */
    { .max_steps = 16, .subpixel_samples = 1, .early_out = false },
};

/*---------------------------------------------------------------------------
 * Internal Constants (all fixed-point 8.8)
 *---------------------------------------------------------------------------*/

/** 0.5 in 8.8 fixed-point */
#define FX_HALF         0x0080
/** 1.0 in 8.8 fixed-point */
#define FX_ONE          0x0100
/** 2.0 in 8.8 fixed-point */
#define FX_TWO          0x0200
/** 0.75 in 8.8 fixed-point (subpixel quality default) */
#define FX_SUBPIX_75    0x00C0
/** 0.125 in 8.8 fixed (1/8) */
#define FX_EIGHTH       0x0020

/*---------------------------------------------------------------------------
 * Helper: Blend Two RGB565 Pixels
 *---------------------------------------------------------------------------*/

u16 fxaa_blend_pixels(u16 c0, u16 c1, uint16_t factor)
{
    /*
     * factor is 0-256, where 0 = all c0, 256 = all c1.
     * Blend each channel independently.
     */
    if (factor == 0)   return c0;
    if (factor >= 256) return c1;

    uint32_t inv = 256 - factor;

    /* Red:   5 bits → scale to 8 bits, blend, scale back */
    /* Green: 6 bits → scale to 8 bits, blend, scale back */
    /* Blue:  5 bits → scale to 8 bits, blend, scale back */

    uint32_t r0 = ((c0 >> 11) & 0x1F) << 3;   /* Scale to 0-248 */
    uint32_t g0 = ((c0 >> 5)  & 0x3F) << 2;   /* Scale to 0-252 */
    uint32_t b0 = (c0         & 0x1F) << 3;   /* Scale to 0-248 */

    uint32_t r1 = ((c1 >> 11) & 0x1F) << 3;
    uint32_t g1 = ((c1 >> 5)  & 0x3F) << 2;
    uint32_t b1 = (c1         & 0x1F) << 3;

    uint32_t r = (r0 * inv + r1 * factor) >> 8;
    uint32_t g = (g0 * inv + g1 * factor) >> 8;
    uint32_t b = (b0 * inv + b1 * factor) >> 8;

    return (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/*---------------------------------------------------------------------------
 * Helper: Edge Endpoint Search
 *---------------------------------------------------------------------------*/

uint8_t fxaa_edge_search(const uint8_t* luma_buf,
                         int x, int y,
                         int step_x, int step_y,
                         uint8_t max_steps)
{
    if (max_steps == 0) return 0;

    int center_luma = luma_buf[y * SCREEN_TOP_WIDTH + x];
    int prev_luma   = center_luma;
    uint8_t steps   = 0;

    for (uint8_t i = 1; i <= max_steps; i++) {
        int cx = x + step_x * i;
        int cy = y + step_y * i;

        /* Bounds check */
        if (cx < 0 || cx >= SCREEN_TOP_WIDTH ||
            cy < 0 || cy >= SCREEN_TOP_HEIGHT) {
            break;
        }

        int cur_luma = luma_buf[cy * SCREEN_TOP_WIDTH + cx];
        int delta    = cur_luma - prev_luma;

        /* Sign change in delta → edge endpoint found */
        if ((prev_luma - center_luma > 0) != (cur_luma - center_luma > 0)) {
            steps = i;
            break;
        }

        /* Decreasing contrast → we're past the edge */
        int abs_delta = (delta < 0) ? -delta : delta;
        if (prev_luma != center_luma && abs_delta < 2) {
            steps = i;
            break;
        }

        prev_luma = cur_luma;
        steps = i;
    }

    return steps;
}

/*---------------------------------------------------------------------------
 * Helper: Compute Blend Factor
 *---------------------------------------------------------------------------*/

uint8_t fxaa_compute_blend(uint8_t luma_center,
                           uint8_t luma_n, uint8_t luma_s,
                           uint8_t luma_e, uint8_t luma_w,
                           edge_dir_t direction,
                           uint16_t threshold)
{
    (void)threshold;  /* May be used by future quality presets */

    /*
     * Calculate local contrast: max - min in the 3×3 cross neighborhood.
     */
    uint8_t luma_max = luma_center;
    uint8_t luma_min = luma_center;

    if (luma_n > luma_max) luma_max = luma_n;
    if (luma_n < luma_min) luma_min = luma_n;
    if (luma_s > luma_max) luma_max = luma_s;
    if (luma_s < luma_min) luma_min = luma_s;
    if (luma_e > luma_max) luma_max = luma_e;
    if (luma_e < luma_min) luma_min = luma_e;
    if (luma_w > luma_max) luma_max = luma_w;
    if (luma_w < luma_min) luma_min = luma_w;

    uint16_t luma_range = (uint16_t)(luma_max - luma_min);

    /* Scale to 8.8 for the threshold comparison */
    uint16_t range_fx = (uint16_t)(luma_range << 8);

    /*
     * Early out: if range is below 1/4 of threshold,
     * this isn't really an edge worth blending.
     */
    if (range_fx < (threshold >> 2)) {
        return 0;
    }

    /*
     * Compute blend amount based on how much the center pixel
     * deviates from the average of its edge-direction neighbors.
     */
    uint32_t avg_neighbor;
    switch (direction) {
        case EDGE_HORIZONTAL:
            /* Edge runs horizontally — blend north-south */
            avg_neighbor = (uint32_t)(luma_n + luma_s);
            break;
        case EDGE_VERTICAL:
            /* Edge runs vertically — blend east-west */
            avg_neighbor = (uint32_t)(luma_e + luma_w);
            break;
        case EDGE_DIAG_UP:
            /* Diagonal / — blend SW-NE */
            avg_neighbor = (uint32_t)(luma_n + luma_s + luma_e + luma_w) >> 1;
            break;
        case EDGE_DIAG_DOWN:
            /* Diagonal \ — blend NW-SE */
            avg_neighbor = (uint32_t)(luma_n + luma_s + luma_e + luma_w) >> 1;
            break;
        default:
            return 0;
    }
    avg_neighbor >>= 1;  /* Divide by 2 to get true average */

    int32_t diff = (int32_t)luma_center - (int32_t)avg_neighbor;
    int32_t abs_diff = (diff < 0) ? -diff : diff;

    /*
     * Blend factor: proportional to how much the center deviates.
     * Formula: blend = clamp(abs_diff / range, 0, 1) * 256
     */
    if (luma_range < 4) return 0;  /* Avoid division by near-zero */

    uint32_t blend = (uint32_t)(abs_diff << 8) / luma_range;

    /* Clamp to 0-255 */
    return (blend > 255) ? 255 : (uint8_t)blend;
}

/*---------------------------------------------------------------------------
 * Per-Row FXAA Processing
 *---------------------------------------------------------------------------*/

void fxaa_process_row(u16* fb_row,
                      uint32_t width,
                      const uint8_t* luma_row,
                      const uint8_t* luma_prev,
                      const uint8_t* luma_next,
                      const edge_result_t* edges,
                      uint8_t quality,
                      uint16_t edge_thresh)
{
    const fxaa_quality_t* q = &FXAA_QUALITY_PRESETS[quality & 3];

    /* Skip the border columns */
    for (uint32_t x = 1; x < width - 1; x++) {
        const edge_result_t* edge = &edges[x];

        if (!edge->is_edge) {
            continue;
        }

        if (q->early_out) {
            /*
             * Quick check: if no neighboring pixel is an edge,
             * skip processing (isolated speck, not a true edge).
             */
            if (!edges[x - 1].is_edge && !edges[x + 1].is_edge) {
                continue;
            }
        }

        /*
         * Compute blend factor based on local neighborhood.
         */
        uint8_t blend = fxaa_compute_blend(
            luma_row[x],
            luma_prev[x],       /* North */
            luma_next[x],       /* South */
            luma_row[x + 1],    /* East  */
            luma_row[x - 1],    /* West  */
            edge->direction,
            edge_thresh
        );

        if (blend == 0) {
            continue;
        }

        /*
         * Select pixel pair to blend across based on edge direction.
         * We blend the center pixel with the neighbor opposite to
         * the gradient direction.
         */
        u16 c0 = fb_row[x];     /* Center pixel */
        u16 c1;                 /* Opposite neighbor along edge perpendicular */

        switch (edge->direction) {
            case EDGE_HORIZONTAL:
                /* Edge horizontal → blend vertically. Pick the neighbor
                 * with luminance closer to the average. */
                {
                    int diff_n = (int)luma_prev[x] - (int)luma_row[x];
                    int diff_s = (int)luma_next[x] - (int)luma_row[x];
                    int abs_n = (diff_n < 0) ? -diff_n : diff_n;
                    int abs_s = (diff_s < 0) ? -diff_s : diff_s;
                    c1 = (abs_n < abs_s) ? fb_row[x - SCREEN_TOP_WIDTH]
                                         : fb_row[x + SCREEN_TOP_WIDTH];
                }
                break;

            case EDGE_VERTICAL:
                /* Edge vertical → blend horizontally. */
                {
                    int diff_e = (int)luma_row[x + 1] - (int)luma_row[x];
                    int diff_w = (int)luma_row[x - 1] - (int)luma_row[x];
                    int abs_e = (diff_e < 0) ? -diff_e : diff_e;
                    int abs_w = (diff_w < 0) ? -diff_w : diff_w;
                    c1 = (abs_e < abs_w) ? fb_row[x + 1] : fb_row[x - 1];
                }
                break;

            case EDGE_DIAG_UP:
                /* Diagonal / → blend NE-SW */
                c1 = fb_row[x + 1];  /* Default to east neighbor */
                break;

            case EDGE_DIAG_DOWN:
                /* Diagonal \ → blend NW-SE */
                c1 = fb_row[x - 1];  /* Default to west neighbor */
                break;

            default:
                continue;
        }

        /*
         * Apply the blend. Scale blend from 0-255 to 0-256 for
         * the blend function.
         */
        uint16_t blend_256 = (uint16_t)((blend * 257) >> 8);  /* Expand 0-255 to 0-256 */
        fb_row[x] = fxaa_blend_pixels(c0, c1, blend_256);

        /*
         * Sub-pixel AA: slightly blend with neighbors to smooth
         * sub-pixel features (thin lines smaller than a pixel).
         */
        if (q->subpixel_samples > 0) {
            uint32_t nw = rgb565_to_luma(fb_row[x - 1]);
            uint32_t ne = rgb565_to_luma(fb_row[x + 1]);
            uint32_t avg = (uint32_t)(luma_row[x] + nw + ne) / 3;

            int32_t sub_diff = (int32_t)luma_row[x] - (int32_t)avg;
            int32_t abs_sub  = (sub_diff < 0) ? -sub_diff : sub_diff;

            if (abs_sub > 4) {
                uint16_t sub_blend = (uint16_t)((MIN((uint32_t)abs_sub * 48, 128)) * 257 >> 8);
                u16 avg_color = fxaa_blend_pixels(fb_row[x],
                    fxaa_blend_pixels(fb_row[x - 1], fb_row[x + 1], 128),
                    128);
                fb_row[x] = fxaa_blend_pixels(fb_row[x], avg_color, sub_blend);
            }
        }
    }
}

/*---------------------------------------------------------------------------
 * Full-Frame FXAA Pass
 *---------------------------------------------------------------------------*/

uint64_t fxaa_process_frame(u16* fb_rgb565,
                            uint8_t quality,
                            uint16_t edge_thresh)
{
    if (!fb_rgb565) return 0;

    /* Start timing */
    uint64_t t_start = timing_get_cycles();

    /*
     * Processing pipeline:
     *   1. Convert to luminance (row by row)
     *   2. Edge detect (needs 3 rows)
     *   3. FXAA blend (modifies RGB565 in-place)
     *
     * We process in a streaming fashion using a 3-row sliding window
     * to keep memory usage low (3 × 400 bytes for luma + 1 edge row).
     */

    uint8_t   luma_ring[3][SCREEN_TOP_WIDTH];
    edge_result_t edge_row[SCREEN_TOP_WIDTH];

    /* Row indices for the sliding window */
    int prev_idx = 0;
    int curr_idx = 1;
    int next_idx = 2;

    /* Pre-load first two rows */
    luma_from_rgb565_row(&fb_rgb565[0 * SCREEN_TOP_WIDTH],
                         luma_ring[prev_idx], SCREEN_TOP_WIDTH);
    luma_from_rgb565_row(&fb_rgb565[1 * SCREEN_TOP_WIDTH],
                         luma_ring[curr_idx], SCREEN_TOP_WIDTH);

    for (uint32_t y = 0; y < SCREEN_TOP_HEIGHT; y++) {
        /* Determine next row (clamp to last row for bottom edge) */
        uint32_t next_y = (y + 1 < SCREEN_TOP_HEIGHT) ? (y + 1) : y;

        /* Convert next row to luminance */
        luma_from_rgb565_row(&fb_rgb565[next_y * SCREEN_TOP_WIDTH],
                             luma_ring[next_idx], SCREEN_TOP_WIDTH);

        /* Edge detect on current row */
        edge_detect_row(luma_ring[curr_idx],
                        luma_ring[prev_idx],
                        luma_ring[next_idx],
                        edge_row,
                        SCREEN_TOP_WIDTH,
                        edge_thresh);

        /* Apply FXAA blend to current row */
        fxaa_process_row(&fb_rgb565[y * SCREEN_TOP_WIDTH],
                         SCREEN_TOP_WIDTH,
                         luma_ring[curr_idx],
                         luma_ring[prev_idx],
                         luma_ring[next_idx],
                         edge_row,
                         quality,
                         edge_thresh);

        /* Rotate row buffer indices for next iteration */
        int tmp  = prev_idx;
        prev_idx = curr_idx;
        curr_idx = next_idx;
        next_idx = tmp;
    }

    uint64_t t_end = timing_get_cycles();
    return t_end - t_start;
}
