#include "login.h"
#include "iconeapp.h"
#include "logoapp.h"
#include "session.h"
#include "qr.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "teclado.h"
#include "ponteiro.h"
#include "ajustes.h"
#include "idioma.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// QR is the primary TV login route: measured codes contain 32 hexadecimal digits and URLs about 63 characters.
#define LG_QR_SIZE       440.0f
#define LG_BLOCK_W      1100.0f
#define LG_PILL_W        360.0f
#define LG_PILL_H         76.0f
#define LG_EMAIL_PILL_W  560.0f
#define LG_FIELD_W       760.0f
#define LG_FIELD_H        88.0f
#define LG_SHOW_W     150.0f
// Keep the four-module quiet zone inside the texture so layout changes cannot remove it.
#define LG_QR_MARGIN       4

static float button_animation;
static float pulse;

// Email login is an alternative for users without another device. Fields use the existing platform keyboard modal.
enum { LG_QR = 0, LG_EMAIL };
enum { LE_EMAIL = 0, LE_PASSWORD, LE_SHOW, LE_SIGN_IN, LE_QR, LE_N };
static int mode, focus, qr_focus;
static int open_field = -1;
static char email[TECLADO_LONGO + 1], password[TECLADO_LONGO + 1];
static char notice[160];          // Local validation message.
static float focus_animation[LE_N], qr_focus_animation[2];
static int waiting;            // Email request is pending.
static int password_visible;         // Show password in the field and keyboard modal.

// Email accepts address characters; passwords accept all printable ASCII, including spaces.
static const char *EMAIL_CHARACTERS = "abcdefghijklmnopqrstuvwxyz0123456789@._-+";
static const char *PASSWORD_CHARACTERS =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
  " .,_-@!#$%&*+=?/\\:;'\"()[]{}<>^~`|";

static void clear_text(char *p, size_t n) { volatile char *v = p; while (n--) *v++ = 0; }

// Retry appears only after a failed request.
static int qr_has_retry(void) {
  SessionState st = session_state();
  return st == SESSION_ERROR || st == SESSION_LOGGED_OUT;
}
static int qr_button_count(void) { return session_available() ? (qr_has_retry() ? 2 : 1) : 0; }
// Email button slot on the QR screen.
static int qr_email_slot(void) { return qr_has_retry() ? 1 : 0; }

static void open_email(void) {
  mode = LG_EMAIL; focus = email[0] ? LE_PASSWORD : LE_EMAIL; notice[0] = 0;
}
static void return_to_qr(void) {
  mode = LG_QR; qr_focus = 0; notice[0] = 0;
  clear_text(password, sizeof password);
  if (!session_is_logged_in() && session_state() != SESSION_WAITING &&
      session_state() != SESSION_REQUESTING && session_state() != SESSION_EMAIL)
    session_login_start();
}
static void open_input(int c) {
  open_field = c;
  if (c == LE_EMAIL) {
    teclado_abrir_com("Email", "Your Nuvio account email.", 120, EMAIL_CHARACTERS, email);
    teclado_tipo(TECLADO_TIPO_EMAIL);
  } else {
    // Keep the current password in the masked modal so a correction does not require typing it again.
    teclado_abrir_com("Password", "Your Nuvio account password. It is not shown on screen.", 128, PASSWORD_CHARACTERS, password);
    teclado_tipo(TECLADO_TIPO_SENHA);
    teclado_mascarar(!password_visible);
  }
}
static void submit_email(void) {
  if (session_state() == SESSION_EMAIL) return;
  if (!email[0] || !password[0]) {
    snprintf(notice, sizeof notice, "%s", "Enter your email and password.");
    focus = email[0] ? LE_PASSWORD : LE_EMAIL;
    return;
  }
  notice[0] = 0;
  session_login_email(email, password);
  waiting = session_state() == SESSION_EMAIL;
  // Clear the screen's password immediately after dispatch.
  clear_text(password, sizeof password);
}
static void activate_email(void) {
  switch (focus) {
    case LE_EMAIL: case LE_PASSWORD: open_input(focus); break;
    case LE_SHOW: password_visible = !password_visible; break;
    case LE_SIGN_IN: submit_email(); break;
    default: return_to_qr(); break;
  }
}

// Pointer focus selects the target; pointer activation follows the normal OK action.
static void pointer_email_focus(int i, int b) { (void)b; if (mode == LG_EMAIL) focus = i; }
static void pointer_qr_focus(int i, int b) { (void)b; if (mode == LG_QR) qr_focus = i; }

static GLuint qr_texture;
static char   qr_content[512];

// The striped background is shared with the splash. Load on first draw and release after login; its 1280x720 size is sufficient.
static char   background_path[600];
static GLuint background_texture;
static int    background_attempted;

void login_set_assets(const char *asset_directory) {
  snprintf(background_path, sizeof background_path, "%s/marcas/login-fundo.jpg", asset_directory ? asset_directory : ".");
}

static void load_background(void) {
  SDL_Surface *s, *c;
  background_attempted = 1;
  if (!background_path[0]) return;
  s = IMG_Load(background_path);
  if (!s) { printf("[login] background unavailable (%s)\n", background_path); return; }
  c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);   // RGBA bytes.
  SDL_FreeSurface(s);
  if (!c) return;
  glGenTextures(1, &background_texture);
  glBindTexture(GL_TEXTURE_2D, background_texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c->w, c->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, c->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);   // Invalidate the renderer's last bound texture.
  SDL_FreeSurface(c);
}

int login_draw_background(float alpha) {
  if (!background_attempted) load_background();
  if (!background_texture) return 0;
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, background_texture, GFX_TEXTO,
           0, 0, 0, 0.0f, 1, 1, 1, alpha);
  return 1;
}

void login_release(void) {
  if (background_texture) { gfx_tex_esquecer(background_texture); glDeleteTextures(1, &background_texture); background_texture = 0; }
  background_attempted = 0;
}   // Cache the drawn QR content across frames.

// Upload the QR as one texture; a version-4 symbol would otherwise require 1089 draw calls per frame.
static void create_qr_texture(const char *text) {
  Qr q;
  int n, side, x, y;
  unsigned char *px;
  if (!text || !text[0]) return;
  if (!strcmp(qr_content, text) && qr_texture) return;
  if (!qr_gerar(&q, text)) { printf("[login] QR content exceeds supported size\n"); return; }

  side = q.lado + 2 * LG_QR_MARGIN;
  px = (unsigned char *)malloc((size_t)side * side * 3);
  if (!px) return;
  memset(px, 255, (size_t)side * side * 3);   // White background includes the quiet zone.
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) {
        size_t i = ((size_t)(y + LG_QR_MARGIN) * side + (x + LG_QR_MARGIN)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }

  if (!qr_texture) glGenTextures(1, &qr_texture);
  glBindTexture(GL_TEXTURE_2D, qr_texture);
  // Nearest filtering keeps adjacent QR modules sharp for phone cameras.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, side, side, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(px);
  n = snprintf(qr_content, sizeof qr_content, "%s", text);
  (void)n;
}

void login_init(void) {
  button_animation = 0.0f;
  pulse = 0.0f;
  mode = LG_QR; focus = LE_EMAIL; qr_focus = 0; open_field = -1; notice[0] = 0; waiting = 0;
  password_visible = 0;
  clear_text(password, sizeof password);
  // Request a code immediately on screen entry so installation does not require an extra OK press.
  if (!session_is_logged_in()) session_login_start();
}

void login_event(const SDL_Event *e) {
  SDL_Keycode k;
  int ok, back;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  ok = (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && !e->key.repeat;
  back = k == SDLK_AC_BACK || k == SDLK_ESCAPE || e->key.keysym.scancode == NV_SCANCODE_BACK;
  if (mode == LG_EMAIL) {
    if (back) { return_to_qr(); return; }
    if (session_state() == SESSION_EMAIL) return;   // While awaiting the server, ignore input. Vertical navigation skips the password visibility button.
    if (k == SDLK_UP)
      focus = focus == LE_QR ? LE_SIGN_IN : focus == LE_SIGN_IN ? LE_PASSWORD : LE_EMAIL;
    else if (k == SDLK_DOWN)
      focus = focus == LE_EMAIL ? LE_PASSWORD : focus == LE_SIGN_IN || focus == LE_QR ? LE_QR : LE_SIGN_IN;
    else if (k == SDLK_RIGHT && focus == LE_PASSWORD) focus = LE_SHOW;
    else if (k == SDLK_LEFT && focus == LE_SHOW) focus = LE_PASSWORD;
    else if (ok) activate_email();
    return;
  }
  if (k == SDLK_UP && qr_focus > 0) qr_focus--;
  else if (k == SDLK_DOWN && qr_focus + 1 < qr_button_count()) qr_focus++;
  else if (ok && qr_button_count()) {
    if (qr_focus >= qr_button_count()) qr_focus = qr_button_count() - 1;
    if (qr_focus == qr_email_slot()) { open_email(); return; }
    // Retry only failed requests; restarting a displayed code would invalidate the code being entered on the phone.
    if (session_state() == SESSION_ERROR || session_state() == SESSION_LOGGED_OUT)
      session_login_start();
  }
}

void login_update(float dt, Uint32 now) {
  int i, r;
  button_animation = anim_mola(button_animation, 1.0f, dt, NV_MOLA_FOCO);
  pulse += dt;
  teclado_atualizar(dt, now);
  r = teclado_resultado();
  if (r != TECLADO_NADA && open_field >= 0) {
    int advance = 0;
    if (open_field == LE_PASSWORD) password_visible = !teclado_mascarado();
    if (r == TECLADO_PRONTO) {
      // Keyboard completion advances email to password, then password to Sign in.
      if (open_field == LE_EMAIL) {
        snprintf(email, sizeof email, "%s", teclado_texto());
        focus = LE_PASSWORD;
        advance = 1;
      } else {
        snprintf(password, sizeof password, "%s", teclado_texto());
        focus = LE_SIGN_IN;
      }
      notice[0] = 0;
    }
    teclado_esquecer();
    open_field = -1;
    if (advance) open_input(LE_PASSWORD);
  }
  // After failure, focus the field requiring correction. The pending flag tracks results between frames.
  if (waiting && session_state() != SESSION_EMAIL) {
    waiting = 0;
    if (mode == LG_EMAIL && !session_is_logged_in()) focus = LE_PASSWORD;
  }
  if (qr_focus >= qr_button_count() && qr_button_count()) qr_focus = qr_button_count() - 1;
  for (i = 0; i < LE_N; i++)
    focus_animation[i] = anim_mola(focus_animation[i], mode == LG_EMAIL && focus == i && !teclado_aberto() ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);
  for (i = 0; i < 2; i++)
    qr_focus_animation[i] = anim_mola(qr_focus_animation[i], mode == LG_QR && qr_focus == i ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
}

// Focused button pills are bright; unfocused pills use the existing glass material.
static void button_pill(float cx, float y, float w, const char *label, float k, float alpha,
                   PonteiroFn on_focus, int i) {
  GfxRect pill = { cx - w * 0.5f, y, w, LG_PILL_H };
  TxtLinha t;
  int tone = k >= 0.5f ? 24 : 236;
  gfx_cor(pill, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, anim_mistura(0.12f, 0.92f, k) * alpha);
  t = txt_linha_corta(TXT_BODY, label, tone, tone, tone + 2 > 255 ? 255 : tone + 2, 255, w - 48.0f);
  txt_desenhar_alpha(t, cx - t.w * 0.5f, y + (LG_PILL_H - t.h) * 0.5f, alpha);
  if (on_focus) ponteiro_alvo(pill.x, pill.y, pill.w, pill.h, on_focus, NULL, i, 0);
}

static void centered_line(TxtEstilo style, const char *s, int r, int g, int b,
                          float y, float alpha) {
  TxtLinha l = txt_linha_corta(style, s, r, g, b, 255, LG_BLOCK_W);
  txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, y, alpha);
}

// Email field label above its value or password dots.
static void draw_field(float y, int i, const char *label, const char *value, int masked) {
  float x = (NV_TELA_W - LG_FIELD_W) * 0.5f, k = focus_animation[i];
  GfxRect cx = { x, y + 42.0f, LG_FIELD_W, LG_FIELD_H };
  float ar, ag, ab;
  TxtLinha r = txt_linha(TXT_CAPTION, label, 176, 178, 186, 255);
  txt_desenhar_alpha(r, x + 4.0f, y, 1.0f);
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(cx, NV_RAIO_CARD, 1.0f, 1.0f, 1.0f, anim_mistura(0.07f, 0.13f, k));
  if (k > 0.01f)
    gfx_vidro_aro((GfxRect){ cx.x - 6, cx.y - 6, cx.w + 12, cx.h + 12 }, NV_RAIO_CARD, 2.5f,
                  ar, ag, ab, 0.95f * k);
  ponteiro_alvo(cx.x, y, cx.w, cx.h + 42.0f, pointer_email_focus, NULL, i, 0);
  if (value[0]) {
    char dots[TECLADO_LONGO * 3 + 1];
    TxtLinha t;
    float width = cx.w - 48.0f - (i == LE_PASSWORD ? LG_SHOW_W + 12.0f : 0.0f);
    if (masked) {
      size_t j, n = strlen(value);
      for (j = 0; j < n && j < TECLADO_LONGO; j++) memcpy(dots + j * 3, "\xE2\x80\xA2", 3);
      dots[j * 3] = 0;
    }
    // User-entered text is drawn literally.
    t = txt_linha(TXT_HEADLINE, masked ? dots : value, 240, 242, 248, 255);
    gfx_recorte(cx.x + 24.0f, cx.y, width, cx.h);
    txt_desenhar_alpha(t, cx.x + 24.0f - (t.w > width ? t.w - width : 0.0f),
                       cx.y + (cx.h - t.h) * 0.5f, 1.0f);
    gfx_sem_recorte();
  }
}

static void draw_email(float y, Uint32 now) {
  SessionState st = session_state();
  const char *failure = session_error_email();
  (void)now;
  centered_line(TXT_BODY, "Use the email and password of your Nuvio account.", 176, 178, 186, y, 1.0f);
  y += 84.0f;
  draw_field(y, LE_EMAIL, "Email", email, 0);
  y += 42.0f + LG_FIELD_H + 30.0f;
  draw_field(y, LE_PASSWORD, "Password", password, !password_visible);
  // Show/hide control stays on the right inside the password field.
  { float x = (NV_TELA_W + LG_FIELD_W) * 0.5f - LG_SHOW_W - 12.0f;
    float yb = y + 42.0f + (LG_FIELD_H - 60.0f) * 0.5f, k = focus_animation[LE_SHOW];
    GfxRect b = { x, yb, LG_SHOW_W, 60.0f };
    int tone = k >= 0.5f ? 24 : 220;
    TxtLinha t = txt_linha(TXT_CAPTION, password_visible ? "Hide" : "Show", tone, tone, tone, 255);
    gfx_cor(b, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, anim_mistura(0.10f, 0.92f, k));
    txt_desenhar_alpha(t, b.x + (b.w - t.w) * 0.5f, b.y + (b.h - t.h) * 0.5f, 1.0f);
    ponteiro_alvo(b.x, b.y, b.w, b.h, pointer_email_focus, NULL, LE_SHOW, 0); }
  y += 42.0f + LG_FIELD_H + 48.0f;
  button_pill(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, st == SESSION_EMAIL ? "Signing in…" : "Sign in",
         focus_animation[LE_SIGN_IN], 1.0f, pointer_email_focus, LE_SIGN_IN);
  y += LG_PILL_H + 24.0f;
  button_pill(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, "Use the QR code",
         focus_animation[LE_QR], 1.0f, pointer_email_focus, LE_QR);
  y += LG_PILL_H + 36.0f;
  if (notice[0]) centered_line(TXT_BODY, notice, 236, 108, 108, y, 1.0f);
  else if (st != SESSION_EMAIL && failure[0]) {
    // The Rust backend supplies the English error text.
    TxtLinha l = txt_linha_corta(TXT_BODY, failure, 236, 108, 108, 255, LG_BLOCK_W);
    txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, y, 1.0f);
  }
}

void login_draw(Uint32 now) {
  SessionState st = session_state();
  float y;
  (void)now;

  { GfxRect screen = { 0, 0, NV_TELA_W, NV_TELA_H };
    if (!login_draw_background(1.0f))
      gfx_cor(screen, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f); }
  // This modal covers the full screen and consumes pointer input.
  ponteiro_camada();
  ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);

  logoapp_marca((GfxRect){ NV_MARGEM_X, NV_MARGEM_Y, 72.0f, 72.0f }, 1.0f);

  y = 118.0f;
  centered_line(TXT_TITULO1, "Sign in to your account", 255, 255, 255, y, 1.0f);
  y += 118.0f;

  if (mode == LG_EMAIL && session_available()) {
    draw_email(y, now);
    teclado_desenhar(now);
    return;
  }

  if (!session_available()) {
    // A missing build configuration needs a specific message; repeated login attempts cannot fix it.
    centered_line(TXT_HEADLINE, "Account service is not configured.",
                  236, 108, 108, y, 1.0f);
    centered_line(TXT_BODY,
                  "This build needs the account service URL and public key.",
                  176, 178, 186, y + 62.0f, 1.0f);
    return;
  }

  switch (st) {
    case SESSION_REQUESTING:
      centered_line(TXT_HEADLINE, "Preparing your code…", 210, 212, 220, y, 1.0f);
      button_pill(NV_TELA_W * 0.5f, y + 110.0f, LG_EMAIL_PILL_W, "Sign in with email and password",
             qr_focus_animation[0], 1.0f, pointer_qr_focus, 0);
      break;

    case SESSION_WAITING: {
      const char *url = session_login_url();
      centered_line(TXT_BODY, "Point your phone camera at the code:",
                    176, 178, 186, y, 1.0f);
      y += 62.0f;

      create_qr_texture(url);
      if (qr_texture) {
        // The rounded light frame makes the QR and its quiet zone read as one card.
        GfxRect moldura = { (NV_TELA_W - LG_QR_SIZE - 32.0f) * 0.5f, y - 16.0f,
                            LG_QR_SIZE + 32.0f, LG_QR_SIZE + 32.0f };
        GfxRect r = { (NV_TELA_W - LG_QR_SIZE) * 0.5f, y, LG_QR_SIZE, LG_QR_SIZE };
        gfx_cor(moldura, 0.06f, 1.0f, 1.0f, 1.0f, 1.0f);
        gfx_tex_aspect_atual = 0.0f;   // Display without cropping.
        gfx_rect(r, qr_texture, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, 1.0f);
      } else {
        centered_line(TXT_HEADLINE, "could not draw the code",
                      236, 108, 108, y + 100.0f, 1.0f);
      }
      y += LG_QR_SIZE + 42.0f;

      // The small text URL provides a fallback for users without a camera.
      if (url[0]) centered_line(TXT_CAPTION, url, 150, 152, 160, y, 1.0f);
      y += 46.0f;

      // A slow two-second breathing pulse indicates that login is still waiting.
      { float a = 0.5f + 0.5f * sinf(pulse * 3.14159f);
        centered_line(TXT_CAPTION, "Waiting for authorization…",
                      150, 152, 160, y, 0.45f + 0.40f * a); }
      y += 64.0f;
      button_pill(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, "Sign in with email and password",
             qr_focus_animation[0], 1.0f, pointer_qr_focus, 0);
      break;
    }

    case SESSION_EXCHANGING:
      centered_line(TXT_HEADLINE, "Authorized. Signing in…", 210, 212, 220, y, 1.0f);
      break;

    case SESSION_LOGGED_IN:
      centered_line(TXT_HEADLINE, "Done.", 210, 212, 220, y, 1.0f);
      break;

    case SESSION_ERROR:
    case SESSION_LOGGED_OUT:
    default: {
      const char *msg = session_error();
      centered_line(TXT_HEADLINE, msg[0] ? msg : "Could not reach the server.",
                    236, 108, 108, y, 1.0f);
      y += 96.0f;
      button_pill(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, "Try again",
             qr_focus_animation[0], button_animation, pointer_qr_focus, 0);
      button_pill(NV_TELA_W * 0.5f, y + LG_PILL_H + 24.0f, LG_EMAIL_PILL_W, "Sign in with email and password",
             qr_focus_animation[1], button_animation, pointer_qr_focus, 1);
      break;
    }
  }
}

int login_complete(void) { return session_is_logged_in(); }
#ifdef NV_NATIVE_UI_TEST
int login_test_qr_ready(void) { return qr_texture != 0; }
void login_test_email_keyboard_start(void) {
  open_email();
  open_input(LE_EMAIL);
}
void login_test_sign_in(void) { submit_email(); }
#endif
