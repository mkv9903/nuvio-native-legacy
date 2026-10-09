// THE UNIFIED BUTTON TABLE (owner, 21/09/2026: "buttons and badge — every
// detail"). Up to now each modal drew its own pill with custom numbers:
// 64 px in avisos.c and atualizacao.c, 86 in ctxmenu.c, 72 in detail.c, 76 in
// Saved panel; rest at 0.176, 0.20 or 0.133 depending on file. The
// PRODUCT log calls for a single vocabulary — if "save" has two looks,
// one is wrong. This is the look.
//
//   PRIMARY    72 px height (Play dimension in detail.c), pill radius
//              (0.5), TXT_DET_BOTAO 25/500. Rest: surface 0.14/0.15/
//              0.17 (tinted panel gray, one step above it) with
//              text 235. Focus: highlight color, ink via ajustes_acento_tinta
//              (white except bright highlights) and diffuse glow GFX_SOMBRA 0.35
//              behind it — same light as sidebar menu pill.
//   SECONDARY  56 px, same radius and text body. Rest: WITHOUT core,
//              1.5 px outline at 22% white, text 235. Focus: same as
//              primary. Secondary is shorter AND lighter by design:
//              hierarchy comes from scale and weight, not a second color.
//   DISC       round button with icon only (three on title screen):
//              rest 0.14/0.15/0.17, focus highlight + ink + glow.
//
// `focus` is the SPRING (0..1), not a boolean: background interpolates, text swaps in
// STEP at 0.5 — color is part of text.c cache key and a color per
// frame would rasterize a new texture every frame (see ctxmenu.c).
//
// Icon is a name from deploy/app/art/icones (gfx_icone): "mais", "visto",
// "avancar", "recomendar", ... NULL = no icon. Sits to LEFT of label
// and pair (icon + label) is centered in pill.
#ifndef NV_BUTTONS_H
#define NV_BUTTONS_H
#include "gfx.h"

#define BUTTON_H_PRIMARY   72.0f
#define BUTTON_H_SECONDARY 56.0f
#define BUTTON_PAD_X       36.0f   // lateral padding for primary
#define BUTTON_PAD_X2      28.0f   // for secondary (shorter, less air)
#define BUTTON_ICON        26.0f   // icon size inside pill
#define BUTTON_ICON_GAP    14.0f   // gap from icon to label
#define BUTTON_GAP         16.0f   // between side-by-side buttons

// Width button_pill will occupy.
float button_width(const char *label, const char *icon, int primary);

// Draws pill in `r`. `align`: 0 centers pair in button, 1 left-aligns with BUTTON_PAD_X.
void button_pill(GfxRect r, const char *label, const char *icon,
                 float focus, int primary, int align, float a);

// Surface of primary only. Returns text color 0..255 for this focus.
int  button_surface(GfxRect r, float focus, float a);

// Round button with icon only. `r` is disc square.
void button_disc(GfxRect r, const char *icon, float focus, float a);

// Focus color of every button.
float button_focus_color(float *r, float *g, float *b);

// Diffuse glow behind focused button.
void button_glow(GfxRect r, float focus, float a);

// Backward-compatibility shims
#define BOTAO_H_PRIMARIO   BUTTON_H_PRIMARY
#define BOTAO_H_SECUNDARIO BUTTON_H_SECONDARY
#define BOTAO_PAD_X        BUTTON_PAD_X
#define BOTAO_PAD_X2       BUTTON_PAD_X2
#define BOTAO_ICONE        BUTTON_ICON
#define BOTAO_ICONE_GAP    BUTTON_ICON_GAP
#define BOTAO_GAP          BUTTON_GAP

float botao_largura(const char *rotulo, const char *icone, int primario);
void  botao_pilula(GfxRect r, const char *rotulo, const char *icone,
                   float foco, int primario, int alinhar, float a);
int   botao_superficie(GfxRect r, float foco, float a);
void  botao_disco(GfxRect r, const char *icone, float foco, float a);
float botao_cor_foco(float *r, float *g, float *b);
void  botao_luz(GfxRect r, float foco, float a);

#endif
