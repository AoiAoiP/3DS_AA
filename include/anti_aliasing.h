/**
 * @file   anti_aliasing.h
 * @brief  FXAA 3.11 Subset - Anti-Aliasing Core
 *
 * Fixed-point implementation of FXAA (Fast Approximate Anti-Aliasing)
 * optimized for the ARM11 CPU on New 3DS. This is a streamlined version
 * of FXAA 3.11 that:
 *
 *  - Uses only luminance (Y) channel for edge detection
 *  - Employs 8.8 fixed-point arithmetic throughout (no FPU)
 *  - Supports configurable quality presets (0-3) trading quality for speed
 *  - Includes edge direction detection + bilinear blending along edges
 *
 * Reference: "FXAA 3.11" by Timothy Lottes, NVIDIA (2009)
 */

#ifndef ANTI_ALIASING_H
#define ANTI_ALIASING_H

#include "aa_plugin.h"
#include "edge_detect.h"

/*---------------------------------------------------------------------------
 * FXAA Quality Presets
 *---------------------------------------------------------------------------*/

/** Quality preset configuration */
typedef struct {
    uint8_t max_steps;          /**< Maximum search steps along edge */
    uint8_t subpixel_samples;   /**< Sub-pixel AA sample count (0=none) */
    bool    early_out;          /**< Skip processing if no edge nearby */
} fxaa_quality_t;

/** Predefined quality presets */
extern const fxaa_quality_t FXAA_QUALITY_PRESETS[4];

/*---------------------------------------------------------------------------
 * Core AA API
 *---------------------------------------------------------------------------*/

/**
 * @brief Apply FXAA to a complete framebuffer.
 *
 * Main entry point for the anti-aliasing pass. Processes the entire
 * top-screen framebuffer with the configured quality preset.
 *
 * @param fb_rgb565     Input/output: RGB565 framebuffer (400x240).
 *                      Modified in-place with anti-aliased result.
 * @param quality       Quality preset level (0=fastest, 3=best).
 * @param edge_thresh   Edge detection contrast threshold (8.8 fixed).
 * @return              Processing time in ARM11 cycles.
 */
uint64_t fxaa_process_frame(u16* fb_rgb565,
                            uint8_t quality,
                            uint16_t edge_thresh);

/**
 * @brief Apply FXAA to a single scanline.
 *
 * Row-by-row processing variant — useful when memory is tight
 * or when pipelining with other per-scanline operations.
 *
 * @param fb_row        Input/output: one RGB565 scanline (400 pixels).
 * @param width         Row width in pixels.
 * @param luma_row      Luminance values for current row.
 * @param luma_prev     Luminance values for previous row.
 * @param luma_next     Luminance values for next row.
 * @param edges         Edge detection results for current row.
 * @param quality       Quality preset.
 * @param edge_thresh   Edge threshold (8.8 fixed).
 */
void fxaa_process_row(u16* fb_row,
                      uint32_t width,
                      const uint8_t* luma_row,
                      const uint8_t* luma_prev,
                      const uint8_t* luma_next,
                      const edge_result_t* edges,
                      uint8_t quality,
                      uint16_t edge_thresh);

/*---------------------------------------------------------------------------
 * Internal FXAA Helpers (exposed for testing / benchmarking)
 *---------------------------------------------------------------------------*/

/**
 * @brief Compute the FXAA edge blend factor for a single pixel.
 *
 * Determines how much to blend the pixel with its neighbors along
 * the detected edge direction.
 *
 * @param luma_center   Luminance of the center pixel.
 * @param luma_n        Luminance of the north neighbor.
 * @param luma_s        Luminance of the south neighbor.
 * @param luma_e        Luminance of the east neighbor.
 * @param luma_w        Luminance of the west neighbor.
 * @param direction     Edge direction classification.
 * @param threshold     Contrast threshold (8.8 fixed).
 * @return              Blend factor (0 = no blend, 255 = full blend).
 */
uint8_t fxaa_compute_blend(uint8_t luma_center,
                           uint8_t luma_n, uint8_t luma_s,
                           uint8_t luma_e, uint8_t luma_w,
                           edge_dir_t direction,
                           uint16_t threshold);

/**
 * @brief Blend two RGB565 pixels by a given factor.
 *
 * @param c0       Pixel color on one side of the edge.
 * @param c1       Pixel color on the other side.
 * @param factor   Blend weight (0=all c0, 256=all c1), in 0-256 units.
 * @return         Blended RGB565 pixel.
 */
u16 fxaa_blend_pixels(u16 c0, u16 c1, uint16_t factor);

/**
 * @brief Search along an edge to find its endpoints.
 *
 * Walks along the detected edge direction to determine how far
 * the edge extends, which controls the blend kernel width.
 *
 * @param luma_buf      Luminance frame buffer.
 * @param x             Starting X coordinate.
 * @param y             Starting Y coordinate.
 * @param step_x        X step direction (-1, 0, or 1).
 * @param step_y        Y step direction (-1, 0, or 1).
 * @param max_steps     Maximum search distance.
 * @return              Distance to edge endpoint (in pixels).
 */
uint8_t fxaa_edge_search(const uint8_t* luma_buf,
                         int x, int y,
                         int step_x, int step_y,
                         uint8_t max_steps);

#endif /* ANTI_ALIASING_H */
