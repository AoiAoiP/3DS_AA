# 3DS Anti-Aliasing Plugin

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
| Frame time | ≤ 2.5ms (avg) | 🚧 |
| Frame rate impact | ≤ 10% | 🚧 |
| Memory overhead | ≤ 2MB system RAM | ✅ (~375KB) |
| Input lag | ≤ 1 frame | ✅ (double-buffered) |

## Requirements

### Hardware
- **New 3DS XL** (recommended: 804MHz CPU + 2MB L2 cache)
- Old 3DS works but AA will auto-disable below 800MHz

### Software
- **Luma3DS v13.1+** (with plugin loader support)
- **devkitARM r52+** (for building)

### Build Dependencies
- [devkitPro](https://devkitpro.org/) with `3ds-dev` package
- `libctru` ≥ v2.4.0

## Building

```bash
# In devkitPro MSYS2 shell:
cd 3ds_aa
make clean
make
```

Outputs:
- `3ds_aa.3dsx` — Homebrew Launcher format (for development/testing)
- `3ds_aa.elf` — ELF binary (for debugging)

## Deployment

### As a Luma3DS Plugin
```bash
# Copy to SD card (replace <title_id> with the game's title ID):
cp 3ds_aa.3dsx /luma/plugins/<title_id>/code.ips
```

### For Development / Testing
```bash
# Deploy wirelessly via 3dslink:
make run
```

## Project Structure

```
3ds_aa/
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
├── build/               # Compilation output
├── Makefile             # devkitPro build system
├── Roadmap.md           # Full development plan
└── README.md
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

🚧 **Alpha** — Under active development per the [Roadmap](Roadmap.md).

- [ ] Stage 0: Environment verification (build & deploy helloworld)
- [ ] Stage 1: Framebuffer capture & color channel manipulation
- [ ] Stage 2: FXAA algorithm port & optimization
- [ ] Stage 3: Double buffering & performance tuning
- [ ] Stage 4: Hotkey control & compatibility testing

## License

MIT
