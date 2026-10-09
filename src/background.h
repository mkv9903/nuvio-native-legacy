// BACKGROUND BEHIND PANELS (Settings › Appearance › Background, Settings v2).
//
// A single module for drawing the full-screen backdrop with title artwork:
// Settings (and screens using ajustes_ui_fundo) and, after merge,
// the title page (detail.c). The mode is this TV's LOCAL choice:
//   BACKGROUND_ART     (0, default) crisp art in "cover" with mockup dark veil
//                      (linear 90deg: 62%, 40% center, 50%), dithered via nv_dither;
//   BACKGROUND_BLURRED (1) BLURRED art: the 96x54 copy from gfx_desfocado
//                      baked in "cover" onto a 320x180 buffer and scaled up, with
//                      a 28% veil. No per-frame blur; baking only reruns when
//                      artwork changes. Without artwork yet, plain background;
//   BACKGROUND_FROST   (2) without art: cool gray with three broad lights in
//                      the accent color from ajustes_acento() (58%, 38%, 14%),
//                      all via GFX_VEU_CSS and GFX_LUZ (nv_dither: 8-bit panel won't band).
#ifndef NV_BACKGROUND_H
#define NV_BACKGROUND_H
#include "gfx.h"

#define BACKGROUND_ART     0
#define BACKGROUND_BLURRED 1
#define BACKGROUND_FROST   2

// The mode selected on this TV (ajustes_fundo).
int  background_mode(void);
// Paints the background in chosen mode in `area` (layout coordinates; full
// screen is normal case), with artwork `artUrl` (same key as texture cache;
// NULL or "" = no artwork, plain background color and veil) and alpha `a`.
void background_draw(GfxRect area, const char *artUrl, float a);
// Same, in requested mode (previews in Settings › Background). `radiusPx` rounds
// corners (0 in full screen); outside full screen Blurred stretches blurred
// copy on rectangle without full-screen baking.
void background_draw_mode(int mode, GfxRect area, float radiusPx, const char *artUrl, float a);

// Once per frame (main.c, after gfx_ambiente_preparar): with "Frosted glass"
// on and no immersive light, bakes blurred artwork of SCENE TITLE for glass
// (gfx_vidro_fosco) to draw inside panels. Nothing if no scene artwork.
// Re-runs only when palette changes.
void background_frosted_frame(void);

// Dark veil of full-screen Blurred for subsequent drawings (0..1; < 0 returns
// to standard 28%). Title page uses higher: text sits over it.
void background_blurred_veil(float strength);

// One-time verification of full-screen background `mode` (BACKGROUND_BLURRED or
// BACKGROUND_FROST, background.c): -1 not yet, 1 screen pixel matched CPU calculation,
// 0 mismatch (background falls back to direct drawing for session).
int background_check(int mode);

// Backward-compatibility shims
#define FUNDO_ARTE    BACKGROUND_ART
#define FUNDO_BORRADA BACKGROUND_BLURRED
#define FUNDO_FROST   BACKGROUND_FROST

int  fundo_modo(void);
void fundo_desenhar(GfxRect area, const char *arteUrl, float a);
void fundo_desenhar_modo(int modo, GfxRect area, float raioPx, const char *arteUrl, float a);
void fundo_fosco_quadro(void);
void fundo_borrada_veu(float forca);
int  fundo_conferencia(int modo);

#endif
