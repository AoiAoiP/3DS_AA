/**
 * @file   framebuffer.h
 * @brief  Framebuffer Capture & RGB565 Manipulation
 *
 * Provides direct access to the 3DS top-screen framebuffer for
 * read-modify-write post-processing. Handles RGB565 format conversion
 * and the double-buffered capture pipeline.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "aa_plugin.h"

/*---------------------------------------------------------------------------
 * Initialization & Teardown
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize the framebuffer subsystem.
 *
 * Allocates the back-buffer and sets up GPU access for screen capture.
 * Must be called once at plugin startup.
 *
 * @return true on success, false on allocation failure.
 */
bool framebuffer_init(void);

/**
 * @brief Shutdown framebuffer subsystem and free resources.
 */
void framebuffer_exit(void);

/*---------------------------------------------------------------------------
 * Frame Capture
 *---------------------------------------------------------------------------*/

/**
 * @brief Capture the current top-screen left-eye framebuffer.
 *
 * Copies the GPU front-buffer into our back-buffer for processing.
 * On New 3DS in 3D mode, captures only the left-eye view.
 *
 * @return Pointer to the captured framebuffer (back_buffer),
 *         or NULL if capture failed.
 */
u16* framebuffer_capture(void);

/*---------------------------------------------------------------------------
 * Pixel Operations (RGB565)
 *---------------------------------------------------------------------------*/

/**
 * @brief Convert an RGB565 pixel to 8-bit-per-channel luminance.
 *
 * Uses ITU-R BT.601 coefficients. Result is 0-255.
 *
 * @param rgb  Raw RGB565 pixel value.
 * @return     8-bit luminance value.
 */
static inline uint8_t rgb565_to_luma(u16 rgb)
{
    /* BT.601: Y = 0.299*R + 0.587*G + 0.114*B */
    /* Scale to 0-255 with integer math */
    uint32_t r = RGB565_R(rgb) * 255 / 31;
    uint32_t g = RGB565_G(rgb) * 255 / 63;
    uint32_t b = RGB565_B(rgb) * 255 / 31;
    return (uint8_t)((r * 76 + g * 150 + b * 29 + 128) >> 8);
}

/**
 * @brief Convert 8-bit R,G,B values to packed RGB565.
 *
 * @param r  Red channel   (0-31, 5-bit).
 * @param g  Green channel (0-63, 6-bit).
 * @param b  Blue channel  (0-31, 5-bit).
 * @return   Packed RGB565 value.
 */
static inline u16 rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return RGB565_PACK(r & 0x1F, g & 0x3F, b & 0x1F);
}

/**
 * @brief Remove the red channel from every pixel in a buffer.
 *
 * Diagnostic function (Stage 1 verification): should turn the
 * display cyan/blue-green to confirm we have write access to
 * the framebuffer.
 *
 * @param buf    Pointer to RGB565 buffer.
 * @param count  Number of pixels in the buffer.
 */
void framebuffer_kill_red(u16* buf, uint32_t count);

/**
 * @brief Copy a portion of the framebuffer back to the front buffer.
 *
 * After AA processing, writes the modified back-buffer to the
 * front-buffer, respecting V-sync to avoid tearing.
 *
 * @param src  Source buffer (processed back-buffer).
 * @return     true on success.
 */
bool framebuffer_swap(u16* src);

/*---------------------------------------------------------------------------
 * Double Buffer Management
 *---------------------------------------------------------------------------*/

/**
 * @brief Get the current back-buffer for processing.
 * @return Pointer to the back-buffer.
 */
u16* framebuffer_get_back_buffer(void);

/**
 * @brief Get the front-buffer (GPU display buffer).
 * @return Direct pointer to GPU framebuffer.
 */
u16* framebuffer_get_front_buffer(void);

#endif /* FRAMEBUFFER_H */
