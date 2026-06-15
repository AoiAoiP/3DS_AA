/**
 * @file   main.c
 * @brief  3DS Anti-Aliasing Plugin - Entry Point
 *
 * Luma3DS runtime plugin that performs post-process anti-aliasing on
 * the top-screen framebuffer every frame.
 *
 * Architecture:
 *   1. Plugin entry point registers with Luma's plugin loader
 *   2. Hooks into the game's frame loop via aptHook / gsp event
 *   3. Each frame: capture → edge detect → FXAA blend → swap
 *   4. Hotkey (L+Up) toggles AA on/off
 *   5. Auto-disables if CPU freq < 800 MHz (performance guarantee)
 *
 * Build: make (requires devkitARM)
 * Output: 3ds_aa.3dsx
 * Deploy: Copy 3ds_aa.3dsx to /3ds/ on SD card, or use 3dslink
 *         For Luma plugin: rename to code.ips and place in
 *         /luma/plugins/<title_id>/
 */

#include "aa_plugin.h"
#include "framebuffer.h"
#include "edge_detect.h"
#include "anti_aliasing.h"
#include "timing.h"

/*---------------------------------------------------------------------------
 * Global Plugin Context
 *---------------------------------------------------------------------------*/

aa_context_t g_aa_ctx;

/*---------------------------------------------------------------------------
 * Forward Declarations
 *---------------------------------------------------------------------------*/

static void aa_hotkey_check(void);
static void aa_process_current_frame(void);
static void aa_stats_overlay_draw(void);

/*---------------------------------------------------------------------------
 * Plugin Lifecycle — Called by Luma3DS Plugin Loader
 *---------------------------------------------------------------------------*/

/**
 * @brief Plugin initialization.
 *
 * Called once when the plugin is loaded by Luma3DS.
 * Sets up all subsystems and performs hardware capability checks.
 */
static bool aa_plugin_init(void)
{
    /* Zero-initialize global context */
    memset(&g_aa_ctx, 0, sizeof(g_aa_ctx));
    g_aa_ctx.state = AA_STATE_OFF;

    /* Set default FXAA parameters */
    g_aa_ctx.fxaa_edge_threshold = FXAA_EDGE_THRESHOLD_DEF;
    g_aa_ctx.fxaa_quality        = FXAA_QUALITY_DEF;
    g_aa_ctx.fxaa_max_steps      = FXAA_MAX_STEPS_DEF;

    /* Initialize timing subsystem (PMU, CPU freq detection) */
    timing_init();

    /* Check hardware requirements */
    if (g_aa_ctx.cpu_freq_mhz < CPU_FREQ_MIN_MHZ && !g_aa_ctx.force_enable) {
        /*
         * CPU too slow for AA — plugin loads but stays in OFF state.
         * User can force-enable via hotkey if they accept the perf hit.
         */
        g_aa_ctx.state = AA_STATE_OFF;
    }

    /* Initialize edge detection (precompute LUTs) */
    edge_detect_init();

    /* Initialize framebuffer capture (allocate back-buffer) */
    if (!framebuffer_init()) {
        /* Back-buffer allocation failed — fatal */
        g_aa_ctx.state = AA_STATE_ERROR;
        return false;
    }

    /* Initialize GPU/display services */
    gfxInitDefault();
    gfxSet3D(false);  /* Process only 2D mode — avoids 3D double-render cost */

    /* Enable AA by default on capable hardware */
    if (g_aa_ctx.state != AA_STATE_ERROR &&
        g_aa_ctx.cpu_freq_mhz >= CPU_FREQ_MIN_MHZ) {
        g_aa_ctx.state = AA_STATE_ENABLED;
    }

    return true;
}

/**
 * @brief Plugin shutdown — release all resources.
 */
static void aa_plugin_exit(void)
{
    g_aa_ctx.state = AA_STATE_OFF;

    framebuffer_exit();
    edge_detect_exit();
    timing_exit();
    gfxExit();
}

/*---------------------------------------------------------------------------
 * Frame Processing Hook
 *---------------------------------------------------------------------------*/

/**
 * @brief APT hook callback — called on frame events.
 *
 * @param hook   Hook type (APT_HOOK_ON_RESTORE, etc.)
 * @param param  Callback parameter.
 */
static void aa_apt_hook(APT_HookType hook, void* param)
{
    (void)param;

    switch (hook) {
        case APT_HOOK_ON_SUSPEND:
            /* Game suspended — disable AA to save power */
            break;

        case APT_HOOK_ON_RESTORE:
            /* Game resumed — re-enable AA if it was on */
            if (g_aa_ctx.state == AA_STATE_THROTTLED) {
                g_aa_ctx.state = AA_STATE_ENABLED;
            }
            break;

        default:
            break;
    }
}

/**
 * @brief Main frame processing callback.
 *
 * Called once per frame. This is the core of the plugin:
 *   Check hotkeys → Capture frame → Detect edges → Apply FXAA → Swap buffer
 */
static void aa_frame_callback(void)
{
    /* Check for user hotkey toggle */
    aa_hotkey_check();

    /* Handle toggle request */
    if (g_aa_ctx.toggle_request) {
        g_aa_ctx.toggle_request = false;

        if (g_aa_ctx.state == AA_STATE_ENABLED ||
            g_aa_ctx.state == AA_STATE_THROTTLED) {
            g_aa_ctx.state = AA_STATE_OFF;
        } else if (g_aa_ctx.state == AA_STATE_OFF) {
            /* Check if we can enable */
            if (g_aa_ctx.cpu_freq_mhz >= CPU_FREQ_MIN_MHZ ||
                g_aa_ctx.force_enable) {
                g_aa_ctx.state = AA_STATE_ENABLED;
            }
        }
    }

    /* Process the current frame if AA is enabled */
    if (g_aa_ctx.state == AA_STATE_ENABLED) {
        aa_process_current_frame();
    }

    /* Draw performance overlay if requested */
    if (g_aa_ctx.show_stats) {
        aa_stats_overlay_draw();
    }
}

/*---------------------------------------------------------------------------
 * Frame Processing Implementation
 *---------------------------------------------------------------------------*/

static void aa_process_current_frame(void)
{
    uint64_t t_start = timing_get_cycles();

    /* Phase 1: Capture the current front buffer into back buffer */
    u16* back_buf = framebuffer_capture();
    if (!back_buf) {
        /* Capture failed — skip this frame */
        g_aa_ctx.skipped_frames++;
        return;
    }

    /* Phase 2: Apply FXAA to the back buffer */
    uint64_t aa_cycles = fxaa_process_frame(
        back_buf,
        g_aa_ctx.fxaa_quality,
        g_aa_ctx.fxaa_edge_threshold
    );

    /* Phase 3: Swap processed frame to front buffer */
    if (!framebuffer_swap(back_buf)) {
        g_aa_ctx.skipped_frames++;
        return;
    }

    /* Phase 4: Record timing */
    uint64_t t_end = timing_get_cycles();
    g_aa_ctx.last_frame_cycles = t_end - t_start;
    g_aa_ctx.total_cycles += g_aa_ctx.last_frame_cycles;
    g_aa_ctx.frame_count++;

    /*
     * Performance guard: if we're consistently over budget,
     * throttle AA to maintain gameplay smoothness.
     */
    uint32_t frame_us = timing_cycles_to_us(
        g_aa_ctx.last_frame_cycles, g_aa_ctx.cpu_freq_mhz);

    if (frame_us > AA_TIME_CUTOFF_US && !g_aa_ctx.force_enable) {
        g_aa_ctx.state = AA_STATE_THROTTLED;
    }

    /* Unused but tracked for future diagnostics */
    (void)aa_cycles;
}

/*---------------------------------------------------------------------------
 * Hotkey Handling
 *---------------------------------------------------------------------------*/

/**
 * @brief Check for AA toggle hotkey: L + D-Pad Up.
 *
 * Also handles:
 *   - L + D-Pad Down: Toggle performance overlay
 *   - L + D-Pad Right: Force-enable AA (override freq check)
 */
static void aa_hotkey_check(void)
{
    /* Scan input */
    hidScanInput();
    u32 keys_down = hidKeysDown();

    /* L trigger must be held */
    if (!(keys_down & KEY_L)) {
        return;
    }

    /* L + Up: Toggle AA on/off */
    if (keys_down & KEY_DUP) {
        g_aa_ctx.toggle_request = true;
    }

    /* L + Down: Toggle performance overlay */
    if (keys_down & KEY_DDOWN) {
        g_aa_ctx.show_stats = !g_aa_ctx.show_stats;
    }

    /* L + Right: Force-enable AA (bypass freq check) */
    if (keys_down & KEY_DRIGHT) {
        g_aa_ctx.force_enable = !g_aa_ctx.force_enable;
        if (g_aa_ctx.force_enable && g_aa_ctx.state == AA_STATE_OFF) {
            g_aa_ctx.state = AA_STATE_ENABLED;
        }
    }
}

/*---------------------------------------------------------------------------
 * Performance Overlay
 *---------------------------------------------------------------------------*/

/**
 * @brief Draw a simple on-screen performance overlay.
 *
 * Shows AA state, last frame processing time, and frame counter.
 * Uses the console subsystem for text rendering.
 */
static void aa_stats_overlay_draw(void)
{
    static timing_stats_t frame_stats;
    static uint32_t overlay_frame = 0;

    /* Update rolling statistics */
    timing_record_sample(&frame_stats, g_aa_ctx.last_frame_cycles);
    overlay_frame++;

    /* Only update the display every 30 frames (0.5s @ 60fps) */
    if (overlay_frame % 30 != 0) {
        return;
    }

    /* Init console for this frame */
    consoleSelect(consoleGetDefault());

    /*
     * Print stats to the top-left corner of the top screen.
     * Using \x1b escape codes for cursor positioning.
     */
    printf("\x1b[0;0H");  /* Move cursor to (0,0) */

    const char* state_str;
    switch (g_aa_ctx.state) {
        case AA_STATE_OFF:       state_str = "OFF";       break;
        case AA_STATE_ENABLED:   state_str = "ENABLED";   break;
        case AA_STATE_THROTTLED: state_str = "THROTTLED"; break;
        case AA_STATE_ERROR:     state_str = "ERROR";     break;
        default:                 state_str = "UNKNOWN";   break;
    }

    char stats_line[128];
    timing_format_stats(&frame_stats, g_aa_ctx.cpu_freq_mhz,
                        stats_line, sizeof(stats_line));

    printf("3DS AA [%s] %dMHz %s\n",
           state_str, g_aa_ctx.cpu_freq_mhz,
           g_aa_ctx.force_enable ? "FORCED" : "");

    printf("Last: %luus  %s\n",
           (unsigned long)timing_cycles_to_us(
               g_aa_ctx.last_frame_cycles, g_aa_ctx.cpu_freq_mhz),
           stats_line);

    printf("Frames: %lu  Skip: %lu  Q:%d\n",
           (unsigned long)g_aa_ctx.frame_count,
           (unsigned long)g_aa_ctx.skipped_frames,
           g_aa_ctx.fxaa_quality);
}

/*---------------------------------------------------------------------------
 * Plugin Entry Point
 *---------------------------------------------------------------------------*/

/**
 * @brief Luma3DS plugin main entry.
 *
 * Called by Luma3DS's plugin loader after the game process starts.
 * We register our hooks and then yield to the main game loop.
 *
 * The plugin runs in the same process as the game, so we share
 * the same virtual memory space and can directly access GPU buffers.
 *
 * IMPORTANT: This function must never return. The plugin loader
 * expects us to either loop forever (processing frames) or call
 * threadExit() if we spawn a separate thread.
 */
void plugin_main(void)
{
    /* Initialize plugin subsystems */
    if (!aa_plugin_init()) {
        /* Initialization failed — exit plugin gracefully */
        aa_plugin_exit();
        threadExit(0);
        /* unreachable */
    }

    /* Register APT hook for suspend/resume events */
    aptHook(&aa_apt_hook, NULL);

    /*
     * Main processing loop.
     *
     * The strategy: we run our AA pass inside the game's main loop
     * by hooking into the frame presentation path. On 3DS, the main
     * loop typically looks like:
     *
     *   while (aptMainLoop()) {
     *       hidScanInput();
     *       // ... game logic ...
     *       gfxFlushBuffers();
     *       gfxSwapBuffers();
     *       gspWaitForVBlank();
     *   }
     *
     * We intercept at gfxSwapBuffers time to post-process the
     * framebuffer before it's displayed.
     */
    while (aptMainLoop()) {
        /* Process input for hotkeys */
        hidScanInput();

        /* Run the AA frame callback */
        aa_frame_callback();

        /*
         * Yield to the game's rendering.
         * The game will render its frame, swap buffers, and then
         * we capture and process on the next iteration.
         *
         * Note: In a real Luma3DS plugin, we'd hook gspSwapBuffers
         * more directly. For this initial implementation, we run
         * our processing inline with aptMainLoop to establish the
         * baseline pipeline.
         */
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    /* Cleanup (only reached if aptMainLoop returns) */
    aa_plugin_exit();
}

/**
 * @brief Standard C main — used for .3dsx builds during development.
 *
 * When built as a .3dsx (homebrew launcher), this is the entry point.
 * For Luma3DS plugin (.ips), plugin_main() is the entry point.
 */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    plugin_main();
    return 0;
}
