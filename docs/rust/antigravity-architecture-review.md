# UI Architecture Evaluation & Framework Breakdown for LG webOS

**Target Hardware:** LG 32LK628BPTF (webOS TV Lite 4.4.3, ARMv7, 1 GB RAM)  
**Hardware Context:** Linux kernel 4.4.84. Available free system RAM on boot is approximately **~250–300 MB** (webOS system services and compositor consume ~650–700 MB; the hardware video pipeline consumes ~60–80 MB during active playback).

---

## 1. UI Architecture Evaluation on LG webOS

Any UI framework targeting webOS must satisfy three non-negotiable constraints:

1. **The Transparent Video Cutout (`gfx_furo`):**  
   Hardware video decodes onto a dedicated hardware plane that lives **behind** the application window. The UI framebuffer must support rendering an alpha-transparent hole (`glClearColor(0,0,0,0)`) so the video shines through without software composition overhead.
2. **D-pad Directional Focus Navigation:**  
   Focus must navigate cleanly across movie cards, carousels, and the TV guide using Magic Remote D-pad events (directional graph navigation).
3. **OpenGL ES 2.0 / EGL Support:**  
   The TV SoC only supports **OpenGL ES 2.0 / EGL**. Modern desktop frameworks that mandate Vulkan or WebGPU (such as `Iced` with WGPU) cannot run on this TV.

---

### Realistic Full-App RAM Budget Breakdown

When evaluating RAM usage on a 1 GB TV, framework idle baseline alone is misleading. A production TV streaming media center requires substantial memory for active assets:

* **Poster & Backdrop Texture Cache:**  
  * Each 1080p hero backdrop (1920×1080 RGBA) takes **~8.3 MB** uncompressed in GPU RAM.
  * 40–60 poster cards (300×450 RGBA) cached for smooth 60 FPS scrolling take **~25–35 MB**.
  * UI icons, badges, and sprite sheets take **~5 MB**.  
  * *Total Image & Texture RAM needed:* **~40–60 MB**.
* **EPG & Catalog Cache:** Live TV schedule for hundreds of channels, descriptions, and metadata take **~10–15 MB**.
* **Font Glyph Atlas & Subtitle Cache:** FreeType / HarfBuzz multi-language font atlas takes **~6–10 MB**.
* **Network & HTTP Response Buffers:** Stream links, JSON parsing, and Debrid connections take **~5–8 MB**.

---

### Framework Comparison Table (Full-App Load)

| Architecture | Framework Baseline | Texture & Asset Cache | **Total Full-App RAM** | UI Dev Speed | Remote & Hole Punching | Feasibility on 1 GB TV |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Rust Core + Existing C UI** | ~8 MB | ~45–55 MB | **~55–70 MB** | **Instant (Zero UI work)** | **Already 100% working** | 🏆 **Safest & Most Efficient** |
| **Raylib (`raylib-rs`)** | ~5 MB | ~45–55 MB | **~50–65 MB** | Moderate (Game loop math) | Complete native control | ✅ **Ultra-Lightweight** |
| **Slint + SDL2 Platform** | ~10 MB | ~50–60 MB | **~65–80 MB** | Fast (Declarative `.slint`) | Excellent (via SDL2 backend) | ✅ **Best Full Native Rewrite** |
| **SDL2 + `glow` (Low-Level)** | ~6 MB | ~45–55 MB | **~55–70 MB** | Slow (Manual C-style math) | Rock solid | ✅ **Complete Control** |
| **Svelte / Web UI + Rust Core** | ~85–110 MB | ~55–70 MB | **~150–200+ MB** | **Fastest (CSS & HTML)** | Native webOS support | ⚠️ **High OOM Risk (Borderline)** |
| **`egui`** | ~12 MB | ~50–60 MB | **~70–85 MB** | Fast | Poor (Lacks TV carousel UX)| ❌ Unsuitable for consumer TV |
| **`Iced`** | N/A | N/A | **N/A** | N/A | Incompatible (Needs Vulkan) | ❌ Incompatible with webOS 4 |

---

## 2. Detailed Breakdown of the Top Approaches

---

### Approach A: The Pragmatic Hybrid (Rust Core + Existing C UI)
* **Concept:** Keep the battle-tested C rendering and UI layer (`home.c`, `detail.c`, `guia.c`, `gfx.c`, `tex_cache.c`), and extract all networking, scrapers, data models, and business logic into a compiled Rust static library (`libnuvio_core.a`).
* **Why it works:**  
  * The existing C UI already handles Magic Remote pointer physics, D-pad navigation, spring physics, image texture budgeting, and the hardware video punch-through hole.
  * Rewriting 47,000 lines of UI coordinate math from C to Rust reproduces work that is already functional and tested.
* **Full-App Memory Footprint:**  
  * Rust Core (models, scrapers, EPG): ~8–12 MB  
  * Texture Cache (posters, backdrops, icons): ~45–55 MB  
  * UI state & fonts: ~8 MB  
  * **Total: ~60–75 MB.** (Leaves a generous ~180–220 MB free memory cushion on the TV).
* **Rust's Role:**  
  * Stremio add-on client & manifest parsing (`serde_json`).
  * RealDebrid / TorBox / Streams resolvers.
  * XMLTV EPG parsing.
  * Trakt / Simkl sync.
* **C's Role:** Simply queries the Rust library via C FFI and draws the cards on screen using the existing OpenGL ES 2.0 shaders.

---

### Approach B: Slint UI with SDL2 Platform Backend (Full Rust Rewrite)
* **Concept:** Rewrite both logic and UI in Rust using **Slint** for declarative UI layout, backed by **SDL2** for webOS windowing and input handling.
* **Why it works:**  
  * Slint was built by former Qt/QML creators specifically for embedded devices with constrained RAM.
  * You write UI in clean `.slint` markup (similar to QML/CSS) instead of manual coordinate math:
    ```slint
    export component MovieCard inherits Rectangle {
        in property <image> poster;
        in property <string> title;
        width: 160px; height: 240px;
        Image { source: root.poster; }
    }
    ```
  * Slint supports transparent backgrounds out-of-the-box for hardware video punch-through.
  * To bypass webOS's proprietary Wayland shell, Slint’s pluggable `slint::platform::Platform` trait is implemented on top of the TV's native `libSDL2.so`. SDL2 handles the Magic Remote and window creation; Slint renders directly to the OpenGL ES 2.0 surface.
* **Full-App Memory Footprint:**  
  * Slint Engine + Rust Core: ~12–15 MB  
  * Slint Image Cache & Textures: ~50–60 MB  
  * EPG & data models: ~10 MB  
  * **Total: ~70–85 MB.** (Well within the safe operating threshold of the TV).

---

### Approach C: Raylib (`raylib-rs`) — 2D Game Loop Architecture
* **Concept:** A TV media center behaves very much like a 2D game (scrolling card planes, sprite textures, remote control input loops). Raylib provides an ultra-minimalist C/Rust 2D game engine that runs natively over OpenGL ES 2.0.
* **Why it works:**  
  * Bare-metal performance with zero framework overhead.
  * Full control over transparent backgrounds (`clear_background(Color::BLANK)`).
  * Direct 60 FPS rendering loop with zero micro-stutters.
* **Full-App Memory Footprint:**  
  * Engine + Rust Core: ~6–8 MB  
  * Textures & Poster Cache: ~45–55 MB  
  * **Total: ~55–68 MB.** (Extremely lean).
* **Tradeoff:** You must write your own virtual keyboard for text search and manage the D-pad focus state graph manually.

---

### Approach D: Svelte / Web App + Local Rust Core Daemon
* **Concept:** Run a lightweight local Rust binary on `127.0.0.1` that talks to `luna://com.webos.media`, while the UI runs as a standard webOS web app written in **Svelte** or **Enact.js** (LG's official TV framework).
* **Why it works:** 95% of commercial TV apps (Netflix, YouTube, Stremio Web) use this model. Modern CSS Flexbox and Grid make designing movie carousels trivially fast.
* **Full-App Memory Footprint (The Catch on 1 GB TV):**  
  * WebOS Chromium Web App Manager (`webapp-mgr`): ~85–110 MB  
  * DOM tree & JS runtime: ~20–30 MB  
  * Decoded image textures in Chromium GPU process: ~45–60 MB  
  * Local Rust daemon: ~10 MB  
  * **Total: ~160–210 MB.**  
* **Risk:** On a TV with only ~250 MB free RAM, running near ~200 MB during heavy 1080p playback brings the app dangerously close to the Linux Low Memory Killer threshold (`lmkd`), increasing the risk of sudden app crashes during rapid browsing.
