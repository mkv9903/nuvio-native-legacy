// MAGIC REMOTE POINTER (#99) AND DIRECT TOUCH (#216).
//
// HOW IT ARRIVES. On webOS the pointer is a wl_pointer from compositor, and
// LG's SDL delivers it as ordinary mouse: SDL_MOUSEMOTION in window coordinates,
// SDL_MOUSEBUTTONDOWN/UP on click (remote OK with cursor on screen) and
// SDL_MOUSEWHEEL on scroll wheel. In addition SDK's SDL_webOS.h defines two
// SYNTHETIC KEYS that are not keys: 484 (SDL_WEBOS_SCANCODE_CURSOR_SHOW) and
// 485 (..._CURSOR_HIDE), sent when system shows or hides cursor.
// And SDL_webOSCursorVisibility() asks compositor to show/hide its cursor.
// Looked up via dlsym: device SDL is another binary than SDK's, and on firmware
// without it the app must still start.
//
// THE ARROW IS THE SYSTEM'S ON webOS. LG's SDL only delivers motion and click
// with SDL_ShowCursor on (MEASURED on C9, see main.c), so visible arrow is
// compositor's and this module draws none. On Mac system cursor stays off and
// module draws its own circle.
//
// WHAT THIS MODULE DOES. Tracks where pointer is (LOGICAL coordinates,
// 1920x1080, same as layout) and translates pointer into what UI already understands:
//   - HOVER focuses. Each screen registers focusable rectangles (pointer_target);
//     on next motion topmost under cursor has `focus` called — placing focus via
//     SAME variable that arrows move.
//   - CLICK and OK: focuses target and delivers KEYDOWN RETURN on button press and
//     KEYUP on button release. Hold gestures happen automatically. Target with
//     custom `activate` calls it instead of OK.
//   - WHEEL becomes arrows: up/down, and left/right on side wheel.
//   - TOUCH (#216): tap = focus + OK; drag scrolls; dragging over draggable target
//     calls activate.
//   - REMOTE ARROW hides cursor; returns with actual motion.
//
// ZERO COST WITHOUT POINTER: with cursor hidden pointer_target() returns on
// first line and nothing is drawn.
//
// LAYERS. Sheet or modal calls pointer_layer() before drawing: background targets
// are invalidated. Layer WITHOUT targets receives click as pure OK.
#ifndef NV_POINTER_H
#define NV_POINTER_H
#include <SDL2/SDL.h>

typedef void (*PointerFn)(int a, int b);

typedef struct {
  float x, y, w, h;
  union { PointerFn focus; PointerFn focar; };
  union { PointerFn activate; PointerFn ativar; };
  int a, b;
  union { int draggable; int arrasta; };
} PointerTarget;

void pointer_init(void);
// Diagnostic: logs non-key events.
void pointer_diag(const SDL_Event *e);

// Filters SDL event. Returns 1 when event belonged to pointer and was handled.
// `deliver` receives keys synthesized by pointer (OK, wheel arrows).
int  pointer_event(const SDL_Event *e, void (*deliver)(const SDL_Event *));

// Once per frame, BEFORE drawing: resets target list and puts idle cursor to sleep.
void pointer_frame(Uint32 now);

// Draws cursor, last in frame, and CLOSES frame.
void pointer_draw(void);

// Is it worth registering targets?
int  pointer_active(void);
int  pointer_is_active(void);

void pointer_target(float x, float y, float w, float h,
                    PointerFn focus, PointerFn activate, int a, int b);
void pointer_layer(void);
// Marks LAST registered target as draggable by finger (#216).
void pointer_target_draggable(void);
// 1 while focus/activate are running due to a FINGER (not Magic Remote).
int  pointer_is_touch(void);
// Has touch screen (Android, or a finger has touched).
int  pointer_has_touch(void);

// Current logical position.
float pointer_x(void);
float pointer_y(void);

// --- pure, for tests/ponteiro.c ------------------------------------------
int  pointer_find(const PointerTarget *v, int n, float x, float y);
void pointer_test_clock(Uint32 (*fn)(void));
void pointer_test_window(int w, int h);
void pointer_test_touch(int enabled);
void pointer_test_system_cursor(SDL_bool (*fn)(SDL_bool));

// Backward-compatibility shims
typedef PointerFn     PonteiroFn;
typedef PointerTarget PonteiroAlvo;

void  ponteiro_iniciar(void);
void  ponteiro_diag(const SDL_Event *e);
int   ponteiro_evento(const SDL_Event *e, void (*entregar)(const SDL_Event *));
void  ponteiro_quadro(Uint32 agora);
void  ponteiro_desenhar(void);
int   ponteiro_ativo(void);
void  ponteiro_alvo(float x, float y, float w, float h,
                    PonteiroFn focar, PonteiroFn ativar, int a, int b);
void  ponteiro_camada(void);
void  ponteiro_alvo_arrastavel(void);
int   ponteiro_toque(void);
int   ponteiro_tem_toque(void);
float ponteiro_x(void);
float ponteiro_y(void);
int   ponteiro_achar(const PonteiroAlvo *v, int n, float x, float y);
void  ponteiro_teste_relogio(Uint32 (*fn)(void));
void  ponteiro_teste_janela(int w, int h);
void  ponteiro_teste_toque(int ligado);
void  ponteiro_teste_cursor_sistema(SDL_bool (*fn)(SDL_bool));

#endif
