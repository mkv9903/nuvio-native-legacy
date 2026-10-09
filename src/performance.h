// PERFORMANCE METER ON CLOCK ISLAND (log mockup, frames 15 and 16,
// approved 03/10; inside island at owner's request, 03/10 evening).
// Numbers come from FPS= line that main.c already writes every 3 s:
// frames per second, worst frame of window, janks, on-screen textures,
// texture queue and dumps, app memory and image cache. Turned on in Settings >
// This TV's Performance > "Performance meter" (disabled by default). Costs
// a sample every 3 s, not per frame.
//
// NO LONGER A SEPARATE LAYER: used to be its own island opposite clock
// and covered island animations. Now it is CONTENT of the clock island —
// ilha.c outside player, plrilha.c inside — in its material:
//   Minimal   on SAME line as time: separator, dot, "60 fps", worst frame and RAM;
//   Compact   time line with fps, plus a compact second line below;
//   Expanded  island grows below time with entire panel (worst frame chart,
//             memory, textures, cache).
// Whoever draws the island decides when there is space: with alert, activity,
// modal or island flight, meter vanishes (Minimal beside a card, if it fits)
// and returns when free. Below 45 fps (cutoff for [gpu-modos] slow) dot and
// number become amber — the only color.
#ifndef NV_PERFORMANCE_H
#define NV_PERFORMANCE_H
#include <SDL2/SDL.h>
#include "gfx.h"

// Index matches Settings (V_MEDIDOR, ajustes_medidor_desempenho).
enum { PERF_OFF = 0, PERF_MINIMAL, PERF_COMPACT, PERF_EXPANDED };

void performance_sample(float fps, float worst_ms, int janks, int screen_tex, float screen_tex_mb,
                        int tex_queue, int dumps, int screen_dumps, float rss_mb);
// Form chosen in Settings; PERF_OFF while there is no sample.
int  performance_mode(void);
// SEGMENT ON TIME LINE, right after it: width (including gap and front divider;
// 0 = none) and drawing, with `x` at right edge of time and `yc` middle of line.
// In Compact/Expanded it is just dot and fps.
float performance_line_width(int mode);
void  performance_line(int mode, float x, float yc, float a);
// BODY, below time line (Compact and Expanded; 0 x 0 in Minimal). `r` is
// entire area below line, across island width.
void  performance_body_size(int mode, float *w, float *h);
void  performance_body(int mode, GfxRect r, float a);

#ifdef DESEMPENHO_TESTE
void performance_test_series(const float *worst, int n);
void performance_test_mode(int mode);
#endif

// Backward-compatibility shims
#define DS_DESLIGADO PERF_OFF
#define DS_MINIMO    PERF_MINIMAL
#define DS_MENOR     PERF_COMPACT
#define DS_GRANDE    PERF_EXPANDED

void  desempenho_amostra(float fps, float piorMs, int janks, int texTela, float texTelaMb,
                         int filaTex, int despejos, int despejosTela, float rssMb);
int   desempenho_forma(void);
float desempenho_linha_w(int forma);
void  desempenho_linha(int forma, float x, float yc, float a);
void  desempenho_corpo_tam(int forma, float *w, float *h);
void  desempenho_corpo(int forma, GfxRect r, float a);

#ifdef DESEMPENHO_TESTE
void desempenho_teste_serie(const float *pior, int n);
void desempenho_teste_forma(int forma);
#endif

#endif
