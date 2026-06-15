/**
 * @file   edge_detect.h
 * @brief  Edge Detection for Anti-Aliasing
 *
 * Implements luminance-based edge detection using Sobel-like operators
 * optimized for the ARM11 CPU. Detects edge pixels, their gradient
 * magnitude, and local edge direction for use by the FXAA blending pass.
 *
 * All computation is done in fixed-point integer arithmetic to avoid
 * the 3DS's lack of a hardware FPU.
 */

#ifndef EDGE_DETECT_H
#define EDGE_DETECT_H

#include "aa_plugin.h"

/*---------------------------------------------------------------------------
 * Edge Detection Constants
 *---------------------------------------------------------------------------*/

/** Fixed-point 8.8 representation of 1.0 */
#define FX_ONE              0x0100

/** Luminance contrast threshold for edge detection (8.8 fixed-point) */
#define EDGE_CONTRAST_THRESH  FX8_8(0.05f)

/*---------------------------------------------------------------------------
 * Data Structures
 *---------------------------------------------------------------------------*/

/**
 * @brief Edge direction classification.
 *
 * Edges are classified by the dominant local gradient direction.
 * The AA blend kernel is chosen based on this classification.
 */
typedef enum {
    EDGE_NONE = 0,      /**< No significant edge detected */
    EDGE_HORIZONTAL,    /**< Edge runs approximately horizontally */
    EDGE_VERTICAL,      /**< Edge runs approximately vertically */
    EDGE_DIAG_UP,       /**< Diagonal edge: / (rising) */
    EDGE_DIAG_DOWN,     /**< Diagonal edge: \ (falling) */
    EDGE_COUNT
} edge_dir_t;

/**
 * @brief Per-pixel edge detection result.
 */
typedef struct {
    edge_dir_t  direction;      /**< Edge classification */
    uint16_t    gradient_mag;   /**< Gradient magnitude (0-4095) */
    uint16_t    gradient_dir;   /**< Gradient angle (0-255, 0=0°, 64=90°) */
    bool        is_edge;        /**< True if pixel is on an edge */
    bool        is_horizontal;  /**< True if edge is primarily horizontal */
} edge_result_t;

/*---------------------------------------------------------------------------
 * Edge Detection API
 *---------------------------------------------------------------------------*/

/**
 * @brief Detect edges in a single row of the framebuffer.
 *
 * Processes one scanline for edges using a 3x3 Sobel operator
 * on the luminance channel. Designed for row-by-row streaming
 * to minimize cache pressure.
 *
 * @param luma_row     Pointer to luminance values for current row.
 * @param luma_prev    Pointer to luminance values for previous row.
 * @param luma_next    Pointer to luminance values for next row.
 * @param results      Output: edge detection results for this row.
 * @param width        Width of the row in pixels.
 * @param threshold    Edge contrast threshold (8.8 fixed-point).
 */
void edge_detect_row(const uint8_t* luma_row,
                     const uint8_t* luma_prev,
                     const uint8_t* luma_next,
                     edge_result_t* results,
                     uint32_t width,
                     uint16_t threshold);

/**
 * @brief Full-frame edge detection pass.
 *
 * Converts a complete RGB565 framebuffer to luminance, then
 * runs Sobel edge detection across the entire frame.
 *
 * @param fb           Input RGB565 framebuffer (top screen, 400x240).
 * @param edges        Output edge map (same dimensions).
 * @param threshold    Edge contrast threshold (8.8 fixed-point).
 * @return             Number of edge pixels detected.
 */
uint32_t edge_detect_frame(const u16* fb,
                           edge_result_t* edges,
                           uint16_t threshold);

/**
 * @brief Compute luminance for a single row of RGB565 pixels.
 *
 * Converts one scanline from RGB565 to 8-bit luminance.
 * Optimized with precomputed lookup tables.
 *
 * @param rgb_row   Input RGB565 scanline.
 * @param luma_row  Output luminance values (0-255).
 * @param width     Number of pixels in the row.
 */
void luma_from_rgb565_row(const u16* rgb_row,
                          uint8_t* luma_row,
                          uint32_t width);

/**
 * @brief Initialize edge detection subsystem.
 *
 * Precomputes luminance lookup tables for fast RGB565→Luma conversion.
 */
void edge_detect_init(void);

/**
 * @brief Cleanup edge detection resources.
 */
void edge_detect_exit(void);

#endif /* EDGE_DETECT_H */
