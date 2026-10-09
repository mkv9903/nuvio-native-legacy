#include "buttons.h"
#include "ajustes.h"
#include "anim.h"
#include "text.h"

// Rest surface of primary: floating panel gray (0.055/0.058/0.068) one step above,
// tinted towards blue same way. Never #000 or pure neutral gray — over translucent
// panel pure neutral appears greenish.
#define BTN_REST_R    0.14f
#define BTN_REST_G    0.15f
#define BTN_REST_B    0.17f
// Text at rest: 235, same as badges (badges.h). 255 over 0.15 vibrates.
#define BTN_TEXT_REST 235

// FOCUS COLOR (owner, 29/09/2026): DARKENED accent of movie player,
// not full accent. Was previously corFocoPlayer calculation (player.c),
// which now calls this: focus of movie circular buttons and live pills
// and rest of app share same color. Accent goes 74% from dark panel
// (0.055/0.058/0.068); white and near-white go 88% to avoid turning gray.
// INK continues owner's 21/09 rule (ajustes_acento_tinta): only white
// or near-white accent takes dark text. Darkening only helps white text
// on color; white darkened to 88% stays at ~0.89 luminance, and dark text
// on it maintains plenty of contrast.
//
// ACCENTS OF 03/10/2026 (acentos-mockup.html): focus color is ACTUAL
// accent, without darkening. New accents were already measured for this — deep
// tones are at white limit 4.6:1 and light tones take dark ink.
float button_focus_color(float *r, float *g, float *b) {
  return ajustes_acento_tinta(r, g, b);
}

// Focus on GLASS: same filled pill as gfx_vidro_pilula_cheia, but in
// color of button_focus_color — with or without glass, focused button is same color.
static void glass_focus(GfxRect r, float focus, float a) {
  float fr, fg, fb, f = focus < 0.0f ? 0.0f : (focus > 1.0f ? 1.0f : focus);
  if (f <= 0.01f || a <= 0.001f) return;
  button_focus_color(&fr, &fg, &fb);
  gfx_cor(r, 0.5f, fr, fg, fb, f * a);
}

void button_glow(GfxRect r, float focus, float a) {
  float fr, fg, fb;
  GfxRect glow;
  if (focus < 0.01f) return;
  // Glass: focus is white pill, no colored glow behind.
  if (ajustes_vidro()) return;
  button_focus_color(&fr, &fg, &fb);
  // 0.9x height clearance on each side and alpha 0.35 x spring: sidebar menu pill glow.
  glow.x = r.x - r.h * 0.9f; glow.y = r.y - r.h * 0.9f;
  glow.w = r.w + r.h * 1.8f; glow.h = r.h * 2.8f;
  gfx_rect(glow, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, fr, fg, fb, 0.35f * focus * a);
}

float button_width(const char *label, const char *icon, int primary) {
  TxtLinha l = txt_linha(TXT_DET_BOTAO, label, 0, 0, 0, 255);
  float w = (float)l.w + (primary ? BUTTON_PAD_X : BUTTON_PAD_X2) * 2.0f;
  if (icon && icon[0]) w += BUTTON_ICON + BUTTON_ICON_GAP;
  return w;
}

int button_surface(GfxRect r, float focus, float a) {
  float fr, fg, fb, ti = button_focus_color(&fr, &fg, &fb);
  if (ajustes_vidro()) {
    // Primary action: at rest glass carries highlight wash and rim (theme stays visible);
    // in focus, FILLED pill in highlight.
    gfx_vidro_painel_acento(r, 0.5f, 0.55f, a);
    glass_focus(r, focus, a);
    return gfx_vidro_tinta(focus);
  }
  button_glow(r, focus, a);
  gfx_cor(r, 0.5f, anim_mistura(BTN_REST_R, fr, focus),
          anim_mistura(BTN_REST_G, fg, focus), anim_mistura(BTN_REST_B, fb, focus), a);
  return focus >= 0.5f ? (int)(ti * 255.0f + 0.5f) : BTN_TEXT_REST;
}

void button_pill(GfxRect r, const char *label, const char *icon,
                 float focus, int primary, int align, float a) {
  float fr, fg, fb, ti = button_focus_color(&fr, &fg, &fb);
  int is_focused = focus >= 0.5f;
  int c = is_focused ? (int)(ti * 255.0f + 0.5f) : BTN_TEXT_REST;
  int glass = ajustes_vidro();
  float radius = 0.5f;
  TxtLinha l;
  float has_icon = (icon && icon[0]) ? 1.0f : 0.0f;
  float group, x0, padx = primary ? BUTTON_PAD_X : BUTTON_PAD_X2;

  if (glass) {
    if (primary) c = button_surface(r, focus, a);
    else {
      gfx_vidro_painel(r, 0.5f, 0.55f, a);
      glass_focus(r, focus, a);
      c = gfx_vidro_tinta(focus);
    }
  } else if (primary) {
    button_surface(r, focus, a);
  } else {
    button_glow(r, focus, a);
    // Core only exists according to focus; at rest it is outline. Ring thickness
    // is fraction of HEIGHT: 1.5 px in 56 px.
    // (23/09) At rest core is also present: dark surface of neighboring circulars (BTN_REST),
    // slightly translucent.
    gfx_cor(r, radius, anim_mistura(BTN_REST_R, fr, focus),
            anim_mistura(BTN_REST_G, fg, focus), anim_mistura(BTN_REST_B, fb, focus),
            (0.82f + 0.18f * focus) * a);
    if (focus < 0.99f)
      gfx_anel(r, radius, 1.5f, 1, 1, 1, 0.22f * (1.0f - focus) * a);
  }

  l = txt_linha(TXT_DET_BOTAO, label, c, c, c, 255);
  group = (float)l.w + has_icon * (BUTTON_ICON + BUTTON_ICON_GAP);
  x0 = align ? r.x + padx : r.x + (r.w - group) * 0.5f;
  if (has_icon > 0.0f) {
    float ic = (float)c / 255.0f;
    GfxRect ig = { x0, r.y + (r.h - BUTTON_ICON) * 0.5f, BUTTON_ICON, BUTTON_ICON };
    gfx_icone(ig, icon, ic, ic, ic, a);
    x0 += BUTTON_ICON + BUTTON_ICON_GAP;
  }
  txt_desenhar_alpha(l, x0, r.y + (r.h - (float)l.h) * 0.5f, a);
}

void button_disc(GfxRect r, const char *icon, float focus, float a) {
  float fr, fg, fb, ti = button_focus_color(&fr, &fg, &fb);
  float ic = focus >= 0.5f ? ti : 1.0f;
  int glass = ajustes_vidro();
  // Glyph measures one third of disc — proportion measured on device.
  float g = r.w * 0.333f;
  GfxRect ig = { r.x + (r.w - g) * 0.5f, r.y + (r.h - g) * 0.5f, g, g };
  if (glass) {
    ic = focus >= 0.5f ? (float)gfx_vidro_tinta(focus) / 255.0f : 1.0f;
    gfx_vidro_painel(r, 0.5f, 0.55f, a);
    glass_focus(r, focus, a);
  } else {
    button_glow(r, focus, a);
    gfx_cor(r, 0.5f, anim_mistura(BTN_REST_R, fr, focus),
            anim_mistura(BTN_REST_G, fg, focus), anim_mistura(BTN_REST_B, fb, focus), a);
  }
  gfx_icone(ig, icon, ic, ic, ic, a);
}

// Backward-compatibility wrappers
float botao_largura(const char *rotulo, const char *icone, int primario) {
  return button_width(rotulo, icone, primario);
}
void  botao_pilula(GfxRect r, const char *rotulo, const char *icone,
                   float foco, int primario, int alinhar, float a) {
  button_pill(r, rotulo, icone, foco, primario, alinhar, a);
}
int   botao_superficie(GfxRect r, float foco, float a) {
  return button_surface(r, foco, a);
}
void  botao_disco(GfxRect r, const char *icone, float foco, float a) {
  button_disc(r, icone, foco, a);
}
float botao_cor_foco(float *r, float *g, float *b) {
  return button_focus_color(r, g, b);
}
void  botao_luz(GfxRect r, float foco, float a) {
  button_glow(r, foco, a);
}
