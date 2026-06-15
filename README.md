# 3DS Anti-Aliasing Plugin

[![zh-CN](https://img.shields.io/badge/lang-简体中文-red.svg)](README_cn.md)

**Luma3DS runtime plugin** that applies post-process FXAA (Fast Approximate Anti-Aliasing) to the top screen of New 3DS XL, reducing geometric aliasing caused by the 400×240 low-resolution display.

## Features

- **Real-time FXAA** — luminance-based edge detection + directional blending
- **Fixed-point arithmetic** — no FPU required, optimized for ARM11
- **Configurable quality** — 4 presets (0=fastest → 3=best)
- **Double-buffered** — no tearing, ≤1 frame input lag
- **Hotkey control** — `L + D-Pad Up` to toggle AA on/off
- **Performance overlay** — `L + D-Pad Down` to show frame timing stats
- **Auto-throttling** — disables AA if frame time exceeds 4ms budget
- **Force mode** — `L + D-Pad Right` to force-enable on slower hardware

## Performance Targets

| Metric | Target | Status |
|--------|--------|--------|
| Frame time | ≤ 2.5ms (avg) | 🚧 Awaiting hardware testing |
| Frame rate impact | ≤ 10% | 🚧 Awaiting hardware testing |
| Memory overhead | ≤ 2MB system RAM | ✅ (~375KB) |
| Input lag | ≤ 1 frame | ✅ (double-buffered) |
| Compilation | Clean build | ✅ |

## Requirements

### Hardware
- **New 3DS XL** (recommended: 804MHz CPU + 2MB L2 cache)
- Old 3DS works but AA will auto-disable below 800MHz

### Software (on 3DS)
- **Luma3DS v13.1+** (with plugin loader support)

### Build Environment
- **Docker Desktop** (Windows / macOS / Linux)
- **devkitPro Docker image**: `devkitpro/devkitarm`

## Building

```bash
# Clean build
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd)":/app -w /app devkitpro/devkitarm make clean

# Build
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd)":/app -w /app devkitpro/devkitarm make
```

> **Note for Git Bash users:** The `MSYS_NO_PATHCONV=1` prefix is required to prevent Git Bash from mangling Docker volume paths. On Linux/macOS or PowerShell, omit it.

Outputs:
- `3ds_aa.3dsx` — Homebrew Launcher format (for development/testing)
- `3ds_aa.elf` — ELF binary (for debugging)

## Deployment

### As a Luma3DS Plugin
```bash
# Rename and copy to SD card (replace <title_id> with the game's title ID):
cp 3ds_aa.3dsx /luma/plugins/<title_id>/code.ips
```
The plugin will auto-load when the game starts.

### For Development / Testing
```bash
# Copy to SD card for Homebrew Launcher:
cp 3ds_aa.3dsx /3ds/

# Or deploy wirelessly via 3dslink (3DS must be in hbmenu network receive mode):
3dslink 3ds_aa.3dsx -a <3DS_IP_ADDRESS>
```

## Project Structure

```
3DS_AA/
├── source/
│   ├── main.c           # Plugin entry point, frame hook, hotkeys
│   ├── framebuffer.c    # Screen capture & RGB565 manipulation
│   ├── edge_detect.c    # Sobel edge detection on luminance
│   ├── anti_aliasing.c  # FXAA core (fixed-point)
│   └── timing.c         # PMU cycle counter & CPU freq detection
├── include/
│   ├── aa_plugin.h      # Master header: config, types, macros
│   ├── framebuffer.h    # Framebuffer API
│   ├── edge_detect.h    # Edge detection API
│   ├── anti_aliasing.h  # FXAA API
│   └── timing.h         # Timing & performance API
├── Makefile             # devkitPro build system (Docker-based)
├── README.md
├── Roadmap.md           # Full development plan
└── Install_instruction.md
```

## Hotkeys

| Combo | Action |
|-------|--------|
| `L + D-Pad Up` | Toggle AA on/off |
| `L + D-Pad Down` | Toggle performance overlay |
| `L + D-Pad Right` | Force-enable AA (bypass frequency check) |

## How It Works

1. **Frame Capture** — Each frame, the top-screen RGB565 framebuffer (400×240) is copied to a back buffer via `gfxGetFramebuffer()`.

2. **Luminance Conversion** — RGB565 pixels are converted to 8-bit luminance using a precomputed 64K-entry lookup table (ITU-R BT.601 coefficients).

3. **Edge Detection** — A 3×3 Sobel operator computes gradient magnitude and direction for each pixel. Pixels exceeding the contrast threshold are classified as horizontal, vertical, or diagonal edges.

4. **FXAA Blending** — For each edge pixel:
   - The local contrast range is computed from the cross-shaped neighborhood
   - Edge direction determines which pixel pair to blend across
   - A blend factor (0–255) controls how much anti-aliasing is applied
   - Sub-pixel AA (quality preset 3) additionally smooths thin features

5. **Buffer Swap** — The processed back buffer is copied to the front buffer during VBlank to prevent tearing.

## Algorithm Reference

- FXAA 3.11 by Timothy Lottes (NVIDIA, 2009)
- ITU-R BT.601 luminance coefficients

## Status

🚧 **Alpha** — Compilation verified, awaiting hardware testing.

- [x] Stage 0: Environment verification (Docker-based build compiles cleanly)
- [ ] Stage 1: Framebuffer capture & color channel manipulation (needs 3DS hardware)
- [ ] Stage 2: FXAA algorithm port & optimization (code complete, needs profiling)
- [ ] Stage 3: Double buffering & performance tuning (code complete, needs profiling)
- [ ] Stage 4: Hotkey control & compatibility testing (code complete, needs hardware)

## License

MIT
