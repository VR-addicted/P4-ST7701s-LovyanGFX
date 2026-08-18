# Project Constraints & Rules

## Core Framework
- **Framework:** Pure ESP-IDF (NO PlatformIO, NO Arduino framework).
- **Build System:** CMake / `idf.py`.
- **Target Chip:** ESP32-P4.
- **Design Philosophy:** Minimal abstraction layers. Avoid unnecessary wrapper libraries, Arduino dependencies, or extra framework layers. Keep everything as native, lightweight, and performant as possible in pure ESP-IDF C/C++.

## Guidelines
- Do NOT generate `platformio.ini` or suggest PlatformIO commands.
- Do NOT import Arduino headers (`Arduino.h`) or Arduino-specific libraries.
- All build, flash, and monitor workflows MUST use standard ESP-IDF commands (e.g., `idf.py build`, `idf.py flash`).
- Keep the code lightweight, modular, and native to ESP-IDF drivers to avoid unnecessary overhead.
- **Preserve Comments:** NEVER remove, modify, or strip existing comments, inline notes, or documentation in the code.
- **Preserve Optical Formatting:** Respect and maintain exact user code layout, alignment, custom spacing, and visual formatting. Do NOT apply reformatting or re-indentation that alters user layout.

## Storage & Filesystem
- **Partition:** `storage` (9MB).
- **Filesystem:** LittleFS (`esp_littlefs`). Always use `littlefs_create_partition_image(storage data FLASH_IN_PROJECT)` in `CMakeLists.txt`. Do NOT use SPIFFS.

## Hardware & Peripherals
- **Display:** ST7701 MIPI DSI display (480x800) initialized with `LovyanGFX` (`LGFX_ST7701_P4`).
- **LovyanGFX Sprite Color Depth:** Framebuffer and Sprites use 24-bit color depth (RGB888). When initializing any `lgfx::LGFX_Sprite`, ALWAYS call `sprite.setPsram(true);` and `sprite.createSprite(width, height);` BEFORE `sprite.setColorDepth(24);` because `setColorDepth` returns early if `getBuffer()` is `nullptr`.
- **RGB LED Strip:** WS28xx strip on **GPIO 29** (50 LEDs), driven via ESP-IDF `led_strip` RMT driver (`espressif/led_strip`).

## Performance & Code Optimization Rules
- **Zero Bottlenecks in Critical Paths:** Every frame, loop, and interrupt path must be engineered for minimum CPU cycle consumption to maximize available compute for game logic and controllers.
- **Avoid Unnecessary API Calls in Loops:** Never make redundant driver, OS, or library API calls inside tight loops or timetrap routines. Cache states and reuse persistent global variables where applicable.
- **Inline Code in Loops:** Use `static inline` functions and direct scanline/memory pointers (`memcpy`, pointer arithmetic) instead of deep abstraction wrappers, virtual function dispatches, or per-pixel library callbacks.
- **Precalculated Look-Up Tables (LUTs):** Always precompute coordinate trajectories, curves (sine, easing, bounce), and mathematical functions into LUT arrays in RAM at startup instead of evaluating floating-point arithmetic or trigonometric functions in real-time loops.
- **Persistent RAM Assets:** Keep persistent basis assets (sprites, canvases, background buffers) in PSRAM once loaded rather than repeatedly decoding or reading from flash/LittleFS.
- **Direct DMA/Double-Buffering:** Always compose frames completely offscreen in PSRAM before performing a single burst transfer to the display framebuffer via DMA/`esp_cache_msync`.
