// See screensaver.h.
#include "horafmt.h"
#include "screensaver.h"
#include "esmaecer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef DESCANSO_SEM_GFX
#include "gfx.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "agenda.h"
#include "ajustes.h"
#include "idioma.h"
#include "app.h"
#endif

int screensaver_item_valid(int has_backdrop, int has_title, int on_list, int progress, int source) {
  if (!has_backdrop || !has_title) return 0;
  if (source == SCREENSAVER_SOURCE_LIST) return on_list || progress > 0;
  return 1;
}

// Two sine waves of co-prime periods: path repeats only after hours.
void screensaver_path(float s, float *px, float *py) {
  *px = 0.5f + 0.5f * sinf(s / 37.0f);
  *py = 0.5f + 0.5f * sinf(s / 53.0f + 1.3f);
}

#ifndef DESCANSO_SEM_GFX

#define SCR_ART_WIDTH   1920.0f
#define SCR_LOGO_W       820.0f
#define SCR_LOGO_H       220.0f
#define SCR_X            120.0f
#define SCR_BASE        (NV_TELA_H - 132.0f)
#define SCR_TEXT_W      1080.0f
#define SCR_WAIT_S         6.0f   // without next art ready, max wait in black
#define SCR_PATH_X       140.0f  // clock block shifts +-140 x +-80 px
#define SCR_PATH_Y        80.0f
#define SCR_CYCLE_MS    60000u   // clock stays in place for one minute
#define SCR_SWITCH_MS    1500u   // 1.5 s fade out, 1.5 s fade in

static int   prev_active;
static int   current_style = ESM_ESTILO_VITRINE;
static int   current_source;
static float active_time;          // seconds since screensaver entered
static int   queue[SCREENSAVER_QUEUE_MAX];
static int   queue_len, queue_pos;
static unsigned queue_revision;
static float item_t;               // seconds within current item
static float wait_t;               // black screen waiting for next art
static int   current_idx = -1;     // catalog index on screen (or -1)
static GLuint tex_art, tex_logo, tex_next;
static int   request_open_idx = -1;
static char  next_line[256];       // "Next premiere" for clock, assembled on enter

static unsigned random_pick(unsigned *state) {
  *state = *state * 1664525u + 1013904223u;
  return *state >> 8;
}

static int item_valid(int i, int source) {
  const CatItem *c = cat_item(i);
  return c && screensaver_item_valid(c->backdrop[0] != 0, c->titulo[0] != 0, c->naLista,
                                     c->progresso, source);
}

// Shuffles queue for session. "My list & Continue" with under 3 items
// falls back to full catalog: three transitions would repeat otherwise.
static void build_queue(unsigned seed) {
  int n = cat_n(), i, source = current_source;
  queue_len = 0; queue_pos = 0;
  if (source == SCREENSAVER_SOURCE_LIST) {
    int count = 0;
    for (i = 0; i < n; i++) if (item_valid(i, source)) count++;
    if (count < 3) source = SCREENSAVER_SOURCE_CATALOG;
  }
  for (i = 0; i < n && queue_len < SCREENSAVER_QUEUE_MAX; i++)
    if (item_valid(i, source)) queue[queue_len++] = i;
  // Fisher-Yates shuffle
  for (i = queue_len - 1; i > 0; i--) {
    int j = (int)(random_pick(&seed) % (unsigned)(i + 1)), t = queue[i];
    queue[i] = queue[j]; queue[j] = t;
  }
  queue_revision = cat_revisao();
}

static int queue_item(int pos) {
  int i;
  if (queue_len <= 0) return -1;
  i = queue[((pos % queue_len) + queue_len) % queue_len];
  return item_valid(i, SCREENSAVER_SOURCE_CATALOG) ? i : -1;
}

static void build_next(void) {
  int i, n, best = 99, found = -1;
  next_line[0] = 0;
  n = agenda_n();
  for (i = 0; i < n; i++) {
    const AgItem *a = agenda_lista(i);
    int d = a && a->dataProx[0] ? agenda_dias(a->dataProx) : AG_SEM_DATA;
    // Without title line would read " · S2 E3 · tomorrow": nameless premiere
    // says nothing. Schedule fills title later; until then keep looking.
    if (d >= 0 && d <= 7 && d < best && a->titulo[0]) { best = d; found = i; }
  }
  if (found >= 0) {
    const AgItem *a = agenda_lista(found);
    char when[64];
    agenda_falta(a->dataProx, when, sizeof when);
    if (a->temporada > 0 && a->episodio > 0)
      snprintf(next_line, sizeof next_line, i18n("%s · T%d E%d · %s"), a->titulo,
               a->temporada, a->episodio, when);
    else snprintf(next_line, sizeof next_line, i18n("%s · %s"), a->titulo, when);
  }
}

static void enter(unsigned now) {
  active_time = 0.0f;
  item_t = 0.0f; wait_t = 0.0f;
  current_idx = -1; tex_art = tex_logo = tex_next = 0;
  build_queue(now ^ 0x9e3779b9u);
  current_idx = queue_item(0);
  if (app_na_home() && agenda_n() == 0) agenda_montar();
  build_next();
}

int screensaver_onscreen_index(void) {
  if (current_style != ESM_ESTILO_VITRINE || current_idx < 0 || !tex_art) return -1;
  if (item_t < 0.6f || item_t > SCREENSAVER_ITEM_S - SCREENSAVER_EXIT_S) return -1;
  return current_idx;
}

void screensaver_request_open(void) { request_open_idx = screensaver_onscreen_index(); }
int  screensaver_requested_open(void) { int v = request_open_idx; request_open_idx = -1; return v; }

void screensaver_frame(unsigned now, float dt, int active, int style, int source) {
  const CatItem *c;
  if (dt < 0.0f) dt = 0.0f;
  if (dt > 0.1f) dt = 0.1f;
  current_style = style; current_source = source;
  if (!active) { prev_active = 0; current_idx = -1; tex_art = tex_logo = tex_next = 0; return; }
  if (!prev_active) enter(now);
  prev_active = 1;
  active_time += dt;
  if (style != ESM_ESTILO_VITRINE) return;
  // Catalog changed midway (sync, profile): reshuffle without interrupting current.
  if (queue_revision != cat_revisao()) { build_queue(now); queue_pos = 0; }
  if (current_idx < 0) { current_idx = queue_item(queue_pos); item_t = 0.0f; }
  c = current_idx >= 0 ? cat_item(current_idx) : NULL;
  tex_art = c ? tex_obter_larg(c->backdrop, SCR_ART_WIDTH) : 0;
  tex_logo = c && c->logo[0] ? tex_obter_logo_larg(c->logo, SCR_LOGO_W) : 0;
  // Item timer advances only with art on screen: prevents fade-in from spending
  // time during download.
  if (tex_art || !c) item_t += dt;
  // Art for NEXT is requested midway through current.
  {
    int next_idx = queue_item(queue_pos + 1);
    const CatItem *p = next_idx >= 0 ? cat_item(next_idx) : NULL;
    tex_next = p && item_t > SCREENSAVER_ITEM_S * 0.5f ? tex_obter_larg(p->backdrop, SCR_ART_WIDTH) : 0;
    if (p && p->logo[0] && item_t > SCREENSAVER_ITEM_S * 0.5f) (void)tex_obter_logo_larg(p->logo, SCR_LOGO_W);
    if (item_t >= SCREENSAVER_ITEM_S) {
      // Stay in black until next art arrives; if none within SCR_WAIT_S, skip.
      wait_t += dt;
      if (tex_next || wait_t > SCR_WAIT_S || !p) {
        queue_pos += (tex_next || !p) ? 1 : 2;
        current_idx = queue_item(queue_pos);
        item_t = 0.0f; wait_t = 0.0f;
      }
    }
  }
}

static float smooth_step(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

// "HH:MM" and long date format, recalculated only when minute rolls over.
static void time_now(char *time_str, size_t nh, char *suf, size_t ns, char *date_str, size_t nd, float *sec_frac) {
  static const char *DAYS[7] = { "domingo", "segunda-feira", "ter\xc3\xa7" "a-feira", "quarta-feira",
                                 "quinta-feira", "sexta-feira", "s\xc3\xa1" "bado" };
  time_t t = time(NULL);
  struct tm lt;
  time_str[0] = 0; if (suf && ns) suf[0] = 0; if (date_str) date_str[0] = 0;
  if (!localtime_r(&t, &lt)) return;
  hora_tela_partes(time_str, nh, suf, ns, &lt);
  if (date_str) snprintf(date_str, nd, i18n("%s, %d de %s"), i18n(DAYS[lt.tm_wday]), lt.tm_mday,
                         i18n(agenda_mes_nome(lt.tm_mon + 1)));
  if (sec_frac) *sec_frac = (float)lt.tm_sec / 60.0f;
}

// --- CLOCK -------------------------------------------------------------------
static void draw_clock(unsigned now, float a) {
  char time_str[12], suf[4], date_str[96];
  float sec, px, py, cx, cy, ar, ag, ab, s = (float)now / 1000.0f;
  int reduced = ajustes_animacoes_reduzidas();
  TxtLinha lh, ld, lp;
  time_now(time_str, sizeof time_str, suf, sizeof suf, date_str, sizeof date_str, &sec);
  // Block changes position EVERY CYCLE (OLED panel) and stays STATIONARY inside it,
  // at integer pixels: moving slightly every frame caused jitter.
  // Transition is never abrupt: in last SCR_SWITCH_MS of cycle block fades out slowly,
  // shifts in dark, and fades into new position.
  {
    unsigned n = now / SCR_CYCLE_MS, f = now % SCR_CYCLE_MS;
    screensaver_path((float)n * 7.0f, &px, &py);
    a *= f < SCR_SWITCH_MS ? smooth_step((float)f / SCR_SWITCH_MS)
       : f > SCR_CYCLE_MS - SCR_SWITCH_MS ? smooth_step((float)(SCR_CYCLE_MS - f) / SCR_SWITCH_MS) : 1.0f;
  }
  cx = floorf(NV_TELA_W * 0.5f + (px * 2.0f - 1.0f) * SCR_PATH_X);
  cy = floorf(NV_TELA_H * 0.5f + (py * 2.0f - 1.0f) * SCR_PATH_Y);
  (void)s; (void)reduced;
  ajustes_acento(&ar, &ag, &ab);
  // Aurora: two subtle glow spots in accent color, slowly drifting.
  {
    float t = 0.0f;   // stationary: only shifts with block
    GfxRect l1 = { cx - 900.0f + sinf(t * 0.11f) * 120.0f, cy - 640.0f, 1300.0f, 1300.0f };
    GfxRect l2 = { cx - 200.0f, cy - 760.0f + cosf(t * 0.09f) * 110.0f, 1200.0f, 1200.0f };
    gfx_rect(l1, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.10f * a);
    gfx_rect(l2, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ab, ar * 0.6f, ag, 0.06f * a);
  }
  // Max brightness of numerals at ~60% (OLED).
  lh = txt_linha(TXT_DESC_HORA, time_str, 236, 238, 244, 255);
  if (lh.tex) {
    float x = floorf(cx - (float)lh.w * 0.5f), y = floorf(cy - (float)lh.h * 0.62f);
    txt_desenhar_alpha(lh, x, y, 0.62f * a);
    // 12 h: "AM/PM" beside numeral, in regular font (numeral only has digits and ':'),
    // aligned with top of digits. Does not enter block width: numeral stays centered as in 24 h.
    if (suf[0]) {
      TxtLinha ls = txt_linha(TXT_HEADLINE, suf, 236, 238, 244, 255);
      txt_desenhar_alpha(ls, x + (float)lh.w + 18.0f, y + (float)lh.h * 0.22f, 0.55f * a);
    }
    // Minute line: dimmed track and filled portion in accent.
    {
      float yl = y + (float)lh.h * 0.98f, w = (float)lh.w;
      gfx_cor((GfxRect){ x, yl, w, 2.0f }, 0, 1, 1, 1, 0.10f * a);
      gfx_cor((GfxRect){ x, yl, w * sec, 2.0f }, 0, ar, ag, ab, 0.70f * a);
      ld = txt_linha(TXT_HEADLINE, date_str, 200, 204, 212, 255);
      txt_desenhar_alpha(ld, floorf(cx - (float)ld.w * 0.5f), yl + 30.0f, 0.62f * a);
      if (next_line[0]) {
        TxtLinha k = txt_linha(TXT_CAPTION2, i18n("Próxima estreia"), 150, 154, 164, 255);
        lp = txt_linha_corta(TXT_CALLOUT, next_line, 210, 214, 222, 255, 1100.0f);
        txt_desenhar_alpha(k, floorf(cx - (float)k.w * 0.5f), yl + 30.0f + (float)ld.h + 40.0f, 0.5f * a);
        txt_desenhar_alpha(lp, floorf(cx - (float)lp.w * 0.5f),
                           yl + 30.0f + (float)ld.h + 40.0f + (float)k.h + 8.0f, 0.55f * a);
      }
    }
  }
}

// --- SHOWCASE ----------------------------------------------------------------
static void meta_from(const CatItem *c, char *dst, size_t n) {
  // "2022 · 3 seasons · Drama · Mystery": catalog meta and first two genres.
  char g[160], *p, *q;
  int k = 0;
  snprintf(dst, n, "%s", c->meta);
  snprintf(g, sizeof g, "%s", c->genero);
  for (p = g; p && *p && k < 2; p = q) {
    size_t z;
    q = strstr(p, "\xc2\xb7");
    if (q) { *q = 0; q += 2; }
    while (*p == ' ') p++;
    z = strlen(p);
    while (z > 0 && p[z - 1] == ' ') p[--z] = 0;
    if (!*p || !strcmp(p, "Programa de TV") || !strcmp(p, "Filme") || !strcmp(p, "S\xc3\xa9rie")) continue;
    if (dst[0]) strncat(dst, " \xc2\xb7 ", n - strlen(dst) - 1);
    strncat(dst, i18n(p), n - strlen(dst) - 1);
    k++;
  }
}

static void first_sentence(const char *s, char *dst, size_t n) {
  const char *f = strstr(s, ". ");
  size_t k = f ? (size_t)(f - s) + 1 : strlen(s);
  if (k >= n) k = n - 1;
  memcpy(dst, s, k); dst[k] = 0;
}

static void draw_showcase(unsigned now, float a) {
  const CatItem *c = current_idx >= 0 ? cat_item(current_idx) : NULL;
  float t = item_t, ent, ext, vis, u;
  char time_str[8];
  (void)now;
  if (!c || !tex_art) {
    if (queue_len <= 0) draw_clock(now, a);   // catalog without backdrop
    return;
  }
  ent = smooth_step(t / SCREENSAVER_ENTER_S);
  ext = t > SCREENSAVER_ITEM_S - SCREENSAVER_EXIT_S ? smooth_step((t - (SCREENSAVER_ITEM_S - SCREENSAVER_EXIT_S)) / SCREENSAVER_EXIT_S) : 0.0f;
  vis = ent * (1.0f - ext) * a;
  u = 0.0f;
  // Artwork STATIONARY, crossfade on transition.
  {
    GfxRect r = { 0, 0, NV_TELA_W, NV_TELA_H };
    (void)u;
    gfx_tex_aspect_atual = tex_aspecto(c->backdrop);
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(r, tex_art, GFX_CARD, 0, 0, 0, 0.0f, 1, 1, 1, vis);
    gfx_card_forcar_cover_atual = 0.0f;
    gfx_tex_aspect_atual = 0.0f;
  }
  // Veils: bottom strong (text), top light (time), shadow on text corner.
  // Light scene must not wash out title.
  gfx_rect((GfxRect){ -700.0f, 260.0f, 2300.0f, 1500.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           0, 0, 0, 0.80f * vis);
  gfx_rect((GfxRect){ 0, NV_TELA_H - 640.0f, NV_TELA_W, 640.0f }, 0, GFX_VEU_BAIXO,
           0, 0, 0, 0, 0, 0, 0, 0.94f * vis);
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, 240.0f }, 0, GFX_VEU_TOPO,
           0, 0, 0, 0, 0, 0, 0, 0.55f * vis);

  // Text cascades: each line 0.45 s after previous.
  {
    float y = SCR_BASE, k[5];
    int i, ok_home = app_na_home();
    char meta[256], sin[400];
    TxtLinha l_meta, l_sin, l_source, l_ok = { 0 };
    for (i = 0; i < 5; i++) {
      float e = smooth_step((t - 1.6f - (float)i * 0.45f) / 0.8f);
      k[i] = e * (1.0f - ext) * a;
    }
    meta_from(c, meta, sizeof meta);
    first_sentence(c->sinopse, sin, sizeof sin);
    l_source = txt_linha(TXT_CAPTION2, c->naLista ? i18n("Na sua lista")
                                     : c->progresso > 0 ? i18n("Continuar assistindo")
                                     : i18n("No seu catálogo"), 190, 194, 204, 255);
    l_meta = txt_linha_corta(TXT_HERO_META, meta, 226, 228, 234, 255, SCR_TEXT_W);
    l_sin = sin[0] ? txt_linha_corta(TXT_HERO_SIN, sin, 200, 204, 212, 255, SCR_TEXT_W)
                   : (TxtLinha){ 0 };
    if (ok_home) l_ok = txt_linha(TXT_CAPTION2, i18n("OK para abrir"), 240, 242, 246, 255);
    // Bottom to top: prompt, synopsis, meta, title, source.
    if (l_ok.tex) {
      float pw = (float)l_ok.w + 44.0f, ph = 46.0f;
      GfxRect pill = { SCR_X, y - ph, pw, ph };
      float rise = (1.0f - k[4]) * 12.0f;
      pill.y += rise;
      gfx_cor(pill, 0.5f, 1, 1, 1, 0.16f * k[4]);
      txt_desenhar_alpha(l_ok, SCR_X + 22.0f, pill.y + (ph - (float)l_ok.h) * 0.5f, k[4]);
      y -= ph + 34.0f;
    }
    if (l_sin.tex) {
      y -= (float)l_sin.h;
      txt_desenhar_alpha(l_sin, SCR_X, y + (1.0f - k[3]) * 12.0f, k[3]);
      y -= 16.0f;
    }
    y -= (float)l_meta.h;
    txt_desenhar_alpha(l_meta, SCR_X, y + (1.0f - k[2]) * 12.0f, k[2]);
    y -= 26.0f;
    if (tex_logo) {
      float ap = tex_aspecto(c->logo), w, h;
      if (ap <= 0.0f) ap = 4.0f;
      h = SCR_LOGO_H; w = h * ap;
      if (w > SCR_LOGO_W) { w = SCR_LOGO_W; h = w / ap; }
      y -= h;
      {
        GfxModo m = tex_marca_escura(c->logo) ? GFX_MARCA : GFX_TEXTO;
        gfx_rect((GfxRect){ SCR_X, y + (1.0f - k[1]) * 14.0f, w, h }, tex_logo, m,
                 0, 0, 0, 0.0f, 1, 1, 1, k[1]);
      }
    } else {
      TxtLinha lt = txt_linha_corta(TXT_TITULO1, c->titulo, 255, 255, 255, 255, SCR_TEXT_W);
      y -= (float)lt.h;
      txt_desenhar_alpha(lt, SCR_X, y + (1.0f - k[1]) * 14.0f, k[1]);
    }
    y -= 22.0f + (float)l_source.h;
    txt_desenhar_alpha(l_source, SCR_X, y + (1.0f - k[0]) * 10.0f, 0.8f * k[0]);
  }

  // Showcase time indicator
  (void)time_str;
  {
    float w = 150.0f, x = NV_TELA_W - 110.0f - w, y = NV_TELA_H - 92.0f;
    float p = t / SCREENSAVER_ITEM_S;
    if (p > 1.0f) p = 1.0f;
    gfx_cor((GfxRect){ x, y, w, 3.0f }, 0.5f, 1, 1, 1, 0.14f * a);
    gfx_cor((GfxRect){ x, y, w * p, 3.0f }, 0.5f, 1, 1, 1, 0.70f * a);
  }
}

void screensaver_draw(unsigned now) {
  // Screensaver fades smoothly over black, never abruptly.
  float a = smooth_step(active_time / 1.2f);
  if (a <= 0.0f) return;
  if (current_style == ESM_ESTILO_VITRINE) draw_showcase(now, a);
  else draw_clock(now, a);
}

#endif

// Backward-compatibility wrappers
void descanso_quadro(unsigned agora, float dt, int ativo, int estilo, int fonte) {
  screensaver_frame(agora, dt, ativo, estilo, fonte);
}
void descanso_desenhar(unsigned agora) {
  screensaver_draw(agora);
}
int descanso_indice_em_tela(void) {
  return screensaver_onscreen_index();
}
void descanso_pedir_abrir(void) {
  screensaver_request_open();
}
int descanso_pedido_abrir(void) {
  return screensaver_requested_open();
}
int descanso_item_serve(int temFundo, int temTitulo, int naLista, int progresso, int fonte) {
  return screensaver_item_valid(temFundo, temTitulo, naLista, progresso, fonte);
}
void descanso_rota(float s, float *px, float *py) {
  screensaver_path(s, px, py);
}
