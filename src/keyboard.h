// ON-SCREEN KEYBOARD, shared typing surface.
//
// WHY IT EXISTS: search screen already had a 6x6 grid of `a-z0-9`
// (busca.c:106), built specifically for this alphabet and D-pad. When pairing
// code needed typing, obvious solution was copying that grid into friends screen —
// but a second grid begins divergence.
//
// WHAT IS GENUINELY SHARED: the ALPHABET (keyboard_alphabet, which busca.c
// also uses) and this MODAL. The embedded grid in search stays there because
// it is non-modal — it shares focus with result rows on right.
//
// THE FORM: a two-column island. LEFT: context, title, hint, text field
// and modes (TV keyboard | voice | type on phone) — and with phone open, QR
// underneath, inside modal. RIGHT: grid and row backspace / clear / done.
//
// FOCUS FOLLOWS DRAWING: LEFT on first column of any grid row enters left column;
// RIGHT returns to key left from. Inside, field sits on top and segments below (UP/DOWN),
// and LEFT/RIGHT navigate between segments. UP from first grid row does nothing.
//
// ALPHABET BELONGS TO CALLER, always. Modal was created for pairing code (6 chars
// of `a-z0-9`) and that keyboard remains the same. Space exists where alphabet has it.
#ifndef NV_KEYBOARD_H
#define NV_KEYBOARD_H
#include <SDL2/SDL.h>

// The 36 grid characters, in reading order (6 rows of 6).
const char *keyboard_alphabet(void);

#define KEYBOARD_MAX  64
#define KEYBOARD_LONG 400

void keyboard_open(const char *title, const char *hint, int max_chars);
void keyboard_open_with(const char *title, const char *hint, int max_chars,
                        const char *alphabet, const char *initial);

enum { KEYBOARD_TYPE_TEXT = 0, KEYBOARD_TYPE_EMAIL, KEYBOARD_TYPE_PASSWORD };
void keyboard_type(int type);
void keyboard_mask(int enable);
void keyboard_context(const char *kicker);
int  keyboard_is_masked(void);
int  keyboard_is_open(void);
int  keyboard_focus_field(void);
int  keyboard_layer(void);
int  keyboard_case(void);
void keyboard_test_modes(int ime, int voice, int phone);

enum { KEYBOARD_FOCUS_LEFT = 0, KEYBOARD_FOCUS_CHAR, KEYBOARD_FOCUS_MIDDLE, KEYBOARD_FOCUS_ACTIONS };
int  keyboard_focus_type(void);
void keyboard_test_wrap(int f0, int f1, int f2);
void keyboard_event(const SDL_Event *e);
void keyboard_update(float dt, Uint32 now);
void keyboard_draw(Uint32 now);

enum { KEYBOARD_NONE = 0, KEYBOARD_READY, KEYBOARD_CANCELLED };
int  keyboard_result(void);
const char *keyboard_text(void);
void keyboard_forget(void);

// Backward-compatibility shims
#define TECLADO_MAX   KEYBOARD_MAX
#define TECLADO_LONGO KEYBOARD_LONG

#define TECLADO_TIPO_TEXTO KEYBOARD_TYPE_TEXT
#define TECLADO_TIPO_EMAIL KEYBOARD_TYPE_EMAIL
#define TECLADO_TIPO_SENHA KEYBOARD_TYPE_PASSWORD

#define TECLADO_FOCO_ESQUERDA   KEYBOARD_FOCUS_LEFT
#define TECLADO_FOCO_CARACTERE  KEYBOARD_FOCUS_CHAR
#define TECLADO_FOCO_MEIO       KEYBOARD_FOCUS_MIDDLE
#define TECLADO_FOCO_ACOES      KEYBOARD_FOCUS_ACTIONS

#define TECLADO_NADA     KEYBOARD_NONE
#define TECLADO_PRONTO   KEYBOARD_READY
#define TECLADO_CANCELOU KEYBOARD_CANCELLED

const char *teclado_alfabeto(void);
void teclado_abrir(const char *titulo, const char *dica, int max);
void teclado_abrir_com(const char *titulo, const char *dica, int max,
                       const char *alfabeto, const char *inicial);
void teclado_tipo(int tipo);
void teclado_mascarar(int liga);
void teclado_contexto(const char *kicker);
int  teclado_mascarado(void);
int  teclado_aberto(void);
int  teclado_foco_campo(void);
int  teclado_camada(void);
int  teclado_caixa(void);
void teclado_teste_modos(int ime, int voz, int cel);
int  teclado_foco_tipo(void);
void teclado_teste_quebra(int f0, int f1, int f2);
void teclado_evento(const SDL_Event *e);
void teclado_atualizar(float dt, Uint32 agora);
void teclado_desenhar(Uint32 agora);
int  teclado_resultado(void);
const char *teclado_texto(void);
void teclado_esquecer(void);

#endif
