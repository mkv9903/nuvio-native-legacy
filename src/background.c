// BACKGROUND BEHIND PANELS. See background.h.
#include "background.h"
#include "ajustes.h"
#include "corviva.h"
#include "layout.h"
#include "tex_cache.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int background_mode(void) { return ajustes_fundo(); }

// veil = 1: full-screen artwork and mockup veil output in a single pass
// (gfx_arte_veu) when the path permits; returns 1 if veil was already applied.
static int draw_art(GfxRect r, float radiusPx, const char *c, float a, int veil) {
  GLuint t = (c && c[0]) ? tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f) : 0;
  int done = 0;
  if (!t) { gfx_cor(r, radiusPx / r.h, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a); return 0; }
  gfx_tex_aspect_atual = tex_aspecto(c);
  gfx_arte_opaca_atual = tex_opaca(c);   // opaque full-screen: underlying light is not painted
  // The veil: 40% in center, 62% on left edge (+0.367 of remainder), 50% on right (+0.167).
  if (veil && radiusPx <= 0.0f && a >= 0.999f)
    done = gfx_arte_veu(r, t, 0.40f, 0.367f, 0.167f, a);
  if (!done) gfx_rect(r, t, GFX_ARTE, 0, 0, 0, radiusPx / r.h, 1, 1, 1, a);
  gfx_arte_opaca_atual = 0;
  gfx_tex_aspect_atual = 0;
  return done;
}

// Color of a palette region with saturation +35% and brightness 62%.
#ifndef FUNDO_BRILHO
#define FUNDO_BRILHO 1.0f
#endif
static void tint(const float *q, float *o) {
  float l = 0.2126f * q[0] + 0.7152f * q[1] + 0.0722f * q[2];
  int k;
  for (k = 0; k < 3; k++) {
    float v = (l + (q[k] - l) * 1.35f) * FUNDO_BRILHO;
    o[k] = v < 0 ? 0 : v > 1 ? 1 : v;
  }
}

static int is_fullscreen(GfxRect r) {
  return r.x <= 0.5f && r.y <= 0.5f && r.w >= NV_TELA_W / gfx_escala() - 1 && r.h >= NV_TELA_H / gfx_escala() - 1;
}

// Bakes (only bakes) blurred artwork into small buffer; paints if alpha > 0.
// Same bake serves frosted glass (gfx_vidro_fosco): artwork light, on screen.
static void bake(const CorvivaPaleta *p, float alpha) {
  float amb[4][3], strength = nv_ambiente_forca, time = nv_tempo_viva;
  int i;
  memcpy(amb, nv_ambiente_viva, sizeof amb);
  for (i = 0; i < 4; i++) tint(p->regiao[i], nv_ambiente_viva[i]);
  nv_ambiente_forca = 1.0f; nv_tempo_viva = 0.0f;   // static: bake does not redo
  gfx_ambiente_preparar();
  if (alpha > 0.0f) gfx_ambiente(alpha * 0.998f);   // < 1: paints now, does not stay pending for clear
  memcpy(nv_ambiente_viva, amb, sizeof amb);
  nv_ambiente_forca = strength; nv_tempo_viva = time;
}

// Crisp art / Frost background with "Frosted glass" active: bake exists only for
// glass. With Immersive, real light is already the bake, so nothing to do.
static void frosted_prepare(GfxRect r, float radiusPx, const char *c) {
  CorvivaPaleta p;
  if (!ajustes_vidro() || !ajustes_vidro_fosco() || nv_ambiente_forca > 0.001f ||
      !is_fullscreen(r) || radiusPx > 0.0f || !c || !c[0] || !corviva_paleta(c, &p) || !p.ok) return;
  bake(&p, 0.0f);
}

void background_frosted_frame(void) {
  CorvivaPaleta p;
  if (!ajustes_vidro() || !ajustes_vidro_fosco() || nv_ambiente_forca > 0.001f ||
      !corviva_cena_paleta(&p)) return;
  bake(&p, 0.0f);
}

// FULL SCREEN BACKGROUND VIA IMMERSIVE LIGHT PATH (gfx_luz_canal).
// Standard drawing goes to small buffer only when key changes — through
// SAME bake as immersive light, which displays correctly on C9 — and each
// frame costs ONE opaque quad (GFX_SNAP, screen light pass). The 28% veil
// of "Blurred art" goes in same pass, and un-veiled buffer is source for
// frosted glass. Frost bakes entire direct drawing.
//
// BLURRED ART = TITLE IMAGE BLURRED (owner, C9, 05/10/2026: "Blurred art still
// doesn't blur, only darkens background"). Previously it was just artwork LIGHT
// (four palette region colors, gfx_ambiente) and depended on corviva palette:
// without palette for that url (palette is only recorded when art is decoded at
// spotlight ceiling, tex_cache.c) blur returned 0 and background fell back to
// CRISP art with 40-62% veil — dark, unblurred and without any log line.
// Now the source is the artwork TEXTURE: the 96x54 copy from gfx_desfocado
// (two passes of GFX_BLUR, same as "Blur unwatched" which C9 already renders)
// is baked in "cover" on 320x180 buffer; bilinear of both upscales
// (96 -> 320 -> 1920) provides remainder of blur. Forms and colors remain;
// detail and text disappear. Nothing per frame beyond usual quad: copy and bake
// only rerun when artwork changes.
//
// ONE-TIME VERIFICATION (C9, 05/10/2026: 1bcd6ebe bake came out dark on TV and
// correct on Mac). The first time each background outputs opaque, a SCREEN
// pixel, read right after quad, is compared with computation here (Frost:
// same computation on CPU; Blurred: small buffer pixel with veil, and small
// buffer against blurred copy). Mismatch: logs "[color] background ... WRONG"
// and direct drawing for session. One read of few px per background per session.
#define CHANNEL_BLURRED 0
#define CHANNEL_FROST   1
#define IDX_BLURRED     0
#define IDX_FROST       1

// Veil for Blurred: 28% behind panels (Settings); title page requests more
// (background_blurred_veil), because hero text sits directly over it.
#define VEIL_BLURRED_DEFAULT 0.28f
static float VEIL_BLURRED[4] = { 0.024f, 0.027f, 0.035f, VEIL_BLURRED_DEFAULT };
void background_blurred_veil(float strength) {
  VEIL_BLURRED[3] = strength < 0.0f ? VEIL_BLURRED_DEFAULT : strength > 1.0f ? 1.0f : strength;
}

static const char *const NAME_IDX[2] = { "borrada", "frost" };
static int checked[2] = { -1, -1 };   // -1 to check, 1 correct, 0 wrong (direct drawing)
static int checked_baked[2] = { -1, -1 };
int background_check(int mode) {
  return mode == BACKGROUND_FROST ? checked[IDX_FROST] : mode == BACKGROUND_BLURRED ? checked[IDX_BLURRED] : -1;
}

// Frost direct at (u, v) of full screen: base, top gradient, and three lights
// (GFX_LUZ: smooth quadratic falloff).
static void ref_frost(const float *ac, float u, float v, int light, float o[3]) {
  static const float L[3][3] = { { 0.14f, 0.08f, 1.05f }, { 0.92f, 0.96f, 0.90f }, { 0.70f, 0.30f, 0.60f } };
  static const float B[3] = { 0.043f, 0.047f, 0.055f }, T[3] = { 0.082f, 0.086f, 0.102f };
  const float A = 1920.0f / 1080.0f;
  float g = v / 0.70f;
  int i, j;
  g = 1.0f - (g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g);
  for (j = 0; j < 3; j++) o[j] = B[j] * (1.0f - g) + T[j] * g;
  if (!light) return;
  for (i = 0; i < 3; i++) {
    float px = (u - L[i][0]) * A, py = v - L[i][1], t = 1.0f - sqrtf(px * px + py * py) / L[i][2], sv, al;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    sv = t * t * (3.0f - 2.0f * t);
    al = sv * sv * ac[9 + i];
    for (j = 0; j < 3; j++) o[j] = o[j] * (1.0f - al) + ac[i * 3 + j] * al;
  }
}

static int level(float c) { return (int)(c * 255.0f + 0.5f); }

// Point where Frost light weighs heaviest among candidates. Returns deviation
// (levels) between "with light" and "without light" and fills u, v and references.
static int light_point(const float *ac, float *u, float *v, int ref[3], int base[3]) {
  static const float CF[3][2] = { { 0.14f, 0.08f }, { 0.92f, 0.94f }, { 0.5f, 0.5f } };
  int i, j, best = -1;
  for (i = 0; i < 3; i++) {
    float c[3], b[3];
    int dev = 0;
    ref_frost(ac, CF[i][0], CF[i][1], 1, c); ref_frost(ac, CF[i][0], CF[i][1], 0, b);
    for (j = 0; j < 3; j++) { int d = abs(level(c[j]) - level(b[j])); if (d > dev) dev = d; }
    if (dev > best) {
      best = dev; *u = CF[i][0]; *v = CF[i][1];
      for (j = 0; j < 3; j++) { ref[j] = level(c[j]); base[j] = level(b[j]); }
    }
  }
  return best;
}

// ONE-TIME DUMP (owner, 05/10/2026: see on TV what was actually drawn).
// On FIRST time per execution that each full-screen background outputs opaque, writes
// /tmp/nuvio-fundo-<frost|borrada>.bmp (screen right after background, half resolution)
// and /tmp/nuvio-fundo-<...>-assado.bmp (320x180 buffer when present), and logs checked pixel.
// Without request file: a request file the app could not delete dropped C9 to 9 fps.
// Flag is set BEFORE writing, so failed writes also do not repeat; without writable
// /tmp (Android) nothing is read. NUVIO_DUMP_FUNDO_DIR overrides /tmp (Mac tests).
static const char *dump_dir(void) {
  const char *d = getenv("NUVIO_DUMP_FUNDO_DIR");
  return d && d[0] ? d : "/tmp";
}

static int dumped[2];
static int dump_needed(int k, float a) {
  if (dumped[k] || a * gfx_opacidade_grupo < 0.999f) return 0;
  if (access(dump_dir(), W_OK) != 0) { dumped[k] = 1; return 0; }
  return 1;
}

static void dump_screen(int k, GLuint t) {
  char path[640];
  snprintf(path, sizeof path, "%s/nuvio-fundo-%s.bmp", dump_dir(), NAME_IDX[k]);
  printf("[fundo-dump] %s: %s", path, gfx_tela_bmp(path) ? "gravado" : "FALHOU");
  if (t) {
    snprintf(path, sizeof path, "%s/nuvio-fundo-%s-assado.bmp", dump_dir(), NAME_IDX[k]);
    printf(", %s: %s", path, gfx_luz_canal_bmp(t, path) ? "gravado" : "FALHOU");
  }
  printf("\n");
  fflush(stdout);
}

static int can_check(int k, GLuint t, float a) {
  return t && checked[k] < 0 && checked_baked[k] != gfx_n_fundo_assados &&
         a * gfx_opacidade_grupo >= 0.999f && !gfx_efeitos_leves() && !gfx_modos_desligados;
}

// After Frost quad (t = small buffer, 0 in direct drawing):
// one-time verification and one-time dump.
static void after_frost(GLuint t, const float *ac, float a) {
  const int k = IDX_FROST;
  int do_dump = dump_needed(k, a), do_check = can_check(k, t, a), dev, ref[3], base[3], j, err = 0, tol;
  unsigned char screen[3] = { 0, 0, 0 }, small[3] = { 0, 0, 0 };
  float u = 0.5f, v = 0.5f;
  if (!do_check && !do_dump) return;
  dev = light_point(ac, &u, &v, ref, base);
  tol = 5 + dev / 5;
  if (!gfx_tela_px(u, v, screen)) return;   // inside snapshot: nothing read, deferred to next
  if (do_dump) dumped[k] = 1;
  if (t) gfx_luz_canal_px(t, u, v, small);
  for (j = 0; j < 3; j++) { int d = abs((int)screen[j] - ref[j]); if (d > err) err = d; }
  if (do_check) {
    if (dev < 12) checked_baked[k] = gfx_n_fundo_assados;   // light too weak: check on next bake
    else checked[k] = err <= tol;
  }
  if ((do_check && dev >= 12) || do_dump) {
    printf("[cor] fundo frost %s: em %.2f,%.2f esperado %d,%d,%d (sem luz %d,%d,%d); tela %d,%d,%d; "
           "quadro pequeno %d,%d,%d%s\n",
           !t ? "direto" : checked[k] == 1 ? "conferido" : checked[k] == 0 ? "CONFERIDO ERRADO (desenho direto)" : "lido",
           u, v, ref[0], ref[1], ref[2], base[0], base[1], base[2], screen[0], screen[1], screen[2], small[0], small[1], small[2],
           t ? "" : " (sem quadro pequeno)");
    fflush(stdout);
  }
  if (do_dump) dump_screen(k, t);
}

// --- BLURRED ART -------------------------------------------------------------
typedef struct {
  GLuint d;     // blurred 96x54 copy (ENTIRE art, stretched)
  float asp;    // w/h of art (0 = unknown: stretch)
} Blurred;

// Cover rectangle of art `asp` on full screen (may overflow bounds).
static GfxRect cover_screen(float asp) {
  float W = NV_TELA_W, H = NV_TELA_H, w = W, h = H;
  if (asp > 0.01f) { if (asp > W / H) w = H * asp; else h = W / asp; }
  return (GfxRect){ (W - w) * 0.5f, (H - h) * 0.5f, w, h };
}

static void paint_blurred_art(const Blurred *b, float a) {
  float prev_asp = gfx_tex_aspect_atual;
  int prev_op = gfx_arte_opaca_atual;
  gfx_tex_aspect_atual = 0.0f; gfx_arte_opaca_atual = 0;
  gfx_rect(cover_screen(b->asp), b->d, GFX_ARTE, 0, 0, 0, 0.0f, 1, 1, 1, a);
  gfx_tex_aspect_atual = prev_asp; gfx_arte_opaca_atual = prev_op;
}

// THE BAKE: 96x54 copy stretched to full screen reveals bilinear seams
// (each texel becomes a 20 px ramp with corners at both ends, high contrast
// art showed bands — owner, 05/10: "harsh gradient"). Here it is painted
// 25 times in a grid of +-32 px, each with alpha 1/n: average of all,
// a ~3-texel box filter eliminating corners. Only in bake, which reruns
// when art changes; direct drawing remains single pass.
static void paint_blurred(void *ctx) {
  const Blurred *b = ctx;
  float prev_asp = gfx_tex_aspect_atual;
  int prev_op = gfx_arte_opaca_atual, i, n = 0;
  GfxRect cv = cover_screen(b->asp);
  gfx_tex_aspect_atual = 0.0f; gfx_arte_opaca_atual = 0;
  for (i = 0; i < 25; i++) {
    GfxRect r = cv;
    r.x += (float)(i % 5 - 2) * 16.0f; r.y += (float)(i / 5 - 2) * 16.0f;
    // shifted copy border must not let clear peek through
    r.x -= 40.0f; r.y -= 40.0f; r.w += 80.0f; r.h += 80.0f;
    gfx_rect(r, b->d, GFX_ARTE, 0, 0, 0, 0.0f, 1, 1, 1, 1.0f / (float)++n);
  }
  gfx_tex_aspect_atual = prev_asp; gfx_arte_opaca_atual = prev_op;
}

// One-time warnings per session: why Blurred did not follow normal path.
static void notice_blurred(int *seen, const char *msg) {
  if (*seen) return;
  *seen = 1;
  printf("[cor] fundo borrada: %s\n", msg);
  fflush(stdout);
}

// After Blurred quad (t = small buffer, 0 in direct drawing).
// Checks at three points: screen == small buffer with veil (screen pass)
// and small buffer == blurred copy at same artwork spot (bake). Deviation
// of small buffer from plain background goes to log: bake containing only
// clear appears there and in dump.
static void after_blurred(GLuint t, const Blurred *b, float a) {
  static const float P[3][2] = { { 0.25f, 0.30f }, { 0.50f, 0.50f }, { 0.75f, 0.70f } };
  const int k = IDX_BLURRED;
  int do_dump = dump_needed(k, a), do_check = can_check(k, t, a), i, j;
  int screen_err = 0, bake_err = 0, dev = 0, worst_i = 1;
  unsigned char screen[3][3], small[3][3], blurred[3][3];
  float ref[3][3];
  GfxRect cv = cover_screen(b->asp);
  if (!do_check && !do_dump) return;
  memset(small, 0, sizeof small); memset(blurred, 0, sizeof blurred);
  for (i = 0; i < 3; i++) if (!gfx_tela_px(P[i][0], P[i][1], screen[i])) return;   // snapshot: deferred
  if (do_dump) dumped[k] = 1;
  for (i = 0; i < 3; i++) {
    float au = (P[i][0] * NV_TELA_W - cv.x) / cv.w, av = (P[i][1] * NV_TELA_H - cv.y) / cv.h;
    int has_small = t && gfx_luz_canal_px(t, P[i][0], P[i][1], small[i]);
    int has_blur = gfx_desfocado_px(b->d, au, av, blurred[i]);
    const float F[3] = { NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B };
    for (j = 0; j < 3; j++) {
      float src = has_small ? small[i][j] / 255.0f : has_blur ? blurred[i][j] / 255.0f : F[j];
      int dt, da, dv;
      ref[i][j] = (src * (1.0f - VEIL_BLURRED[3]) + VEIL_BLURRED[j] * VEIL_BLURRED[3]) * 255.0f;
      dt = abs((int)screen[i][j] - (int)(ref[i][j] + 0.5f));
      da = has_small && has_blur ? abs((int)small[i][j] - (int)blurred[i][j]) : 0;
      dv = abs((int)(src * 255.0f + 0.5f) - level(F[j]));
      if (dt > screen_err) screen_err = dt;
      if (da > bake_err) { bake_err = da; worst_i = i; }
      if (dv > dev) dev = dv;
    }
  }
  // Copy (96x54) and bake (320x180) are sampled at nearest texel: near edge
  // of blurred artwork they differ by a few levels due to bilinear filtering.
  if (do_check) checked[k] = screen_err <= 6 && bake_err <= 72;   // 72: bake is AVERAGE of 25 shifted copies
  printf("[cor] fundo borrada %s: tela %d,%d,%d esperado %.0f,%.0f,%.0f (erro %d); quadro pequeno %d,%d,%d, "
         "copia desfocada %d,%d,%d (erro %d); desvio do fundo liso %d%s\n",
         !t ? "direto" : checked[k] == 1 ? "conferido" : checked[k] == 0 ? "CONFERIDO ERRADO (desenho direto)" : "lido",
         screen[worst_i][0], screen[worst_i][1], screen[worst_i][2], ref[worst_i][0], ref[worst_i][1], ref[worst_i][2], screen_err,
         small[worst_i][0], small[worst_i][1], small[worst_i][2], blurred[worst_i][0], blurred[worst_i][1], blurred[worst_i][2], bake_err, dev,
         t ? "" : " (sem quadro pequeno)");
  fflush(stdout);
  if (do_dump) dump_screen(k, t);
}

// Blurred copy of art `c`, or 0 (no texture yet, or copy limit per frame).
// Requests art at screen width, matching crisp art.
static GLuint blurred_copy(GfxRect r, const char *c, float *asp) {
  GLuint src = tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f);
  *asp = src ? tex_aspecto(c) : 0.0f;
  return src ? gfx_desfocado(src, c) : 0;
}

static unsigned long hash_url(const char *s) {
  unsigned long h = 2166136261u;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return h & 0xffffffffu;
}

static int draw_blurred(GfxRect r, float radiusPx, const char *c, float a) {
  static int notice_no_copy, notice_direct;
  static GLuint last_tex;          // last small buffer rendered, and whose
  static unsigned long last_hash;
  Blurred b;
  unsigned long h;
  if (!c || !c[0]) return 0;
  h = hash_url(c);
  b.d = blurred_copy(r, c, &b.asp);
  if (is_fullscreen(r) && radiusPx <= 0.0f) {
    GLuint t = 0;
    if (!b.d) {
      // Art has not arrived yet, or copy deferred to next frame: use
      // small buffer of this same art if available; else plain background with veil.
      if (last_tex && last_hash == h && glIsTexture(last_tex)) {
        gfx_luz_canal_desenhar(last_tex, a, VEIL_BLURRED, 0);
        return 1;
      }
      notice_blurred(&notice_no_copy, "sem a copia desfocada da arte ainda (arte nao decodificada?): fundo liso");
      gfx_cor(r, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
      gfx_cor(r, 0.0f, VEIL_BLURRED[0], VEIL_BLURRED[1], VEIL_BLURRED[2], VEIL_BLURRED[3] * a);
      return 1;
    }
    if (checked[IDX_BLURRED] != 0) {
      float k[9];
      k[0] = (float)(b.d & 0xffffu); k[1] = (float)(b.d >> 16);
      k[2] = (float)(h & 0xffffu);   k[3] = (float)(h >> 16);
      k[4] = b.asp;
      k[5] = NV_COR_FUNDO_R; k[6] = NV_COR_FUNDO_G; k[7] = NV_COR_FUNDO_B;
      k[8] = 1.0f;   // draw version: 1 = blurred copy
      t = gfx_luz_canal(CHANNEL_BLURRED, k, 9, paint_blurred, &b);
    }
    if (t) {
      if (ajustes_vidro() && ajustes_vidro_fosco()) gfx_vidro_fosco_fonte(t);
      gfx_luz_canal_desenhar(t, a, VEIL_BLURRED, 0);
      last_tex = t; last_hash = h;
    } else {
      // DIRECT DRAWING: copy in cover on screen with veil over it (one extra
      // blended full screen). Without small buffer, or verification failed.
      notice_blurred(&notice_direct, checked[IDX_BLURRED] == 0 ? "desenho direto (conferencia errou)"
                                                              : "desenho direto (sem quadro pequeno)");
      paint_blurred_art(&b, a);
      gfx_cor(r, 0.0f, VEIL_BLURRED[0], VEIL_BLURRED[1], VEIL_BLURRED[2], VEIL_BLURRED[3] * a);
    }
    after_blurred(t, &b, a);
    return 1;
  }
  // PREVIEWS (Settings > Background cards, rounded corners): copy stretched
  // on card (catalog artwork is 16:9, like card) and veil.
  if (b.d) {
    float prev_asp = gfx_tex_aspect_atual;
    gfx_tex_aspect_atual = 0.0f;
    gfx_rect(r, b.d, GFX_ARTE, 0, 0, 0, radiusPx / r.h, 1, 1, 1, a);
    gfx_tex_aspect_atual = prev_asp;
  } else gfx_cor(r, radiusPx / r.h, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
  gfx_cor(r, radiusPx / r.h, VEIL_BLURRED[0], VEIL_BLURRED[1], VEIL_BLURRED[2], VEIL_BLURRED[3] * a);
  return 1;
}

// FROST (acentos-mockup.html, frame 6): light is NOT raw accent, it is same
// hue with L 0.42 and chroma <= 0.11 (ajustes_acento_luz) — White becomes neutral
// haze and Jade does not burn screen. Background linear(165deg, #15161A, #0B0C0E 70%)
// and three lights at 70%, 45% and 18%.
// THE THREE LIGHTS: color (9) and strength (3). Outside Immersive, spotlight
// color across three at 70/45/18%. IN IMMERSIVE (owner, 05/10: "frost background in
// immersive is always black, not with immersive color"), each light is a SCENE
// REGION — same colors Home paints — and stronger because here they are only
// three pools and not full screen.
static void frost_lights(float c[12]) {
  static const float F[3] = { 0.70f, 0.45f, 0.18f }, FI[3] = { 0.90f, 0.70f, 0.45f };
  float ac[3], k = nv_ambiente_forca < 0.0f ? 0.0f : nv_ambiente_forca > 1.0f ? 1.0f : nv_ambiente_forca;
  int i, j;
  ajustes_acento_luz(&ac[0], &ac[1], &ac[2]);
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) c[i * 3 + j] = ac[j] + (nv_ambiente_viva[i][j] - ac[j]) * k;
    c[9 + i] = F[i] + (FI[i] - F[i]) * k;
  }
}

static void draw_frost(GfxRect r, float radiusPx, float a) {
  float c[12], rr = radiusPx / r.h;
  frost_lights(c);
  gfx_cor(r, rr, 0.043f, 0.047f, 0.055f, a);   // #0B0C0E
  gfx_rect(r, 0, GFX_VEU_CSS, 1.0f, 1.0f, 0.70f, rr, 0.082f, 0.086f, 0.102f, a);   // #15161A top
  gfx_luz_canto(r, rr, r.w * 0.14f, r.h * 0.08f, r.h * 1.05f, c[0], c[1], c[2], c[9] * a);
  gfx_luz_canto(r, rr, r.w * 0.92f, r.h * 0.96f, r.h * 0.90f, c[3], c[4], c[5], c[10] * a);
  gfx_luz_canto(r, rr, r.w * 0.70f, r.h * 0.30f, r.h * 0.60f, c[6], c[7], c[8], c[11] * a);
}

static void paint_frost(void *ctx) {
  (void)ctx;
  draw_frost((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 1.0f);
}

static void frost_screen(GfxRect r, float radiusPx, float a) {
  float k[15];
  GLuint t = 0;
  if (!is_fullscreen(r) || radiusPx > 0.0f) { draw_frost(r, radiusPx, a); return; }
  // IN IMMERSIVE FROST IS AMBIENT LIGHT (owner, 05/10: "wanted frost background
  // to follow same dynamics as Apple TV layout background in movie").
  // Exactly what title page already paints without Frost: app floor and light
  // from scene regions above (gfx_ambiente, Home bake). Three Frost pools
  // looked nearly black beside it.
  if (nv_ambiente_forca > 0.5f) {
    gfx_cor(r, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    // Opaque (alpha 1): light goes through UNBLENDED path of gfx_ambiente,
    // same as Home buffer. At 0.998 it was an extra blended full screen
    // per frame, and TCL dropped to 34 fps on title page (log 05/10).
    gfx_ambiente(a >= 0.999f ? 1.0f : a * 0.998f);
    return;
  }
  frost_lights(k);
  k[12] = NV_COR_FUNDO_R; k[13] = NV_COR_FUNDO_G; k[14] = NV_COR_FUNDO_B;
  if (checked[IDX_FROST] != 0) t = gfx_luz_canal(CHANNEL_FROST, k, 15, paint_frost, NULL);
  // DEFERRED (gfx_luz_canal_adiar): full-screen artwork arriving over it
  // (top of title page) reads Frost via uAmb and outputs opaque, or scroll
  // veil goes in same pass. Does not defer when verification or one-time dump
  // will read screen immediately after quad.
  if (t && a >= 0.999f && !can_check(IDX_FROST, t, a) && !dump_needed(IDX_FROST, a) &&
      gfx_luz_canal_adiar(t, 1))
    return;
  if (t) gfx_luz_canal_desenhar(t, a, NULL, 1);
  else draw_frost(r, radiusPx, a);
  after_frost(t, k, a);
}

void background_draw_mode(int mode, GfxRect r, float radiusPx, const char *c, float a) {
  if (r.w < 1 || r.h < 1 || a <= 0.003f) return;
  if (mode == BACKGROUND_FROST) {
    frosted_prepare(r, radiusPx, c);
    frost_screen(r, radiusPx, a);
    return;
  }
  if (mode == BACKGROUND_BLURRED && draw_blurred(r, radiusPx, c, a)) return;
  frosted_prepare(r, radiusPx, c);
  if (draw_art(r, radiusPx, c, a, radiusPx <= 0.0f)) return;
  if (radiusPx > 0.0f) { gfx_cor(r, radiusPx / r.h, 0, 0, 0, 0.42f * a); return; }
  // Mockup veil: linear-gradient(90deg, 62%, 40% center, 50%).
  // Solid 40% fill and, on top, both halves in gradient (GFX_VEU_CSS,
  // nv_dither) with remainder to reach 62% and 50% at edges.
  // Solid goes INSIDE both halves (gfx_veu_css_base): one fewer blended
  // full-screen layer per frame, same pixel.
  gfx_veu_css_base((GfxRect){ r.x, r.y, r.w * 0.5f, r.h }, 2, 1.0f, 1.0f, 0.367f * a, 0.40f * a);
  gfx_veu_css_base((GfxRect){ r.x + r.w * 0.5f, r.y, r.w * 0.5f, r.h }, 3, 1.0f, 1.0f, 0.167f * a, 0.40f * a);
}

void background_draw(GfxRect area, const char *artUrl, float a) {
  background_draw_mode(background_mode(), area, 0.0f, artUrl, a);
}

// Backward-compatibility wrappers for legacy callers
int  fundo_modo(void) { return background_mode(); }
void fundo_desenhar(GfxRect area, const char *arteUrl, float a) { background_draw(area, arteUrl, a); }
void fundo_desenhar_modo(int modo, GfxRect area, float raioPx, const char *arteUrl, float a) {
  background_draw_mode(modo, area, raioPx, arteUrl, a);
}
void fundo_fosco_quadro(void) { background_frosted_frame(); }
void fundo_borrada_veu(float forca) { background_blurred_veil(forca); }
int  fundo_conferencia(int modo) { return background_check(modo); }
