// See telemetry.h. Same pattern as news card (novidades134.c): single page,
// ramp entrance, marked in file to avoid asking again.
#include "telemetry.h"
#include "data.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "idiomacod.h"
#define NV_ESCALA_TELA_ATIVA   // measures by active factor screen (escala.h)
#include "escala.h"
#include <stdio.h>
#include <stdlib.h>

#define TL_FILE      "telemetria-perguntado.txt"
#define TL_OPEN_MS   280.0f
#define TL_CLOSE_MS  160.0f

static int   is_open, is_decided;
static float entry_anim;

int telemetry_is_open(void) { return is_open; }

void telemetry_first_time(void) {
#ifndef __EMSCRIPTEN__
  is_decided = 1;   // only on Tizen
  return;
#else
  char *s;
  if (is_decided) return;
  is_decided = 1;
  s = data_read(TL_FILE);
  if (s) { free(s); return; }
  is_open = 1; entry_anim = 0.0f;
#endif
}

static void close_dialog(int yes) {
  is_open = 0;
  ajustes_definir_envio_auto(yes);
  data_write(TL_FILE, yes ? "1\n" : "0\n");   /* marker, not screen text */
  printf("[telemetria] envio automatico: %s\n", yes ? "ligado" : "desligado");
  fflush(stdout);
}

static int focus_idx;   // 0 = "Yes, allow sending", 1 = "Not now"

void telemetry_event(const SDL_Event *e) {
  SDL_Keycode k;
  if (!is_open || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT) { focus_idx = 0; return; }
  if (k == SDLK_RIGHT) { focus_idx = 1; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { close_dialog(focus_idx == 0); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) { close_dialog(0); return; }
}

void telemetry_update(float dt, Uint32 now) {
  (void)now;
  if (!is_open && entry_anim < 0.002f) { entry_anim = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entry_anim = is_open ? 1.0f : 0.0f; return; }
  entry_anim = anim_rampa(entry_anim, is_open ? 1.0f : 0.0f, dt,
                          is_open ? TL_OPEN_MS : TL_CLOSE_MS);
}

// THE CARD IN GLASS UI (log mockup, frame 13, 03/10): modal island,
// kicker + question + entire text, and three cards showing what goes, what
// does not go and when — same information as text, readable from distance.
#define TL_TX 243, 242, 239

static TxtLinha tl_t(TxtEstilo e, const char *s) { return txt_linha(e, s, TL_TX, 255); }

static float tl_caps(TxtEstilo e, const char *s, float track, float x, float y, float a) {
  char up[160];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, TL_TX, x, y, a, track);
}

static void tl_neutral(GfxRect r, float radius_px, float vid_a, float sr, float sg, float sb, float a) {
  if (ajustes_vidro()) gfx_cor(r, radius_px / r.h, 1, 1, 1, vid_a * a);
  else gfx_cor(r, radius_px / r.h, sr, sg, sb, a);
}

static float tl_button(const char *label, float x, float y, int focused, float a) {
  float ar, ag, ab, w;
  int t = ajustes_tinta_foco();
  TxtLinha l = focused ? txt_linha(TXT_ILHA_ITEM, label, t, t, t, 255) : tl_t(TXT_ILHA_ITEM, label);
  w = 28 + l.w + 28;
  if (focused) {
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect((GfxRect){ x - 14, y - 2, w + 28, 60 + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.32f * a);
    gfx_cor((GfxRect){ x, y, w, 60 }, 0.5f, ar, ag, ab, a);
  } else tl_neutral((GfxRect){ x, y, w, 60 }, 30, 0.08f, 0.141f, 0.149f, 0.173f, a);
  txt_desenhar_alpha(l, x + 28, y + (60 - l.h) * 0.5f, (focused ? 1.0f : 0.88f) * a);
  return w;
}

static void telemetry_draw_body(Uint32 now);

void telemetry_draw(Uint32 now) {
  ESCALA_SE_COUBER_INI(1260.0f, 600.0f);
  telemetry_draw_body(now);
  ESCALA_SE_COUBER_FIM();
}

static void telemetry_draw_body(Uint32 now) {
  static const char *const IC[3] = { "aj_activity", "aj_shield-check", "aj_clock" };
  static const char *const K[3] = { "Vai", "Não vai", "Quando" };
  static const char *const T[3] = { "O que o app fez e quanto demorou",
                                    "Senhas, chaves e o caminho dos endereços",
                                    "A sessão anterior ao abrir; esta, a cada minuto" };
  const char *text = "Nas TVs Samsung ainda há travamentos que só aparecem no registro do app. "
                     "Se você deixar, o app manda esse registro sozinho: o da sessão anterior ao abrir "
                     "e o desta a cada minuto. Vai sem senhas nem chaves; tem só o que o app fez e quanto demorou. "
                     "Dá para desligar a qualquer hora em Ajustes › Sobre › Enviar registros sozinho.";
  float a = anim_suave(entry_anim), dy, x, y, w = 1260, cw = w - 116, th, h, card_w, card_h = 0;
  int i;
  (void)now;
  if (entry_anim < 0.002f) return;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, (ajustes_vidro() ? 0.40f : 0.42f) * entry_anim);
  dy = (1.0f - a) * 36.0f;
  th = txt_bloco(TXT_DET_META2, i18n(text), 0, 0, 0, 0, -4000, cw, 31.5f, 0, 6);
  card_w = (cw - 32) / 3.0f;
  for (i = 0; i < 3; i++) {
    float hh = 22 + 16 + 6 + txt_bloco(TXT_AJ_SUB, i18n(T[i]), 0, 0, 0, 0, -4000, card_w - 48 - 64, 26.6f, 0, 4) + 22;
    if (hh < 22 + 48 + 22) hh = 22 + 48 + 22;
    if (hh > card_h) card_h = hh;
  }
  h = 52 + 18 + 8 + 48 + 14 + th + 30 + card_h + 34 + 60 + 48;
  x = (NV_TELA_W - w) * 0.5f;
  y = (gfx_escala() == 1.0f ? 228.0f : (NV_TELA_H - h) * 0.5f) + dy;
  {
    GfxRect r = { x, y, w, h };
    gfx_rect((GfxRect){ r.x - 20, r.y - 6, r.w + 40, r.h + 46 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, (ajustes_vidro() ? 0.36f : 0.45f) * a);
    if (ajustes_vidro()) {
      gfx_cor(r, 36.0f / h, 0.055f, 0.059f, 0.071f, 0.86f * a);
      gfx_luz_canto(r, 36.0f / h, r.w * 0.22f, -r.h * 0.40f, r.h * 0.62f, 1, 1, 1, 0.10f * a);
    } else gfx_cor(r, 36.0f / h, 0.082f, 0.086f, 0.102f, a);
  }
  x += 58; y += 52;
  tl_caps(TXT_MINI, "Samsung", 2.1f, x, y, 0.45f * a);
  y += 18 + 8;
  {
    TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, i18n("Ajude a deixar o app fluido na Samsung"), TL_TX, 255, cw);
    txt_desenhar_alpha(t, x, y, a); y += 48 + 14;
  }
  y += txt_bloco(TXT_DET_META2, i18n(text), TL_TX, x, y, cw, 31.5f, 0.62f * a, 6) + 30;
  for (i = 0; i < 3; i++) {
    float cx = x + i * (card_w + 16);
    tl_neutral((GfxRect){ cx, y, card_w, card_h }, 22, 0.05f, 0.106f, 0.110f, 0.129f, a);
    tl_neutral((GfxRect){ cx + 24, y + 22, 48, 48 }, 15, 0.08f, 0.141f, 0.149f, 0.173f, a);
    gfx_icone((GfxRect){ cx + 36, y + 34, 24, 24 }, IC[i], 0.953f, 0.949f, 0.937f, 0.85f * a);
    tl_caps(TXT_AJ_CAPS13, K[i], 1.82f, cx + 24 + 48 + 16, y + 22, 0.42f * a);
    txt_bloco(TXT_AJ_SUB, i18n(T[i]), TL_TX, cx + 24 + 48 + 16, y + 22 + 16 + 6, card_w - 48 - 64, 26.6f, 0.88f * a, 4);
  }
  y += card_h + 34;
  {
    float bx = x;
    bx += tl_button(i18n("Sim, pode mandar"), bx, y, focus_idx == 0, a) + 12;
    tl_button(i18n("Agora não"), bx, y, focus_idx == 1, a);
    {
      TxtLinha l = tl_t(TXT_ILHA_GENERO, i18n("pergunta uma vez por instalação"));
      txt_desenhar_alpha(l, x + cw - l.w, y + 30 - l.h * 0.5f, 0.45f * a);
    }
  }
}

#ifdef TELEMETRIA_TESTE
void telemetry_test_open(void) { is_open = 1; entry_anim = 1.0f; focus_idx = 0; }
#endif

// Backward-compatibility wrappers
void telemetria_primeira_vez(void) { telemetry_first_time(); }
int  telemetria_aberto(void) { return telemetry_is_open(); }
void telemetria_evento(const SDL_Event *e) { telemetry_event(e); }
void telemetria_atualizar(float dt, Uint32 agora) { telemetry_update(dt, agora); }
void telemetria_desenhar(Uint32 agora) { telemetry_draw(agora); }

#ifdef TELEMETRIA_TESTE
void telemetria_teste_abrir(void) { telemetry_test_open(); }
#endif
