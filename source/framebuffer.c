/**
 * @file   framebuffer.c
 * @brief  Framebuffer Capture & RGB565 Manipulation
 *
 * Implementation of the framebuffer subsystem. Handles:
 *  - Capturing the 3DS top-screen framebuffer
 *  - RGB565 format pixel operations
 *  - Double-buffer allocation and swap
 *  - Diagnostic "kill red" function (Stage 1 verification)
 */

#include "framebuffer.h"

/*---------------------------------------------------------------------------
 * Module State
 *---------------------------------------------------------------------------*/

/** Pointer to the GPU framebuffer (set by gfxGetFramebuffer) */
static u16* g_front_buffer = NULL;

/** Back-buffer for double-buffered AA processing */
static u16* g_back_buffer = NULL;
static u32  g_back_buffer_size = 0;

/*---------------------------------------------------------------------------
 * Initialization & Teardown
 *---------------------------------------------------------------------------*/

bool framebuffer_init(void)
{
    /* Get the front-buffer pointer from the GPU driver */
    g_front_buffer = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    if (!g_front_buffer) {
        /* Fallback: try without specifying eye */
        g_front_buffer = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    }

    if (!g_front_buffer) {
        return false;
    }

    /* Calculate back-buffer size: 400×240 pixels × 2 bytes/pixel (RGB565) */
    g_back_buffer_size = SCREEN_TOP_PIXELS * sizeof(u16);

    /* Allocate back-buffer from linear heap (CPU-accessible RAM) */
    g_back_buffer = (u16*)linearAlloc(g_back_buffer_size);

    if (!g_back_buffer) {
        return false;
    }

    /* Initialize back-buffer to black */
    memset(g_back_buffer, 0, g_back_buffer_size);

    /* Set up global context */
    g_aa_ctx.back_buffer      = g_back_buffer;
    g_aa_ctx.back_buffer_size = g_back_buffer_size;
    g_aa_ctx.back_buffer_ready = false;

    return true;
}

void framebuffer_exit(void)
{
    if (g_back_buffer) {
        linearFree(g_back_buffer);
        g_back_buffer = NULL;
    }

    g_front_buffer     = NULL;
    g_back_buffer_size = 0;

    g_aa_ctx.back_buffer       = NULL;
    g_aa_ctx.back_buffer_size  = 0;
    g_aa_ctx.back_buffer_ready = false;
}

/*---------------------------------------------------------------------------
 * Frame Capture
 *---------------------------------------------------------------------------*/

u16* framebuffer_capture(void)
{
    if (!g_front_buffer || !g_back_buffer) {
        return NULL;
    }

    /* Re-acquire front buffer pointer each frame (some games may move it) */
    u16* fb = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (fb) {
        g_front_buffer = fb;
    }

    /*
     * DMA-style copy from front buffer to back buffer.
     * On ARM11, memcpy does 32-bit aligned LDM/STM bursts for speed.
     * We own the back buffer, so no tearing concerns here.
     */
    memcpy(g_back_buffer, g_front_buffer, g_back_buffer_size);

    /* ARM data synchronization barrier — ensure copy is visible */
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");

    g_aa_ctx.back_buffer_ready = true;

    return g_back_buffer;
}

/*---------------------------------------------------------------------------
 * Pixel Operations (RGB565)
 *---------------------------------------------------------------------------*/

void framebuffer_kill_red(u16* buf, uint32_t count)
{
    if (!buf || !count) return;

    u16* end = buf + count;

    while (buf < end) {
        u16 pixel = *buf;

        /* Zero out the red channel (bits 15:11) */
        pixel &= 0x07FF;  /* Clear bits 15:11, keep green (10:5) and blue (4:0) */

        *buf = pixel;
        buf++;
    }

    /* Ensure all writes are visible before VBlank swap */
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
}

bool framebuffer_swap(u16* src)
{
    if (!g_front_buffer || !src) {
        return false;
    }

    /*
     * Wait for VBlank before swapping to avoid tearing.
     * gspWaitForVBlank() blocks until the next vertical blank interval.
     */
    gspWaitForVBlank();

    /* Copy processed frame to front buffer */
    memcpy(g_front_buffer, src, g_back_buffer_size);

    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");

    return true;
}

/*---------------------------------------------------------------------------
 * Double Buffer Management
 *---------------------------------------------------------------------------*/

u16* framebuffer_get_back_buffer(void)
{
    return g_back_buffer;
}

u16* framebuffer_get_front_buffer(void)
{
    return g_front_buffer;
}
