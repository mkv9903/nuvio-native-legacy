// See performance.h.
#include "performance.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "idioma.h"
#include "idiomacod.h"
#include "tex_cache.h"
#include "cacheboost.h"
#include <stdio.h>
#include <string.h>

#define PERF_HISTORY_N 36   // 36 samples of 3 s: last 1 min 48 s

static float worst_series[PERF_HISTORY_N];
static int   num_series;
static float last_fps, last_worst, last_screen_mb, last_rss;
static int   last_janks, last_screen_tex, last_tex_queue, last_dumps, last_screen_dumps, has_sample;
#ifdef DESEMPENHO_TESTE
static int fixed_mode = -1;
void performance_test_mode(int mode) { fixed_mode = mode; }
#endif

void performance_sample(float fps, float worst_ms, int janks, int screen_tex, float screen_tex_mb,
                        int tex_queue, int dumps, int screen_dumps, float rss_mb) {
  if (num_series == PERF_HISTORY_N) {
    memmove(worst_series, worst_series + 1, sizeof worst_series - sizeof *worst_series);
    num_series--;
  }
  worst_series[num_series++] = worst_ms;
  last_fps = fps; last_worst = worst_ms; last_janks = janks; last_screen_tex = screen_tex; last_screen_mb = screen_tex_mb;
  last_tex_queue = tex_queue; last_dumps = dumps; last_screen_dumps = screen_dumps; last_rss = rss_mb;
  has_sample = 1;
}

int performance_mode(void) {
  int f = ajustes_medidor_desempenho();
#ifdef DESEMPENHO_TESTE
  if (fixed_mode >= 0) f = fixed_mode;
#endif
  if (!has_sample || f < PERF_MINIMAL || f > PERF_EXPANDED) return PERF_OFF;
  return f;
}

#define PERF_TX    243, 242, 239
#define PERF_AMBER 0.910f, 0.722f, 0.290f
#define PERF_GREEN 0.298f, 0.765f, 0.541f
#define PERF_GAP   14.0f     // CT_VAO of island: time | line | content
#define PERF_DOT   9.0f

static int is_slow(void) { return last_fps < 45.0f; }

static void format_num(char *d, size_t t, double v, int decimals) {
  char *p;
  snprintf(d, t, "%.*f", decimals, v);
  if ((p = strchr(d, '.')) != NULL) *p = idioma_ponto_decimal(ajustes_idioma()) ? '.' : ',';
}

static TxtLinha perf_t(TxtEstilo e, const char *s) { return txt_linha(e, s, PERF_TX, 255); }

// --- time line ---------------------------------------------------------------
// Tabular mono numbers: prevents segment width jitter per sample.
typedef struct { TxtLinha fps, worst, ram; } PerfLine;

static float build_line(int mode, PerfLine *L) {
  char a[24], b[24], c[24];
  float w;
  format_num(a, sizeof a, last_fps, 0);
  snprintf(b, sizeof b, "%s fps", a);
  L->fps = is_slow() ? txt_linha(TXT_MONO18B, b, 232, 184, 74, 255) : perf_t(TXT_MONO18B, b);
  w = PERF_GAP + 1.5f + PERF_GAP + PERF_DOT + 10.0f + (float)L->fps.w;
  L->worst.w = L->ram.w = 0;
  if (mode == PERF_MINIMAL) {
    format_num(a, sizeof a, last_worst, 0);
    snprintf(b, sizeof b, "%s ms", a);
    snprintf(c, sizeof c, "%.0f MB", last_rss);
    L->worst = perf_t(TXT_MONO16, b);
    L->ram = perf_t(TXT_MONO16, c);
    w += 16.0f + (float)L->worst.w + 14.0f + (float)L->ram.w;
  }
  return w;
}

float performance_line_width(int mode) {
  PerfLine L;
  if (mode <= PERF_OFF || !has_sample) return 0.0f;
  return build_line(mode, &L);
}

void performance_line(int mode, float x, float yc, float a) {
  PerfLine L;
  if (mode <= PERF_OFF || !has_sample || a < 0.01f) return;
  build_line(mode, &L);
  x += PERF_GAP;
  // Divider between time and meter: same as card beside clock.
  gfx_cor((GfxRect){ x, yc - 13.0f, 1.5f, 26.0f }, 0.0f, 1.0f, 1.0f, 1.0f, 0.22f * a);
  x += 1.5f + PERF_GAP;
  if (is_slow()) gfx_cor((GfxRect){ x, yc - PERF_DOT * 0.5f, PERF_DOT, PERF_DOT }, 0.5f, PERF_AMBER, a);
  else gfx_cor((GfxRect){ x, yc - PERF_DOT * 0.5f, PERF_DOT, PERF_DOT }, 0.5f, PERF_GREEN, a);
  x += PERF_DOT + 10.0f;
  txt_desenhar_alpha(L.fps, x, yc - (float)L.fps.h * 0.5f, a);
  x += (float)L.fps.w;
  if (mode == PERF_MINIMAL) {
    x += 16.0f;
    txt_desenhar_alpha(L.worst, x, yc - (float)L.worst.h * 0.5f, a * 0.7f);
    x += (float)L.worst.w + 14.0f;
    txt_desenhar_alpha(L.ram, x, yc - (float)L.ram.h * 0.5f, a * 0.7f);
  }
}

// --- Compact: second line ----------------------------------------------------
#define MN_PAD 24.0f
#define MN_H   38.0f

static TxtLinha compact_line(void) {
  char p[16], s[160], q[48], t[48];
  format_num(p, sizeof p, last_worst, 0);
  snprintf(q, sizeof q, i18n("pior %s ms"), p);
  snprintf(t, sizeof t, i18n("%d texturas"), last_screen_tex);
  snprintf(s, sizeof s, "%s · %d %s · %.0f MB · %s", q, last_janks, i18n("janks"), last_rss, t);
  return perf_t(TXT_MONO16, s);
}

// --- Expanded: panel ---------------------------------------------------------
#define GR_W    520.0f
#define GR_PAD  28.0f
#define GR_STAT (1 + 11 + 20.6f + 11)
// F07: one more stat line ("Cache de seek") where player has cache.
#define GR_H    (18 + 22 + 14 + 96 + 30 + (4 + (cacheboost_suportado() ? 1 : 0)) * GR_STAT + 12 + 29 + 8 + 24)

static float stat(const char *k, const char *v, float x, float y, float w, float a) {
  TxtLinha lk = perf_t(TXT_AJ_ESTADO, k), lv = perf_t(TXT_AJ_ESTADO, v);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 12, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 12, 0.88f * a);
  return GR_STAT;
}

static void panel(GfxRect r, float a) {
  float x = r.x + GR_PAD, cw = r.w - 2 * GR_PAD, y = r.y;
  char v[64], p[32];
  int i, items, pending, hot;
  long bytes, bytes_hot, ceiling = tex_orcamento_bytes();
  tex_estatisticas(&items, &pending, &bytes, &hot, &bytes_hot);
  // Divider below time line: expanded island header (plrilha does same).
  gfx_cor((GfxRect){ r.x, r.y, r.w, 1 }, 0, 1, 1, 1, 0.07f * a);
  y += 18;
  // "pior 21 ms · 0 janks" left side, bright white numbers; "a cada 3 s" on right.
  {
    char nb[16], b[16];
    TxtLinha l1, l2, l3, l4, l5, d;
    float xd = x, base = y + 18, space = (float)(txt_largura(TXT_AJ_ESTADO, "a a") - txt_largura(TXT_AJ_ESTADO, "aa"));
    format_num(nb, sizeof nb, last_worst, 0); snprintf(p, sizeof p, "%s ms", nb);
    snprintf(b, sizeof b, "%d", last_janks);
    l1 = perf_t(TXT_AJ_ESTADO, i18n("pior")); l2 = perf_t(TXT_LOG_18B, p); l3 = perf_t(TXT_AJ_ESTADO, "·");
    l4 = perf_t(TXT_LOG_18B, b); l5 = perf_t(TXT_AJ_ESTADO, i18n("janks"));
    d = perf_t(TXT_G18M, i18n("a cada 3 s"));
    txt_desenhar_alpha(l1, xd, base - l1.h * 0.78f, 0.6f * a); xd += l1.w + space;
    txt_desenhar_alpha(l2, xd, base - l2.h * 0.78f, a);        xd += l2.w + space;
    txt_desenhar_alpha(l3, xd, base - l3.h * 0.78f, 0.6f * a); xd += l3.w + space;
    txt_desenhar_alpha(l4, xd, base - l4.h * 0.78f, a);        xd += l4.w + space;
    txt_desenhar_alpha(l5, xd, base - l5.h * 0.78f, 0.6f * a);
    txt_desenhar_alpha(d, x + cw - d.w, base - d.h * 0.78f, 0.5f * a);
    y += 22 + 14;
  }
  // Worst frame chart: one bar per sample, amber above 33 ms
  {
    float gx = x + 10, gy = y + 10, gw = cw - 20, gh = 74, pw = gw / PERF_HISTORY_N, ly = gy + gh * (1 - 33.0f / 70.0f), dx;
    gfx_cor((GfxRect){ x, y, cw, 90 }, 12.0f / 90, 0, 0, 0, 0.18f * a);
    for (i = 0; i < num_series; i++) {
      float vv = worst_series[i] / 70.0f, hh, bx = gx + gw - (num_series - i) * pw;
      if (vv > 1) vv = 1;
      hh = gh * vv; if (hh < 3) hh = 3;
      if (worst_series[i] > 33) gfx_cor((GfxRect){ bx, gy + gh - hh, pw - 2, hh }, 0, PERF_AMBER, 0.6f * a);
      else gfx_cor((GfxRect){ bx, gy + gh - hh, pw - 2, hh }, 0, 0.953f, 0.949f, 0.937f, 0.18f * a);
    }
    for (dx = 0; dx < gw; dx += 10) gfx_cor((GfxRect){ gx + dx, ly, 4, 1 }, 0, 1, 1, 1, 0.22f * a);
    { TxtLinha l = perf_t(TXT_AJ_MINI12, "33 ms"); txt_desenhar_alpha(l, gx + 4, ly - 6 - l.h, 0.45f * a); }
    y += 90 + 6;
  }
  {
    char ha[48];
    TxtLinha la, lb;
    int sec = num_series * 3;
    if (sec < 60) snprintf(ha, sizeof ha, i18n("pior quadro · há %d s"), sec);
    else snprintf(ha, sizeof ha, i18n("pior quadro · há %d min"), (sec + 30) / 60);
    la = perf_t(TXT_AJ_MINI14, ha); lb = perf_t(TXT_AJ_MINI14, i18n("agora"));
    txt_desenhar_alpha(la, x, y, 0.4f * a); txt_desenhar_alpha(lb, x + cw - lb.w, y, 0.4f * a);
    y += 16 + 14;
  }
  snprintf(v, sizeof v, "%.0f MB", last_rss);
  y += stat(i18n("Memória do app"), v, x, y, cw, a);
  snprintf(v, sizeof v, "%d · %.0f MB", last_screen_tex, last_screen_mb);
  y += stat(i18n("Texturas na tela"), v, x, y, cw, a);
  snprintf(v, sizeof v, "%d", last_tex_queue);
  y += stat(i18n("Fila de texturas"), v, x, y, cw, a);
  snprintf(v, sizeof v, i18n("%d · %d da tela"), last_dumps, last_screen_dumps);
  y += stat(i18n("Despejadas"), v, x, y, cw, a);
  // F07: what seek cache holds now / limit.
  if (cacheboost_suportado()) {
    char c[48];
    cacheboost_cache_texto(c, sizeof c);
    y += stat(i18n("Cache de seek"), i18n(c), x, y, cw, a);
  }
  gfx_cor((GfxRect){ x, y, cw, 1 }, 0, 1, 1, 1, 0.07f * a);
  y += 12;
  {
    TxtLinha k = perf_t(TXT_ILHA_GENERO, i18n("Cache de imagens")), l;
    float fr = ceiling > 0 ? (float)bytes / (float)ceiling : 0;
    snprintf(v, sizeof v, "%ld / %ld MB", bytes >> 20, ceiling >> 20);
    l = perf_t(TXT_ILHA_GENERO, v);
    txt_desenhar_alpha(k, x, y, 0.45f * a);
    txt_desenhar_alpha(l, x + cw - l.w, y, 0.88f * a);
    y += 20 + 9;
    if (fr > 1) fr = 1;
    gfx_cor((GfxRect){ x, y, cw, 8 }, 0.5f, 1, 1, 1, 0.07f * a);
    if (fr > 0.005f) gfx_cor((GfxRect){ x, y, cw * fr, 8 }, 0.5f, PERF_GREEN, 0.8f * a);
  }
}

void performance_body_size(int mode, float *w, float *h) {
  float cw = 0.0f, ch = 0.0f;
  if (has_sample && mode == PERF_COMPACT) { TxtLinha l = compact_line(); cw = (float)l.w + 2 * MN_PAD; ch = MN_H; }
  else if (has_sample && mode == PERF_EXPANDED) { cw = GR_W; ch = GR_H; }
  if (w) *w = cw;
  if (h) *h = ch;
}

void performance_body(int mode, GfxRect r, float a) {
  if (!has_sample || a < 0.01f) return;
  if (mode == PERF_COMPACT) {
    TxtLinha l = compact_line();
    txt_desenhar_alpha(l, r.x + (r.w - (float)l.w) * 0.5f, r.y + 12.0f - (float)l.h * 0.5f, 0.7f * a);
  } else if (mode == PERF_EXPANDED) panel(r, a);
}

#ifdef DESEMPENHO_TESTE
void performance_test_series(const float *worst, int n) {
  int i;
  num_series = 0;
  for (i = 0; i < n && i < PERF_HISTORY_N; i++) worst_series[num_series++] = worst[i];
}
#endif

// Backward-compatibility wrappers
void desempenho_amostra(float fps, float piorMs, int janks, int texTela, float texTelaMb,
                        int filaTex, int despejos, int despejosTela, float rssMb) {
  performance_sample(fps, piorMs, janks, texTela, texTelaMb, filaTex, despejos, despejosTela, rssMb);
}
int   desempenho_forma(void) { return performance_mode(); }
float desempenho_linha_w(int forma) { return performance_line_width(forma); }
void  desempenho_linha(int forma, float x, float yc, float a) { performance_line(forma, x, yc, a); }
void  desempenho_corpo_tam(int forma, float *w, float *h) { performance_body_size(forma, w, h); }
void  desempenho_corpo(int forma, GfxRect r, float a) { performance_body(forma, r, a); }

#ifdef DESEMPENHO_TESTE
void desempenho_teste_serie(const float *pior, int n) { performance_test_series(pior, n); }
void desempenho_teste_forma(int forma) { performance_test_mode(forma); }
#endif
