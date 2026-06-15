/**
 * @file   edge_detect.c
 * @brief  Edge Detection for Anti-Aliasing
 *
 * Implements luminance-based Sobel edge detection optimized for ARM11.
 * Uses a 3×3 convolution kernel to compute gradient magnitude and
 * direction at each pixel. All arithmetic is integer/fixed-point.
 *
 * Sobel operators:
 *   Gx = [-1  0  +1]    Gy = [-1 -2 -1]
 *        [-2  0  +2]         [ 0  0  0]
 *        [-1  0  +1]         [+1 +2 +1]
 */

#include "edge_detect.h"

/*---------------------------------------------------------------------------
 * Luminance Lookup Table (LUT)
 *---------------------------------------------------------------------------*/

/**
 * Precomputed luminance LUT for RGB565 → 8-bit luma conversion.
 * Maps 65536 possible RGB565 values to 8-bit luminance.
 * This turns the per-pixel conversion into a single array lookup.
 */
static uint8_t g_luma_lut[65536];

/*---------------------------------------------------------------------------
 * Internal Helpers
 *---------------------------------------------------------------------------*/

/**
 * Compute a single luminance value from RGB565.
 * Used to populate the LUT at startup.
 */
static uint8_t compute_luma_from_rgb565(u16 rgb)
{
    uint32_t r = RGB565_R(rgb) * 255 / 31;
    uint32_t g = RGB565_G(rgb) * 255 / 63;
    uint32_t b = RGB565_B(rgb) * 255 / 31;

    /* BT.601 luma: Y = 0.299*R + 0.587*G + 0.114*B */
    return (uint8_t)((r * 76 + g * 150 + b * 29 + 128) >> 8);
}

/*---------------------------------------------------------------------------
 * Initialization
 *---------------------------------------------------------------------------*/

void edge_detect_init(void)
{
    /* Build the full 64K-entry luminance lookup table */
    for (uint32_t i = 0; i < 65536; i++) {
        g_luma_lut[i] = compute_luma_from_rgb565((u16)i);
    }
}

void edge_detect_exit(void)
{
    /* Nothing to free — LUT is statically allocated */
}

/*---------------------------------------------------------------------------
 * Per-Row Luminance Conversion
 *---------------------------------------------------------------------------*/

void luma_from_rgb565_row(const u16* rgb_row, uint8_t* luma_row, uint32_t width)
{
    for (uint32_t x = 0; x < width; x++) {
        luma_row[x] = g_luma_lut[rgb_row[x]];
    }
}

/*---------------------------------------------------------------------------
 * Sobel Edge Detection (Per-Row)
 *---------------------------------------------------------------------------*/

void edge_detect_row(const uint8_t* luma_row,
                     const uint8_t* luma_prev,
                     const uint8_t* luma_next,
                     edge_result_t* results,
                     uint32_t width,
                     uint16_t threshold)
{
    /*
     * Skip the leftmost and rightmost columns (border pixels)
     * since the Sobel operator needs a full 3×3 neighborhood.
     */
    results[0].is_edge   = false;
    results[0].direction = EDGE_NONE;
    results[width - 1].is_edge   = false;
    results[width - 1].direction = EDGE_NONE;

    for (uint32_t x = 1; x < width - 1; x++) {
        /*
         * Sobel convolution — read 3×3 neighborhood.
         *
         *   p00 p01 p02     NW  N  NE
         *   p10  C  p12  =   W  C   E
         *   p20 p21 p22     SW  S  SE
         */
        int32_t p00 = luma_prev[x - 1];
        int32_t p01 = luma_prev[x];
        int32_t p02 = luma_prev[x + 1];
        int32_t p10 = luma_row[x - 1];
        int32_t p12 = luma_row[x + 1];
        int32_t p20 = luma_next[x - 1];
        int32_t p21 = luma_next[x];
        int32_t p22 = luma_next[x + 1];

        /*
         * Compute Sobel gradients.
         *
         * Gx = (-1*p00 + 0*p01 + 1*p02) +
         *      (-2*p10 + 0*C   + 2*p12) +
         *      (-1*p20 + 0*p21 + 1*p22)
         *
         * Gy = (-1*p00 + -2*p01 + -1*p02) +
         *      ( 0*p10 +  0*C   +  0*p12) +
         *      ( 1*p20 +  2*p21 +  1*p22)
         */
        int32_t gx = -p00 + p02
                     - (p10 << 1) + (p12 << 1)
                     - p20 + p22;

        int32_t gy = -p00 - (p01 << 1) - p02
                     + p20 + (p21 << 1) + p22;

        /* Gradient magnitude: |G| = |Gx| + |Gy| (fast approximation of sqrt(Gx²+Gy²)) */
        int32_t abs_gx = (gx < 0) ? -gx : gx;
        int32_t abs_gy = (gy < 0) ? -gy : gy;
        uint32_t mag = (uint32_t)(abs_gx + abs_gy);

        /* Clamp to 12 bits (0-4095) */
        if (mag > 4095) mag = 4095;

        edge_result_t* result = &results[x];

        /* Threshold check — is this an edge? */
        if (mag < threshold) {
            result->is_edge   = false;
            result->direction = EDGE_NONE;
            result->gradient_mag = 0;
            result->gradient_dir = 0;
            result->is_horizontal = false;
            continue;
        }

        result->is_edge      = true;
        result->gradient_mag = (uint16_t)mag;

        /*
         * Classify edge direction based on gradient components.
         *
         * Edge direction is perpendicular to the gradient:
         *   - If |Gx| ≫ |Gy| → vertical edge (gradient points horizontally)
         *   - If |Gy| ≫ |Gx| → horizontal edge (gradient points vertically)
         *   - Otherwise → diagonal edge
         */

        /* Gradient angle: approximate atan2(Gy, Gx) in 0-255 range */
        if (abs_gx == 0 && abs_gy == 0) {
            result->gradient_dir = 0;
        } else {
            /*
             * Fast atan2 approximation using octant mapping.
             * Result is 0-255: 0=0°, 64=90°, 128=180°, 192=270°.
             */
            uint32_t ratio;
            if (abs_gx > abs_gy) {
                ratio = (uint32_t)(abs_gy * 256) / abs_gx;
                result->gradient_dir = (uint16_t)(ratio >> 1);  /* 0-127 range */
            } else {
                ratio = (uint32_t)(abs_gx * 256) / abs_gy;
                result->gradient_dir = (uint16_t)(128 - (ratio >> 1)); /* 128-0 range */
            }

            /* Adjust for quadrant */
            if (gx < 0) {
                result->gradient_dir = (uint16_t)(256 - result->gradient_dir);
            }
            if (gy < 0) {
                result->gradient_dir = (uint16_t)((result->gradient_dir + 128) & 0xFF);
            }
            result->gradient_dir &= 0xFF;
        }

        /*
         * Determine edge type from gradient direction.
         *
         * The edge itself is perpendicular to the gradient:
         *   |Gy| ≫ |Gx| → gradient is vertical → edge is horizontal
         *   |Gx| ≫ |Gy| → gradient is horizontal → edge is vertical
         */
        if (abs_gy > (abs_gx << 1)) {
            /* Gradient strongly vertical → edge is horizontal */
            result->direction     = EDGE_HORIZONTAL;
            result->is_horizontal = true;
        } else if (abs_gx > (abs_gy << 1)) {
            /* Gradient strongly horizontal → edge is vertical */
            result->direction     = EDGE_VERTICAL;
            result->is_horizontal = false;
        } else if (gx * gy > 0) {
            /* Both same sign → diagonal \ */
            result->direction     = EDGE_DIAG_DOWN;
            result->is_horizontal = false;
        } else {
            /* Opposite signs → diagonal / */
            result->direction     = EDGE_DIAG_UP;
            result->is_horizontal = false;
        }
    }
}

/*---------------------------------------------------------------------------
 * Full-Frame Edge Detection
 *---------------------------------------------------------------------------*/

uint32_t edge_detect_frame(const u16* fb,
                           edge_result_t* edges,
                           uint16_t threshold)
{
    if (!fb || !edges) return 0;

    /*
     * We need 3 rows of luminance: prev, current, next.
     * Use rotating row buffers to avoid full-frame allocation.
     */
    uint8_t luma_buf[3][SCREEN_TOP_WIDTH];
    uint32_t edge_count = 0;

    /* Initialize row pointers (cyclic buffer indices) */
    int prev_idx = 0;
    int curr_idx = 1;
    int next_idx = 2;

    /* Convert first row twice (for row 0, prev = row 0 itself) */
    luma_from_rgb565_row(&fb[0 * SCREEN_TOP_WIDTH],
                         luma_buf[prev_idx], SCREEN_TOP_WIDTH);
    luma_from_rgb565_row(&fb[0 * SCREEN_TOP_WIDTH],
                         luma_buf[curr_idx], SCREEN_TOP_WIDTH);

    for (uint32_t y = 0; y < SCREEN_TOP_HEIGHT; y++) {
        /* Determine next row index */
        uint32_t next_y = (y + 1 < SCREEN_TOP_HEIGHT) ? (y + 1) : y;

        /* Convert next row to luminance */
        luma_from_rgb565_row(&fb[next_y * SCREEN_TOP_WIDTH],
                             luma_buf[next_idx], SCREEN_TOP_WIDTH);

        /* Detect edges on current row */
        edge_detect_row(luma_buf[curr_idx],
                        luma_buf[prev_idx],
                        luma_buf[next_idx],
                        &edges[y * SCREEN_TOP_WIDTH],
                        SCREEN_TOP_WIDTH,
                        threshold);

        /* Count edge pixels in this row */
        for (uint32_t x = 1; x < SCREEN_TOP_WIDTH - 1; x++) {
            if (edges[y * SCREEN_TOP_WIDTH + x].is_edge) {
                edge_count++;
            }
        }

        /* Rotate row buffers */
        int tmp  = prev_idx;
        prev_idx = curr_idx;
        curr_idx = next_idx;
        next_idx = tmp;
    }

    return edge_count;
}
