// TEST HARNESS FOR TIZEN PLAYER. Not part of production builds.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <stdio.h>
#include "../src/video.h"

EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay(void) {
  printf("[TEST] --- start ---\n");

  printf("[TEST] video_iniciar -> %d\n", video_iniciar());

  // Window BEFORE playing:
  video_janela(0, 0, 1920, 1080);

  // Remux MKV with HEVC:
  printf("[TEST] video_tocar -> %d\n",
         video_tocar("https://example.invalid/movie.2160p.remux.mkv"));

  printf("[TEST] --- triggered; pump handles remainder ---\n");
  fflush(stdout);
}

// Phase 2, called after mock prepareAsync responds:
EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay_fase2(void) {
  printf("[TEST] --- phase 2 ---\n");
  printf("[TEST] ready=%d playing=%d dur=%.0f\n",
         video_pronto(), video_tocando(), video_duracao());
  printf("[TEST] tracks: audio=%d subtitle=%d\n",
         video_n_audio(), video_n_legenda());
  video_buscar(120.0);
  video_pausar(1);
  video_janela(480, 270, 960, 540);
  fflush(stdout);
}

// Phase 3: ZOOM MODES
EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay_zoom(void) {
  static const float SCALE[] = { 1.00f, 1.15f, 1.34f, 1.55f };
  static const char *NAME[] = { "Original", "Light zoom", "Cinema zoom", "Ultra zoom" };
  int i, qw = video_largura(), qh = video_altura();
  printf("[TEST] --- phase 3: zoom modes ---\n");
  printf("[TEST] frame %dx%d; player.c targets (1920x1080 screen):\n", qw, qh);
  printf("[TEST]   1.15 -> -144,-81 2208x1242 | 1.34 -> -326,-184 2573x1447 |"
         " 1.55 -> -528,-297 2976x1674\n");
  for (i = 0; i < 4; i++) {
    int sw = (int)(qw / SCALE[i] + 0.5f), sh = (int)(qh / SCALE[i] + 0.5f);
    int sx = (qw - sw) / 2, sy = (qh - sh) / 2;
    printf("[TEST] %s (%.2fx): source %d,%d %dx%d\n", NAME[i], SCALE[i],
           sx, sy, sw, sh);
    video_janela_fonte(sx, sy, sw, sh, 0, 0, 1920, 1080);
  }
  fflush(stdout);
}

// Manual pump trigger for tests without full app loop.
EMSCRIPTEN_KEEPALIVE
void nv_teste_bombear(void) { video_bombear(); }

EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay_fim(void) {
  printf("[TEST] --- end ---\n");
  video_parar();
  fflush(stdout);
}
#endif
