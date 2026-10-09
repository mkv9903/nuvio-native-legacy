// Magic Remote pointer: state, hit-testing, and cursor. See pointer.h.
#include "pointer.h"
#include "gfx.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#if (!defined(__APPLE__) && !defined(NV_LINUX_DESKTOP) && !defined(__EMSCRIPTEN__) && !defined(NV_TPK) && !defined(NV_ANDROID)) || \
    defined(NV_PONT_WEBOS_TESTE)
#include <dlfcn.h>
#define NV_PONT_WEBOS 1
#endif

// Synthetic scancodes from SDL_webOS.h (openlgtv SDK). Transcribed because SDK
// header is not included in Mac or Samsung builds.
#define PTR_SC_CURSOR_SHOW 484
#define PTR_SC_CURSOR_HIDE 485

#define PTR_MAX_TARGETS    512
// Stationary for this duration, cursor hides — on webOS system cursor hides as well,
// requested via SDL_webOSCursorVisibility (see pointer_frame).
#define PTR_SLEEP_MS       4000
// Window in which a key OK and a click are considered the SAME press.
#define PTR_DEDUPE_MS      150
// Duration target under cursor must remain stationary before hover can shift focus.
#define PTR_SETTLE_MS      150
#define PTR_WHEEL_MS        70
// AFTER AN ARROW KEY cursor only returns with genuine gesture. Pressing arrow shakes
// Magic Remote; this jitter arrived as motion, reignited cursor and refocused target under it.
#define PTR_ARROW_WINDOW_MS 350
#define PTR_ARROW_THRESHOLD 32.0f
#define PTR_ARROW_PAUSE_MS  300
#define PTR_TOUCH_THRESHOLD 32.0f  // maximum logical displacement of a tap
#define PTR_FINGERS_MAX     16

static PointerTarget targets[2][PTR_MAX_TARGETS];
static int target_count[2];
static int write_buf = 0;          // filled by drawing of this frame
static int ready_buf = 1;          // from previous frame, read by hit-testing

static float px = NV_TELA_W * 0.5f, py = NV_TELA_H * 0.5f;
static int is_visible = 0;
static Uint32 last_motion_time = 0;
static int window_w = 0, window_h = 0;
static Uint32 (*clock_fn)(void) = NULL;

// Identity of target focused by hover most recently.
typedef struct { PointerFn focus, activate; int a, b; int ok; float cx, cy; } Ident;
static Ident hover;
static Uint32 content_moved_at = 0;

static int ok_pending = 0;          // KEYDOWN RETURN delivered, awaiting KEYUP
static int back_pending = 0;
static Ident activate_pending;
static Uint32 clicked_at = 0, ok_key_at = 0;
static int swallow_click_release = 0, swallow_ok_release = 0;
static Uint32 wheel_at = 0;

static int logged_type[8];

// Hidden by arrow: motion must exceed window and threshold.
static int hidden_by_arrow = 0;
static Uint32 arrow_at = 0, arrow_motion_at = 0;
static float arrow_anchor_x, arrow_anchor_y;
static int arrow_anchored = 0;
// We asked compositor to hide its arrow (webOS).
static int system_hidden = 0;

#ifdef NV_ANDROID
static int touch_available = 1;
#else
static int touch_available;
#endif
typedef struct { SDL_TouchID touch_id; SDL_FingerID finger_id; } Finger;
static Finger fingers[PTR_FINGERS_MAX];
static int finger_count, excess_fingers, touch_cancelled;
static float touch_x, touch_y;
static Ident touch_target;
static int touch_no_targets;

static void stop_inertia(void);
static int drag_mode;
static void cancel_touch(void) {
  stop_inertia();
  drag_mode = 0;
  finger_count = excess_fingers = 0;
  touch_cancelled = 1;
  touch_target.ok = 0;
}

#ifdef NV_PONT_WEBOS
static SDL_bool (*system_cursor)(SDL_bool) = NULL;
#endif

static Uint32 now_ms(void) { return clock_fn ? clock_fn() : SDL_GetTicks(); }

void pointer_test_clock(Uint32 (*fn)(void)) { clock_fn = fn; }
void pointer_test_window(int w, int h) { window_w = w; window_h = h; }
void pointer_test_touch(int enabled) { touch_available = enabled != 0; cancel_touch(); }
#ifdef NV_PONT_WEBOS
void pointer_test_system_cursor(SDL_bool (*fn)(SDL_bool)) { system_cursor = fn; }
#endif

float pointer_x(void) { return px; }
float pointer_y(void) { return py; }
int pointer_active(void) { return is_visible || touch_available; }
int pointer_is_active(void) { return pointer_active(); }

// INPUT DIAGNOSTIC (#99).
static Uint32 diag_until = 30000;
static int diag_count;
static const char *type_name(Uint32 t) {
  switch (t) {
    case SDL_MOUSEMOTION: return "MOUSEMOTION";
    case SDL_MOUSEBUTTONDOWN: return "MOUSEBUTTONDOWN";
    case SDL_MOUSEBUTTONUP: return "MOUSEBUTTONUP";
    case SDL_MOUSEWHEEL: return "MOUSEWHEEL";
    case SDL_FINGERDOWN: return "FINGERDOWN";
    case SDL_FINGERUP: return "FINGERUP";
    case SDL_FINGERMOTION: return "FINGERMOTION";
    case SDL_WINDOWEVENT: return "WINDOWEVENT";
    case SDL_SYSWMEVENT: return "SYSWMEVENT";
    case SDL_TEXTINPUT: return "TEXTINPUT";
    case SDL_TEXTEDITING: return "TEXTEDITING";
    case SDL_KEYDOWN: return "KEYDOWN";
    case SDL_KEYUP: return "KEYUP";
    default: return "?";
  }
}

void pointer_diag(const SDL_Event *e) {
  Uint32 now = SDL_GetTicks();
  if (e->type == SDL_KEYDOWN &&
      (int)e->key.keysym.scancode == PTR_SC_CURSOR_SHOW) diag_until = now + 30000;
  if (now > diag_until || diag_count >= 400) return;
  if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
    int sc = (int)e->key.keysym.scancode;
    if (sc != PTR_SC_CURSOR_SHOW && sc != PTR_SC_CURSOR_HIDE &&
        e->key.keysym.sym != SDLK_RETURN) return;
  }
  if (e->type == SDL_MOUSEMOTION) {
    static int n_motion;
    if (++n_motion > 30) return;
  }
  diag_count++;
  switch (e->type) {
    case SDL_MOUSEMOTION:
      printf("[ponteiro-diag] t=%u MOUSEMOTION win=%u which=%u x=%d y=%d rel=%d,%d estado=%u\n",
             now, e->motion.windowID, e->motion.which, e->motion.x, e->motion.y,
             e->motion.xrel, e->motion.yrel, e->motion.state); break;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
      printf("[ponteiro-diag] t=%u %s win=%u which=%u botao=%d x=%d y=%d\n",
             now, type_name(e->type), e->button.windowID, e->button.which,
             e->button.button, e->button.x, e->button.y); break;
    case SDL_MOUSEWHEEL: {
      int mx = -1, my = -1; Uint32 b = SDL_GetMouseState(&mx, &my);
      SDL_Window *f = SDL_GetMouseFocus();
      printf("[ponteiro-diag] t=%u MOUSEWHEEL win=%u which=%u x=%d y=%d estado_mouse=%d,%d botoes=%u foco=%p\n",
             now, e->wheel.windowID, e->wheel.which, e->wheel.x, e->wheel.y,
             mx, my, b, (void *)f); break; }
    case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION:
      printf("[ponteiro-diag] t=%u %s x=%.3f y=%.3f\n", now, type_name(e->type),
             e->tfinger.x, e->tfinger.y); break;
    case SDL_WINDOWEVENT:
      printf("[ponteiro-diag] t=%u WINDOWEVENT sub=%d d1=%d d2=%d\n", now,
             e->window.event, e->window.data1, e->window.data2); break;
    case SDL_KEYDOWN: case SDL_KEYUP: {
      int mx = -1, my = -1; SDL_GetMouseState(&mx, &my);
      printf("[ponteiro-diag] t=%u %s sc=%d sym=%d estado_mouse=%d,%d\n", now,
             type_name(e->type), (int)e->key.keysym.scancode, (int)e->key.keysym.sym, mx, my);
      break; }
    default:
      printf("[ponteiro-diag] t=%u tipo=0x%x (%s)\n", now, e->type, type_name(e->type));
  }
  fflush(stdout);
}

void pointer_init(void) {
  target_count[0] = target_count[1] = 0;
  is_visible = 0;
  hidden_by_arrow = 0; system_hidden = 0;
  memset(&hover, 0, sizeof hover);
  cancel_touch();
#ifndef NV_ANDROID
  touch_available = SDL_GetNumTouchDevices() > 0;
#endif
  {
    SDL_Window *w = SDL_GL_GetCurrentWindow();
    int ww = 0, wh = 0, dw = 0, dh = 0;
    if (w) { SDL_GetWindowSize(w, &ww, &wh); SDL_GL_GetDrawableSize(w, &dw, &dh); }
    printf("[ponteiro] janela=%dx%d drawable=%dx%d toque=%d\n", ww, wh, dw, dh,
           SDL_GetNumTouchDevices());
  }
#ifdef NV_PONT_WEBOS
  {
    void *self = dlopen(NULL, RTLD_NOW);
    if (self) *(void **)(&system_cursor) = dlsym(self, "SDL_webOSCursorVisibility");
  }
  printf("[ponteiro] ShowCursor=%d\n", SDL_ShowCursor(SDL_QUERY));
  printf("[ponteiro] SDL_webOSCursorVisibility: %s\n",
         system_cursor ? "presente" : "ausente");
  {
    SDL_bool (*panel)(int *, int *) = NULL;
    void *self = dlopen(NULL, RTLD_NOW);
    int pw = 0, ph = 0;
    if (self) *(void **)(&panel) = dlsym(self, "SDL_webOSGetPanelResolution");
    if (panel) panel(&pw, &ph);
    printf("[ponteiro] painel=%dx%d\n", pw, ph);
  }
  fflush(stdout);
#endif
}

static void hide_cursor(const char *reason) {
  if (!is_visible) return;
  is_visible = 0;
  hover.ok = 0;
  (void)reason;
#ifdef NV_PONT_WEBOS
  if (system_cursor) { system_cursor(SDL_FALSE); system_hidden = 1; }
#endif
}

static void reappear(void) {
  hidden_by_arrow = 0;
  if (!is_visible) { is_visible = 1; hover.ok = 0; }
#ifdef NV_PONT_WEBOS
  if (system_hidden && system_cursor) system_cursor(SDL_TRUE);
#endif
  system_hidden = 0;
}

static int arrow_tremor(Uint32 now) {
  if (!hidden_by_arrow) return 0;
  if (now - arrow_at < PTR_ARROW_WINDOW_MS) return 1;
  if (!arrow_anchored || now - arrow_motion_at > PTR_ARROW_PAUSE_MS) {
    arrow_anchored = 1; arrow_anchor_x = px; arrow_anchor_y = py;
  }
  arrow_motion_at = now;
  {
    float dx = px - arrow_anchor_x, dy = py - arrow_anchor_y;
    return dx * dx + dy * dy < PTR_ARROW_THRESHOLD * PTR_ARROW_THRESHOLD;
  }
}

static void log_first_event(int type_idx, const char *name, int x, int y) {
  if (type_idx < 0 || type_idx >= 8 || logged_type[type_idx]) return;
  logged_type[type_idx] = 1;
  printf("[ponteiro] primeiro evento tipo=%s x=%d y=%d -> logico %.0f,%.0f\n",
         name, x, y, px, py);
  fflush(stdout);
}

// Window -> logical coordinates (1920x1080).
static void convert_coords(Uint32 window_id, int x, int y) {
  int w = window_w, h = window_h;
  if (w <= 0 || h <= 0) {
    SDL_Window *win = SDL_GetWindowFromID(window_id);
    if (!win) win = SDL_GetMouseFocus();
    if (win) SDL_GetWindowSize(win, &w, &h);
    if (w <= 0 || h <= 0) { w = (int)NV_TELA_W; h = (int)NV_TELA_H; }
  }
  px = (float)x * NV_TELA_W / (float)w;
  py = (float)y * NV_TELA_H / (float)h;
  if (px < 0) px = 0;
  if (px > NV_TELA_W - 1) px = NV_TELA_W - 1;
  if (py < 0) py = 0;
  if (py > NV_TELA_H - 1) py = NV_TELA_H - 1;
}

int pointer_find(const PointerTarget *v, int n, float x, float y) {
  for (int i = n - 1; i >= 0; i--)
    if (x >= v[i].x && x < v[i].x + v[i].w && y >= v[i].y && y < v[i].y + v[i].h)
      return i;
  return -1;
}

static int same_target(const Ident *id, const PointerTarget *al) {
  return id->ok && id->focus == al->focus && id->activate == al->activate &&
         id->a == al->a && id->b == al->b;
}

static void store_target(Ident *id, const PointerTarget *al) {
  id->ok = 1; id->focus = al->focus; id->activate = al->activate;
  id->a = al->a; id->b = al->b;
  id->cx = al->x + al->w * 0.5f; id->cy = al->y + al->h * 0.5f;
}

static void send_key(void (*deliver)(const SDL_Event *), Uint32 type, SDL_Keycode k) {
  SDL_Event t; SDL_zero(t);
  t.type = type; t.key.keysym.sym = k;
  t.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  deliver(&t);
}

static int trace_count;
static void trace_log(const char *what, const PointerTarget *al, int n) {
  if (trace_count >= 40) return;
  trace_count++;
  if (al) printf("[ponteiro] %s a=%d b=%d em %.0f,%.0f (alvo %.0f,%.0f %.0fx%.0f; %d alvos)\n",
                 what, al->a, al->b, px, py, al->x, al->y, al->w, al->h, n);
  else    printf("[ponteiro] %s sem alvo em %.0f,%.0f (%d alvos)\n", what, px, py, n);
  fflush(stdout);
}

static void on_move(void) {
  const PointerTarget *v = targets[ready_buf];
  int i = pointer_find(v, target_count[ready_buf], px, py);
  if (i < 0 || same_target(&hover, &v[i])) return;
  if (content_moved_at && now_ms() - content_moved_at < PTR_SETTLE_MS) return;
  store_target(&hover, &v[i]);
  if (v[i].focus) { trace_log("foco", &v[i], target_count[ready_buf]); v[i].focus(v[i].a, v[i].b); }
}

static int finger_index(const SDL_TouchFingerEvent *e) {
  for (int i = 0; i < finger_count; i++)
    if (fingers[i].touch_id == e->touchId && fingers[i].finger_id == e->fingerId) return i;
  return -1;
}

static int convert_touch(const SDL_TouchFingerEvent *e) {
  if (!isfinite(e->x) || !isfinite(e->y)) return 0;
  px = fminf(fmaxf(e->x, 0.0f) * NV_TELA_W, NV_TELA_W - 1.0f);
  py = fminf(fmaxf(e->y, 0.0f) * NV_TELA_H, NV_TELA_H - 1.0f);
  return 1;
}

// DRAGGING (#216).
#define PTR_STEP_V            150.0f   // logical px per arrow, vertical
#define PTR_STEP_H            210.0f   // logical px per arrow, horizontal
#define PTR_INERTIA_MIN       0.45f    // px/ms threshold for inertia on release
#define PTR_INERTIA_STOP      0.06f    // px/ms minimum below which inertia halts
#define PTR_INERTIA_TAU       260.0f   // ms exponential decay constant
#define PTR_INERTIA_MAX_STEPS 2        // maximum arrow steps per frame
enum { DRAG_NONE = 0, DRAG_SCROLL, DRAG_TARGET };

static int drag_axis_y;
static float drag_accum, drag_last_x, drag_last_y, drag_vel;
static Uint32 drag_last_ms;
static PointerTarget drag_target_obj;
static int inertia_active;
static float inertia_vel, inertia_accum;
static int inertia_axis_y;
static Uint32 inertia_last_ms;
static void (*touch_deliver)(const SDL_Event *);
static int is_touch_active;

int pointer_is_touch(void) { return is_touch_active; }
int pointer_has_touch(void) { return touch_available; }

static void target_from_touch(const PointerTarget *al, int do_focus, int do_activate) {
  is_touch_active = 1;
  if (do_focus && al->focus) al->focus(al->a, al->b);
  if (do_activate && al->activate) al->activate(al->a, al->b);
  is_touch_active = 0;
}

static float scroll_steps(float accum, int axis_y, int cap) {
  float step = axis_y ? PTR_STEP_V : PTR_STEP_H;
  int n = 0;
  while ((accum >= step || accum <= -step) && (!cap || n < cap)) {
    SDL_Keycode k = axis_y ? (accum > 0 ? SDLK_UP : SDLK_DOWN)
                           : (accum > 0 ? SDLK_LEFT : SDLK_RIGHT);
    accum += accum > 0 ? -step : step;
    if (touch_deliver) {
      send_key(touch_deliver, SDL_KEYDOWN, k);
      send_key(touch_deliver, SDL_KEYUP, k);
    }
    n++;
  }
  return accum;
}

static void stop_inertia(void) { inertia_active = 0; inertia_vel = inertia_accum = 0.0f; }

static int touch_event(const SDL_Event *e, void (*deliver)(const SDL_Event *)) {
  const SDL_TouchFingerEvent *t = &e->tfinger;
  int finger_idx = finger_index(t);
  Uint32 now = now_ms();
  touch_available = 1;
  touch_deliver = deliver;
  if (e->type == SDL_FINGERDOWN) {
    if (finger_idx >= 0) return 1;
    stop_inertia();
    if (finger_count == PTR_FINGERS_MAX) { excess_fingers++; touch_cancelled = 1; return 1; }
    fingers[finger_count++] = (Finger){t->touchId, t->fingerId};
    if (finger_count > 1 || excess_fingers) { touch_cancelled = 1; drag_mode = DRAG_NONE; return 1; }
    touch_cancelled = !convert_touch(t);
    touch_x = px; touch_y = py;
    drag_last_x = px; drag_last_y = py; drag_last_ms = now; drag_vel = 0.0f; drag_accum = 0.0f;
    drag_mode = DRAG_NONE;
    touch_target.ok = 0;
    touch_no_targets = target_count[ready_buf] == 0;
    if (!touch_cancelled) {
      int i = pointer_find(targets[ready_buf], target_count[ready_buf], px, py);
      if (i >= 0) { store_target(&touch_target, &targets[ready_buf][i]); drag_target_obj = targets[ready_buf][i]; }
    }
    return 1;
  }
  if (finger_idx < 0) {
    if (e->type == SDL_FINGERUP && excess_fingers) excess_fingers--;
    return 1;
  }
  if (!convert_touch(t)) { touch_cancelled = 1; drag_mode = DRAG_NONE; }
  else if (!touch_cancelled && finger_count == 1 && !excess_fingers) {
    float dx = px - touch_x, dy = py - touch_y;
    if (drag_mode == DRAG_NONE &&
        dx * dx + dy * dy > PTR_TOUCH_THRESHOLD * PTR_TOUCH_THRESHOLD) {
      if (touch_target.ok && drag_target_obj.draggable) {
        drag_mode = DRAG_TARGET;
        target_from_touch(&drag_target_obj, 1, 0);
      } else {
        drag_mode = DRAG_SCROLL;
        drag_axis_y = fabsf(dy) >= fabsf(dx);
        drag_accum = 0.0f;
        drag_last_x = touch_x; drag_last_y = touch_y;
      }
    }
    if (drag_mode == DRAG_SCROLL) {
      float d = drag_axis_y ? py - drag_last_y : px - drag_last_x;
      Uint32 dt = now - drag_last_ms;
      drag_accum = scroll_steps(drag_accum + d, drag_axis_y, 0);
      if (dt > 0) drag_vel = 0.6f * (d / (float)dt) + 0.4f * drag_vel;
      drag_last_x = px; drag_last_y = py; drag_last_ms = now;
    } else if (drag_mode == DRAG_TARGET) {
      target_from_touch(&drag_target_obj, 0, 1);
    }
  }
  if (e->type != SDL_FINGERUP) return 1;
  if (finger_count == 1 && !excess_fingers && !touch_cancelled && drag_mode == DRAG_SCROLL) {
    if (fabsf(drag_vel) >= PTR_INERTIA_MIN && now - drag_last_ms < 90) {
      inertia_active = 1; inertia_vel = drag_vel; inertia_accum = drag_accum; inertia_axis_y = drag_axis_y; inertia_last_ms = now;
    }
  } else if (finger_count == 1 && !excess_fingers && !touch_cancelled && drag_mode == DRAG_NONE) {
    int i = pointer_find(targets[ready_buf], target_count[ready_buf], px, py);
    PointerTarget al;
    if (i >= 0 && same_target(&touch_target, &targets[ready_buf][i])) {
      al = targets[ready_buf][i];
      trace_log("toque", &al, target_count[ready_buf]);
      target_from_touch(&al, 1, 1);
      if (!al.activate && al.focus) {
        send_key(deliver, SDL_KEYDOWN, SDLK_RETURN);
        send_key(deliver, SDL_KEYUP, SDLK_RETURN);
      }
    } else if (touch_no_targets && target_count[ready_buf] == 0) {
      send_key(deliver, SDL_KEYDOWN, SDLK_RETURN);
      send_key(deliver, SDL_KEYUP, SDLK_RETURN);
    }
  }
  memmove(fingers + finger_idx, fingers + finger_idx + 1, (size_t)(--finger_count - finger_idx) * sizeof *fingers);
  if (!finger_count && !excess_fingers) { touch_target.ok = 0; drag_mode = DRAG_NONE; }
  return 1;
}

static void inertia_frame(Uint32 now) {
  Uint32 dt;
  if (!inertia_active) return;
  dt = now - inertia_last_ms;
  if (dt > 100) dt = 100;
  inertia_last_ms = now;
  inertia_accum += inertia_vel * (float)dt;
  inertia_vel *= expf(-(float)dt / PTR_INERTIA_TAU);
  inertia_accum = scroll_steps(inertia_accum, inertia_axis_y, PTR_INERTIA_MAX_STEPS);
  if (fabsf(inertia_vel) < PTR_INERTIA_STOP) stop_inertia();
}

int pointer_event(const SDL_Event *e, void (*deliver)(const SDL_Event *)) {
  Uint32 now = now_ms();
  switch (e->type) {
    case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
      return touch_event(e, deliver);
    case SDL_APP_WILLENTERBACKGROUND:
      cancel_touch();
      return 0;
    case SDL_WINDOWEVENT:
      if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) cancel_touch();
      return 0;
    case SDL_MOUSEMOTION:
      if (e->motion.which == SDL_TOUCH_MOUSEID) return 1;
      convert_coords(e->motion.windowID, e->motion.x, e->motion.y);
      log_first_event(0, "movimento", e->motion.x, e->motion.y);
      if (arrow_tremor(now)) return 1;
      last_motion_time = now;
      reappear();
      on_move();
      return 1;

    case SDL_MOUSEBUTTONDOWN: {
      if (e->button.which == SDL_TOUCH_MOUSEID) return 1;
      convert_coords(e->button.windowID, e->button.x, e->button.y);
      log_first_event(1, "clique", e->button.x, e->button.y);
      last_motion_time = now;
      reappear();
      if (e->button.button == SDL_BUTTON_RIGHT) {
        send_key(deliver, SDL_KEYDOWN, SDLK_AC_BACK); back_pending = 1;
        return 1;
      }
      if (e->button.button != SDL_BUTTON_LEFT) return 1;
      if (ok_key_at && now - ok_key_at < PTR_DEDUPE_MS) { swallow_click_release = 1; return 1; }
      clicked_at = now;
      {
        const PointerTarget *v = targets[ready_buf];
        int n = target_count[ready_buf];
        int i = pointer_find(v, n, px, py);
        trace_log("clique", i >= 0 ? &v[i] : NULL, n);
        activate_pending.ok = 0;
        if (i >= 0 && v[i].activate) { store_target(&activate_pending, &v[i]); return 1; }
        if (i >= 0 && !v[i].focus) return 1;
        if (i >= 0) {
          if (!same_target(&hover, &v[i])) store_target(&hover, &v[i]);
          if (v[i].focus) v[i].focus(v[i].a, v[i].b);
        }
        if (i >= 0 || n == 0) {
          send_key(deliver, SDL_KEYDOWN, SDLK_RETURN);
          ok_pending = 1;
        }
      }
      return 1;
    }

    case SDL_MOUSEBUTTONUP:
      if (e->button.which == SDL_TOUCH_MOUSEID) return 1;
      convert_coords(e->button.windowID, e->button.x, e->button.y);
      if (back_pending && e->button.button == SDL_BUTTON_RIGHT) {
        back_pending = 0; send_key(deliver, SDL_KEYUP, SDLK_AC_BACK); return 1;
      }
      if (e->button.button != SDL_BUTTON_LEFT) return 1;
      if (swallow_click_release) { swallow_click_release = 0; return 1; }
      if (ok_pending) { ok_pending = 0; send_key(deliver, SDL_KEYUP, SDLK_RETURN); }
      if (activate_pending.ok) {
        const PointerTarget *v = targets[ready_buf];
        int i = pointer_find(v, target_count[ready_buf], px, py);
        Ident id = activate_pending;
        activate_pending.ok = 0;
        if (i >= 0 && same_target(&id, &v[i])) v[i].activate(v[i].a, v[i].b);
      }
      return 1;

    case SDL_MOUSEWHEEL: {
      if (e->wheel.which == SDL_TOUCH_MOUSEID) return 1;
      int dy = e->wheel.y, dx = e->wheel.x;
      log_first_event(2, "rodinha", dx, dy);
      if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { dy = -dy; dx = -dx; }
      if (now - wheel_at < PTR_WHEEL_MS) return 1;
      if (!dy && !dx) return 1;
      wheel_at = now;
      {
        SDL_Keycode k = dy > 0 ? SDLK_UP : dy < 0 ? SDLK_DOWN
                        : dx > 0 ? SDLK_RIGHT : SDLK_LEFT;
        send_key(deliver, SDL_KEYDOWN, k);
        send_key(deliver, SDL_KEYUP, k);
      }
      return 1;
    }

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      int sc = (int)e->key.keysym.scancode;
      SDL_Keycode k = e->key.keysym.sym;
      if (sc == PTR_SC_CURSOR_SHOW || sc == PTR_SC_CURSOR_HIDE) {
        if (e->type == SDL_KEYDOWN) {
          log_first_event(sc == PTR_SC_CURSOR_SHOW ? 3 : 4,
                          sc == PTR_SC_CURSOR_SHOW ? "cursor-mostrou" : "cursor-escondeu",
                          sc, 0);
          if (sc == PTR_SC_CURSOR_HIDE) {
            is_visible = 1; hide_cursor("sistema");
            system_hidden = 0;
          } else if (hidden_by_arrow && now - arrow_at < PTR_ARROW_WINDOW_MS) {
#ifdef NV_PONT_WEBOS
            if (system_cursor) { system_cursor(SDL_FALSE); system_hidden = 1; }
#endif
          } else if (hidden_by_arrow) {
#ifdef NV_PONT_WEBOS
            if (system_hidden && system_cursor) system_cursor(SDL_TRUE);
#endif
            system_hidden = 0;
          } else { reappear(); last_motion_time = now; hover.ok = 0; }
        }
        return 1;
      }
      if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (e->type == SDL_KEYDOWN) {
          if (e->key.repeat) return swallow_ok_release;
          if (ok_pending || (clicked_at && now - clicked_at < PTR_DEDUPE_MS)) {
            swallow_ok_release = 1; return 1;
          }
          ok_key_at = now;
          return 0;
        }
        if (swallow_ok_release) { swallow_ok_release = 0; return 1; }
        return 0;
      }
      if (e->type == SDL_KEYDOWN &&
          (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT)) {
        hide_cursor("seta");
        hidden_by_arrow = 1; arrow_at = now; arrow_anchored = 0;
      }
      return 0;
    }
    default:
      return 0;
  }
}

void pointer_frame(Uint32 now) {
  target_count[write_buf] = 0;
  inertia_frame(now);
#ifdef NV_PONT_WEBOS
  if (!system_cursor) return;
#endif
  if (is_visible && now - last_motion_time > PTR_SLEEP_MS && !ok_pending) hide_cursor("parado");
}

static void close_frame(void) {
  Uint32 now = now_ms();
  ready_buf = write_buf;
  write_buf ^= 1;
  target_count[write_buf] = 0;
  if (is_visible && hover.ok) {
    const PointerTarget *v = targets[ready_buf];
    int found = 0;
    for (int i = target_count[ready_buf] - 1; i >= 0; i--)
      if (same_target(&hover, &v[i])) {
        float cx = v[i].x + v[i].w * 0.5f, cy = v[i].y + v[i].h * 0.5f;
        if (fabsf(cx - hover.cx) > 3.0f || fabsf(cy - hover.cy) > 3.0f)
          content_moved_at = now;
        hover.cx = cx; hover.cy = cy;
        found = 1;
        break;
      }
    if (!found) { content_moved_at = now; hover.ok = 0; }
  }
}

void pointer_target(float x, float y, float w, float h,
                    PointerFn focus, PointerFn activate, int a, int b) {
  PointerTarget *al;
  if (!pointer_active()) return;
  if (w <= 0 || h <= 0 || target_count[write_buf] >= PTR_MAX_TARGETS) return;
  al = &targets[write_buf][target_count[write_buf]++];
  { float e = gfx_escala(); x *= e; y *= e; w *= e; h *= e; }
  al->x = x; al->y = y; al->w = w; al->h = h;
  al->focus = focus; al->activate = activate; al->a = a; al->b = b;
  al->draggable = 0;
}

void pointer_target_draggable(void) {
  if (target_count[write_buf] > 0) targets[write_buf][target_count[write_buf] - 1].draggable = 1;
}

void pointer_layer(void) {
  if (!pointer_active()) return;
  target_count[write_buf] = 0;
}

void pointer_draw(void) {
  float g, d, over;
  if (!is_visible) { close_frame(); return; }
#ifdef NV_PONT_WEBOS
  close_frame();
  return;
#endif
  g = gfx_opacidade_grupo;
  gfx_opacidade_grupo = 1.0f;
  gfx_sem_recorte();
  over = pointer_find(targets[write_buf], target_count[write_buf], px, py) >= 0 ? 1.0f : 0.0f;
  d = 26.0f + 6.0f * over;
  gfx_cor((GfxRect){ px - d * 0.5f - 3.0f, py - d * 0.5f - 1.0f, d + 6.0f, d + 6.0f },
          0.5f, 0, 0, 0, 0.45f);
  gfx_cor((GfxRect){ px - d * 0.5f, py - d * 0.5f, d, d }, 0.5f, 1, 1, 1, 0.96f);
  d -= 10.0f;
  gfx_cor((GfxRect){ px - d * 0.5f, py - d * 0.5f, d, d }, 0.5f,
          0.10f, 0.11f, 0.13f, 0.30f + 0.5f * over);
  gfx_opacidade_grupo = g;
  close_frame();
}

// Backward-compatibility wrappers
void  ponteiro_iniciar(void) { pointer_init(); }
void  ponteiro_diag(const SDL_Event *e) { pointer_diag(e); }
int   ponteiro_evento(const SDL_Event *e, void (*entregar)(const SDL_Event *)) {
  return pointer_event(e, entregar);
}
void  ponteiro_quadro(Uint32 agora) { pointer_frame(agora); }
void  ponteiro_desenhar(void) { pointer_draw(); }
int   ponteiro_ativo(void) { return pointer_active(); }
void  ponteiro_alvo(float x, float y, float w, float h,
                    PonteiroFn focar, PonteiroFn ativar, int a, int b) {
  pointer_target(x, y, w, h, focar, ativar, a, b);
}
void  ponteiro_camada(void) { pointer_layer(); }
void  ponteiro_alvo_arrastavel(void) { pointer_target_draggable(); }
int   ponteiro_toque(void) { return pointer_is_touch(); }
int   ponteiro_tem_toque(void) { return pointer_has_touch(); }
float ponteiro_x(void) { return pointer_x(); }
float ponteiro_y(void) { return pointer_y(); }
int   ponteiro_achar(const PonteiroAlvo *v, int n, float x, float y) {
  return pointer_find(v, n, x, y);
}
void  ponteiro_teste_relogio(Uint32 (*fn)(void)) { pointer_test_clock(fn); }
void  ponteiro_teste_janela(int w, int h) { pointer_test_window(w, h); }
void  ponteiro_teste_toque(int ligado) { pointer_test_touch(ligado); }
#ifdef NV_PONT_WEBOS
void  ponteiro_teste_cursor_sistema(SDL_bool (*fn)(SDL_bool)) { pointer_test_system_cursor(fn); }
#endif
