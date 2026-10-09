// See keyboard.h for why this modal exists and what it does NOT try to be.
#include "keyboard.h"
#include "escala.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "sistexto.h"
#include "pointer.h"
#include "celbotao.h"
#include "celular.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// THE SAME DIMENSIONS AS THE SEARCH GRID (busca.c): 74 key with 12 gap, and
// the same 10% growth on focus. Not a coincidence or copy — it is the size
// already measured for a key that can be hit with the D-pad from three meters,
// and two grids in the same app with different key sizes look like two apps.
#define KB_KEY        74.0f
#define KB_GAP        12.0f
#define KB_COLS        6
// LONG ALPHABET GETS COLUMNS, NOT ROWS (#88). Xtream user/password has 73
// symbols: in 6 columns it needed 13 rows, the ceiling is 7 character rows
// (8 with backspace/clear/done = 1080 px modal, full screen), and everything
// past index 42 — 'Q' onward, ALL digits — remained outside the grid,
// neither drawn nor reachable. User sent photo of keyboard cut off at "P".
//
// 13 is half the Latin alphabet: grid reads a-m / n-z / A-M / N-Z /
// 0-9._- / @!#$%&*+=, each row half of something the person already knows,
// and fits in 6 rows — the SAME height as standard keyboard (994 px). Width:
// 13 x 74 + 12 x 12 = 1106 grid, 1194 modal, 363 slack on each side
// in 1920. Scrolling was discarded: with everything visible there is no "more below"
// for person to guess, and each extra key distance on D-pad costs the same as
// in a scrolled grid.
//
// Only enters above 42 (7 x 6): default (36), portal (39) and MAC (17) stay
// in 6 columns, pixel by pixel the previous layout.
#define KB_COLS_LONG  13
// Column ceiling: alphabet that doesn't fit even in 13 x 7 (91+) widens up to here,
// 20 x 74 + 19 x 12 = 1708 + 88 = 1796 < 1920. Above 140 symbols the rest
// stays out, as before — no caller comes close.
#define KB_COLS_MAX   20
// CEILING for character rows + backspace/clear/done row. The DEFAULT
// alphabet occupies 6 (36 characters); IPTV portal needs dot, colon
// and hyphen in addition to a-z0-9, and doesn't fit in 36. Caller chooses
// alphabet, and modal adjusts — actual rows are `num_rows`.
#define KB_ROWS_MAX   8
#define KB_ROWS_DEF   7          // 6 of a-z0-9 + 1 of backspace/clear/done
#define KB_STEP      (KB_KEY + KB_GAP)

#define KB_SCALE      0.10f

// Box for a typed character. 6 of them with gap 12 fit in 504 of 6-column
// grid, and per-character box is what makes code DICTATABLE over phone:
// separated, no one confuses "rn" with "m" or miscounts letters.
#define KB_BOX_W     76.0f
#define KB_BOX_H     92.0f
#define KB_BOX_GAP   12.0f
// Box width floor per character: below this TXT_TITULO2 glyph doesn't fit
// and letters overlap. Measured on owner capture with maxN=24, where
// calculation gave 9.5 px per box.
#define KB_BOX_W_MIN 44.0f
#define KB_FIELD_PAD 18.0f

#define KB_PAD       44.0f
// TITLE (46) + HINT up to two lines (2 x 28) + BOXES (92) + margins.
//
// WAS 176, FROM MEMORY, and first capture showed result: boxes of typed
// text spawned ABOVE hint line and the two overlapped. Header height of
// a modal is not guessed — it is sum of what is inside it.
#define KB_HINT_Y    (KB_PAD + 50.0f)
#define KB_HINT_H     56.0f
#define KB_BOX_Y     (KB_HINT_Y + KB_HINT_H + 10.0f)
// 44 MARGIN TO GRID, not 26: boxes of typed text have same width as keys,
// and glued to them first capture read as a SEVENTH ROW of keyboard instead
// of "what you already typed".
#define KB_HEADER    (KB_BOX_Y + KB_BOX_H + 44.0f)
#define KB_FOOTER     64.0f
#define KB_W         (grid_w() + KB_PAD * 2.0f)

// TYPE ON PHONE (celbotao.h): was fixed 440 px panel to right of modal,
// with QR always visible. Turned into SAME button as Spotlight and Search,
// next to field (after Voice): single gesture across app, modal becomes
// centered and width of grid again, and server only spins up when user
// requests (OK on button), not every opened keyboard.
#define KB_X         ((NV_TELA_W - KB_W) * 0.5f)

static const char *ALPHABET = "abcdefghijklmnopqrstuvwxyz0123456789";

// row -1 = LEFT COLUMN (field and modes). `col` indicates target:
// KB_T_FIELD text field (OK calls TV keyboard), and segments KB_T_VOICE,
// KB_T_PHONE and KB_T_IME ("TV Keyboard"). Field and TV keyboard segment
// are SEPARATE targets because they are drawn separately: while both lit
// together, focus did not indicate where it was.
//
// HOW REACHED (owner, 05/10, Android TV): column is LEFT of grid, so
// LEFT on first column of any row enters it, and RIGHT returns to key
// where focus left from (return_row/return_col). Was UP from first row,
// legacy of when field sat ABOVE grid: user pressed towards where field
// was seen and nothing happened. Inside, focus follows drawing — field
// on top, segments below, across however many rows segmented wrapped (seg_row[]).
enum { KB_T_FIELD = 0, KB_T_VOICE, KB_T_PHONE, KB_T_IME, KB_T_COUNT };
static int   is_open, row, col;
static int   return_row, return_col, prev_bar_target;
static float bar_anim[KB_T_COUNT];
// Character column where focus dropped down to backspace/clear/done. Without it,
// moving UP from "done" (column 2) in a 13-grid landed on 'c', ten keys away
// from where user was; with it, returns to the same key.
static int   prev_char_col;
static float anim, focus_anim[KB_ROWS_MAX][KB_COLS_MAX];
static const char *current_alphabet = NULL;   // NULL = default
static int   num_rows = KB_ROWS_DEF;
static int   num_cols = KB_COLS;

// EMAIL LOGIN (#216, owner decision): who enters by email and password is
// precisely who doesn't have phone at hand — if they had, they'd log in via QR.
// In these fields the "Type on phone" button disappears.
static int no_phone;
// What exists HERE, with test port (keyboard_test_modes): on Mac there is no
// way to turn off phone server or turn on only voice, and navigation changes
// with each combination.
static int force_ime = -1, force_voice = -1, force_phone = -1;
void keyboard_test_modes(int ime, int voice, int phone) {
  force_ime = ime; force_voice = voice; force_phone = phone;
}
static int ime_ok(void) { return force_ime >= 0 ? force_ime : st_ime_disponivel(); }
static int voice_ok(void) { return force_voice >= 0 ? force_voice : st_voz_disponivel(); }
static int phone_ok(void) { return (force_phone >= 0 ? force_phone : celb_disponivel()) && !no_phone; }

static float grid_w(void) {
  return (float)num_cols * KB_KEY + (float)(num_cols - 1) * KB_GAP;   // 504 with 6
}
// Left column exists as focus destination if it has any target: even a single
// mode (field is only target where TV keyboard exists, which is already a mode).
static int has_bar(void) { return ime_ok() || voice_ok() || phone_ok(); }

// Modal height depends on how many rows alphabet requested, so three
// measurements depending on it became functions. Still same formula.
static float grid_h(void) {
  return (float)num_rows * KB_KEY + (float)(num_rows - 1) * KB_GAP;
}
// GLASS UI (Settings mockup, "keyboard" board, 03/10): modal became a
// TWO-COLUMN island to fit at 3m without scrolling — left title, hint,
// field and modes (TV keyboard, voice, phone); right 74px grid and
// 56px backspace / clear / done row.
#define KB_ISLAND_PX 52.0f
#define KB_ISLAND_PY 48.0f
#define KB_COL_GAP   56.0f
#define KB_EXTRA_H   56.0f
static float kb_left_w0(void) {
  float w = 1340.0f - 2 * KB_ISLAND_PX - KB_COL_GAP - grid_w();
  float ceiling = NV_TELA_W - 80.0f - 2 * KB_ISLAND_PX - KB_COL_GAP - grid_w();
  if (w > ceiling) w = ceiling;
  if (w < 420.0f) w = 420.0f;
  return w;
}

// MEASURED LAYOUT OF LEFT COLUMN. Nothing here is fixed width: segmented
// (TV keyboard | voice | phone), base hints and uppercase kicker label have
// width of ALREADY TRANSLATED text, and column and modal adjust to it.
// Owner capture (TCL, English, Seekr key) showed "Type on your phone" leaving
// pill and entering grid, and German hints exceeding column.
//   1. column widens up to screen ceiling;
//   2. if still doesn't fit: smaller internal padding, then ONE step down in
//      font size, then segmented breaks into rows (each its own pill);
//   3. base hints break into rows.
// Measured in kb_measure(), on each frame drawn (text bridge caches lines,
// measuring is cheap); before first frame previous numbers apply.
#define KB_SEG_PAD     20.0f
#define KB_SEG_PAD_MIN 12.0f
#define KB_ROW_GAP      8.0f
static int   measured, seg_num_rows = 1, hints_rows = 1, seg_row[3];
static float ew_measured, left_h_measured, seg_pad = KB_SEG_PAD, qr_h, qr_h_prev;
static TxtEstilo seg_style = TXT_AJ_SEG;
static float kb_left_w(void) { return measured ? ew_measured : kb_left_w0(); }
static float kb_w(void) { return 2 * KB_ISLAND_PX + kb_left_w() + KB_COL_GAP + grid_w(); }
static float kb_x(void) { return (NV_TELA_W - kb_w()) * 0.5f; }
static float kb_grid_x(void) { return kb_x() + KB_ISLAND_PX + kb_left_w() + KB_COL_GAP; }
static float kb_h(void) {
  float grid = (float)(num_rows - 1) * KB_STEP - KB_GAP + 20.0f + KB_EXTRA_H;
  // Left column has its own height (title, hint, field, modes and base hints):
  // with short alphabet (hexadecimal, 3 rows) grid alone would leave hints
  // on top of text.
  float left = measured ? left_h_measured : 22 + 48 + 10 + 57 + 30 + 76 + 22 + 55 + 16 + 50 + 40 + 30;
  return 2 * KB_ISLAND_PY + (grid > left ? grid : left);
}
static float kb_y(void) { return (NV_TELA_H - kb_h()) * 0.5f; }
static const char *alpha(void) { return current_alphabet ? current_alphabet : ALPHABET; }
static char  typed_text[KEYBOARD_LONG + 1];
static int   text_len, max_chars, current_result;
static char  current_title[96], current_hint[160];
static int   phone_received;
// PASSWORD (#216): field shows bullets, never text. Modal stays on screen and
// screen becomes snapshot.
static int   mask_password, ime_type, is_password;
// What was typed appears in BOXES (short code) or in a text field.
// Only text field is focus target: boxes have no drawn focus state,
// and an invisible focus is a lost focus.
static int field_is_boxes(void) {
  return !mask_password && max_chars > 0 &&
         (kb_left_w() - (float)(max_chars - 1) * KB_BOX_GAP) / (float)max_chars >= KB_BOX_W_MIN;
}
static int field_is_focusable(void) { return ime_ok() && !field_is_boxes(); }
// Phone QR opened INSIDE modal (celbotao.h, "embedded"): 0..1.
static float anim_qr;
// EMAIL SHORTCUTS (#216): row of keys that type entire chunk. With D-pad,
// "@gmail.com" is ten fewer keys.
static const char *EMAIL_SHORTCUTS[] = { ".com", "@gmail.com", "@hotmail.com", "@outlook.com" };
#define KB_NUM_SHORTCUTS ((int)(sizeof EMAIL_SHORTCUTS / sizeof EMAIL_SHORTCUTS[0]))
static int   num_shortcuts;

// LAYERS (owner, 05/10, Real-Debrid key on Android TV): caller needing
// uppercase passed single long alphabet, and grid showed everything at once —
// a-m / n-z / A-M / N-Z / 0-9 and symbols, 5 x 13 = 65 keys to find one.
// When alphabet has BOTH CASES grid gains layers, like every keyboard
// user already knows:
//   LETTERS   0-9 / a-j / k-t / u-z + first symbols of alphabet
//   SYMBOLS   0-9 / all symbols (only exists if they didn't fit in letters;
//             without 0-9 when symbols alone fill height)
// in 10 columns (848 px instead of 1106) and 4 rows of 74 instead of 5 to 7.
//
// Digits stay on FIRST row of both layers: address and IP alternate digit
// and symbol, and switching layer for every dot would cost more than dense
// grid before. For same reason cells remaining after "z" receive first symbols
// IN CALLER'S ORDER — address alphabets start with ":/.-", account with "._-@",
// which are most typed.
//
// LAYER KEYS live in their own row between characters and backspace/clear/done
// (where email shortcut row sits): uppercase, symbols and space, each only if
// alphabet has them. Did not go to backspace/clear/done row because there,
// with password, that's already four translated words in 848 px; seven don't
// fit in German. In dedicated row DOWN from any column reaches them, and UP
// returns to same key (prev_char_col).
//
// WHAT CAN BE TYPED DOES NOT CHANGE: each cell comes from caller's alphabet,
// and uppercase of letter is only typed if IT is in alphabet (apply_case).
// Single-case alphabet (default, portal, MAC, email) has no layers and
// remains pixel by pixel as it was.
#define KB_COLS_LAYER 10
enum { KB_K_CASE = 0, KB_K_SYMBOLS, KB_K_SPACE };
static char cell[2][KB_ROWS_MAX][KB_COLS_MAX];
static int  num_cells[2][KB_ROWS_MAX];
static int  in_layers, layer, layer_keys[3], num_layer_keys;
// 0 lowercase, 1 only NEXT letter (one touch), 2 locked (two touches).
static int  text_case;

// Rows: character rows, middle row (shortcuts in email, layer keys where
// layers exist) and backspace/clear/(show)/done row, always last.
static int char_rows(void) { return num_rows - 1 - ((num_shortcuts || in_layers) ? 1 : 0); }
static int is_char_row(int r) { return r >= 0 && r < char_rows(); }
static int is_shortcut_row(int r) { return num_shortcuts && r == char_rows(); }
static int is_layer_row(int r) { return in_layers && r == char_rows(); }
static int done_col(void) { return is_password ? 3 : 2; }

// Letter as output NOW: in requested case if alphabet has it, else in what it
// has. Never returns what is not in alphabet.
static char apply_case(char c) {
  char want;
  if (!in_layers || !isalpha((unsigned char)c)) return c;
  want = (char)(text_case ? toupper((unsigned char)c) : tolower((unsigned char)c));
  if (strchr(alpha(), want)) return want;
  return (char)(text_case ? tolower((unsigned char)c) : toupper((unsigned char)c));
}

// Fills layer, row by row, starting from `r`; returns next row.
static int fill_layer(int l, int r, const char *src, int count, int ceiling) {
  int i;
  for (i = 0; i < count; i++) {
    if (num_cells[l][r] == num_cols) r++;
    if (r >= ceiling) return ceiling;   // rest stays out, as it always did
    cell[l][r][num_cells[l][r]++] = src[i];
  }
  return count > 0 ? r + 1 : r;
}

// Builds grid from alphabet: cells of each layer, columns and rows.
// `email` = 13-column grid with shortcuts row.
static void build_grid(int email) {
  const char *a = alpha(), *p;
  int letters = (int)strlen(a), ceiling, r;
  memset(cell, 0, sizeof cell);
  memset(num_cells, 0, sizeof num_cells);
  in_layers = layer = text_case = num_layer_keys = 0;
  num_shortcuts = email ? KB_NUM_SHORTCUTS : 0;
  if (!email) {
    for (p = a; *p; p++) {
      if (isupper((unsigned char)*p) && strchr(a, tolower((unsigned char)*p))) {
        in_layers = 1;
        break;
      }
    }
  }
  if (!in_layers) {
    // Columns: 6, or 13 when 7 rows of 6 are not enough (see KB_COLS_LONG),
    // or whatever fits in 7 rows, up to KB_COLS_MAX. Email is always 13
    // (a-m / n-z / 0-9@._ / -+) + shortcuts: 6 rows, default height.
    num_cols = KB_COLS;
    if (email) {
      num_cols = KB_COLS_LONG;
    } else if (letters > (KB_ROWS_MAX - 1) * KB_COLS) {
      num_cols = KB_COLS_LONG;
      if (letters > (KB_ROWS_MAX - 1) * num_cols)
        num_cols = (letters + KB_ROWS_MAX - 2) / (KB_ROWS_MAX - 1);
      if (num_cols > KB_COLS_MAX) num_cols = KB_COLS_MAX;
    }
    // Character rows = whatever alphabet asks, rounded up, plus middle row
    // and backspace/clear/done row. Ceiling exists because `focus_anim` is fixed
    // array and grid taller than this doesn't fit screen; LAST character row
    // can be partial (39 symbols in 6 columns leaves three in seventh), and
    // num_cells[] prevents focus from entering empty cell.
    ceiling = KB_ROWS_MAX - 1 - (email ? 1 : 0);
    r = fill_layer(0, 0, a, letters, ceiling);
    if (r < 1) r = 1;
    num_rows = r + 1 + (email ? 1 : 0);
    return;
  }
  {
    char let[27], dig[11], sym[64];
    int nl = 0, nd = 0, ns = 0, space = 0, r0, r1, left_over, quick;
    for (p = a; *p; p++) {
      unsigned char c = (unsigned char)*p;
      if (c == ' ') space = 1;
      else if (isalpha(c)) {
        char m = (char)tolower(c);
        if (nl < 26 && !memchr(let, m, (size_t)nl)) let[nl++] = m;
      } else if (isdigit(c)) {
        if (nd < 10 && !memchr(dig, c, (size_t)nd)) dig[nd++] = (char)c;
      } else if (ns < (int)sizeof sym && !memchr(sym, c, (size_t)ns)) {
        sym[ns++] = (char)c;
      }
    }
    num_cols = KB_COLS_LAYER;
    ceiling = KB_ROWS_MAX - 2;
    r0 = fill_layer(0, 0, dig, nd, ceiling);
    r0 = fill_layer(0, r0, let, nl, ceiling);
    // Quick symbols: whatever fits after last letter.
    left_over = r0 > 0 ? num_cols - num_cells[0][r0 - 1] : 0;
    quick = ns < left_over ? ns : left_over;
    if (quick > 0) {
      memcpy(cell[0][r0 - 1] + num_cells[0][r0 - 1], sym, (size_t)quick);
      num_cells[0][r0 - 1] += quick;
    }
    r1 = 0;
    if (ns > quick) {
      // Digits repeat on top of symbols as long as that doesn't make symbols
      // layer taller than letters layer (account password has 32 symbols:
      // four rows of them alone) — modal doesn't change height when switching.
      int only_sym = (ns + num_cols - 1) / num_cols;
      int with_dig = only_sym + (nd + num_cols - 1) / num_cols;
      if (with_dig <= r0) r1 = fill_layer(1, 0, dig, nd, ceiling);
      r1 = fill_layer(1, r1, sym, ns, ceiling);
    }
    layer_keys[num_layer_keys++] = KB_K_CASE;
    if (r1) layer_keys[num_layer_keys++] = KB_K_SYMBOLS;
    if (space) layer_keys[num_layer_keys++] = KB_K_SPACE;
    num_rows = (r0 > r1 ? r0 : r1) + 2;
  }
}

static void open_ime_now(void);
void keyboard_type(int type) {
  ime_type = type == KEYBOARD_TYPE_EMAIL ? ST_IME_EMAIL : type == KEYBOARD_TYPE_PASSWORD ? ST_IME_SENHA : ST_IME_TEXTO;
  is_password = type == KEYBOARD_TYPE_PASSWORD;
  mask_password = is_password;
  no_phone = type == KEYBOARD_TYPE_EMAIL || type == KEYBOARD_TYPE_PASSWORD;
  // 13 columns (a-m / n-z / 0-9@._ / -+) + shortcuts: 6 rows, default height
  // instead of 8 of 6 columns.
  if (type == KEYBOARD_TYPE_EMAIL) build_grid(1);
  // Where system keyboard opens automatically (Android), already comes open:
  // touching email field wants device keyboard, not grid.
  if (no_phone && is_open && ime_ok() && st_abre_sozinho()) {
    return_row = 0; return_col = 0;
    row = -1; col = field_is_focusable() ? KB_T_FIELD : KB_T_IME;
    open_ime_now();
  }
}
void keyboard_mask(int enable) { mask_password = enable != 0; }
int  keyboard_is_masked(void) { return mask_password; }

const char *keyboard_alphabet(void) { return ALPHABET; }
int keyboard_is_open(void) { return is_open; }
int keyboard_focus_field(void) { return row < 0 ? col + 1 : 0; }
int keyboard_layer(void) { return layer; }
int keyboard_focus_type(void) {
  if (row < 0) return KEYBOARD_FOCUS_LEFT;
  return is_char_row(row) ? KEYBOARD_FOCUS_CHAR : row == num_rows - 1 ? KEYBOARD_FOCUS_ACTIONS : KEYBOARD_FOCUS_MIDDLE;
}
void keyboard_test_wrap(int f0, int f1, int f2) {
  seg_row[0] = f0; seg_row[1] = f1; seg_row[2] = f2;
  seg_num_rows = (f2 > f1 ? f2 : f1 > f0 ? f1 : f0) + 1;
}
int keyboard_case(void) { return text_case; }
const char *keyboard_text(void) { return typed_text; }
void keyboard_forget(void) {
  volatile char *p = typed_text;
  size_t k = sizeof typed_text;
  while (k--) *p++ = 0;
  text_len = 0;
}

int keyboard_result(void) {
  int r = current_result;
  current_result = KEYBOARD_NONE;
  return r;
}

// Modal context ("Accounts & services · Keys"), consumed by next opening:
// caller sets before keyboard_open_with, next opens without it.
// Valid for an instant: setting and not opening doesn't taint next modal.
static char kicker_pending[120], kicker_current[120];
static Uint32 kicker_when;
void keyboard_context(const char *kicker) {
  snprintf(kicker_pending, sizeof kicker_pending, "%s", kicker ? kicker : "");
  kicker_when = SDL_GetTicks();
}

#ifdef AJUSTES_TESTE
void keyboard_test_text(const char *t) { snprintf(typed_text, sizeof typed_text, "%s", t); text_len = (int)strlen(typed_text); }
void keyboard_test_focus(int r, int c) { row = r; col = c; }
// Sets layer (0 letters, 1 symbols) and case (0, 1, 2) directly; 0 if alphabet
// doesn't have what was requested.
int keyboard_test_layer(int l, int cx) {
  int i, symbols = 0;
  for (i = 0; i < num_layer_keys; i++) if (layer_keys[i] == KB_K_SYMBOLS) symbols = 1;
  if ((cx && !in_layers) || (l && !symbols)) return 0;
  layer = l; text_case = cx; row = 0; col = 0;
  return 1;
}
void teclado_teste_texto(const char *t) { keyboard_test_text(t); }
void teclado_teste_foco(int f, int c) { keyboard_test_focus(f, c); }
int teclado_teste_camada(int cam, int cx) { return keyboard_test_layer(cam, cx); }
#endif

void keyboard_open(const char *title, const char *hint, int max) {
  keyboard_open_with(title, hint, max, NULL, NULL);
}

void keyboard_open_with(const char *title, const char *hint, int max,
                        const char *alphabet, const char *initial) {
  current_alphabet = (alphabet && *alphabet) ? alphabet : NULL;
  celb_fechar_dono(CELB_TECLADO);   // reopen on top doesn't inherit QR of previous
  mask_password = 0; ime_type = ST_IME_TEXTO; is_password = 0; no_phone = 0;
  build_grid(0);
  is_open = 1;
  row = 0; col = 0; prev_char_col = 0;
  return_row = return_col = 0; prev_bar_target = -1;
  measured = 0;
  current_result = KEYBOARD_NONE;
  max_chars = max > 0 && max <= KEYBOARD_LONG ? max : KEYBOARD_MAX;
  // INITIAL TEXT: editing an existing portal shouldn't force retyping entire
  // address. Clamped at max_chars, never truncated mid-character because
  // alphabet is single-byte per character.
  snprintf(typed_text, sizeof typed_text, "%s", initial ? initial : "");
  typed_text[max_chars] = 0;
  text_len = (int)strlen(typed_text);
  snprintf(current_title, sizeof current_title, "%s", title ? title : "");
  snprintf(current_hint,  sizeof current_hint,  "%s", hint  ? hint  : "");
  snprintf(kicker_current, sizeof kicker_current, "%s", SDL_GetTicks() - kicker_when < 1000u ? kicker_pending : "");
  kicker_pending[0] = 0;
  memset(focus_anim, 0, sizeof focus_anim);
  memset(bar_anim, 0, sizeof bar_anim);
  phone_received = 0;
  anim_qr = 0.0f;
}

// System text passes through modal ALPHABET: pairing code is a-z0-9, MAC is
// 0-9a-f — uppercase becomes lowercase when only lowercase exists, and what
// doesn't exist in it (accents, emojis) stays out.
static void set_from_system(const char *t) {
  const char *a = alpha();
  int w = 0;
  for (; *t && w < max_chars; t++) {
    unsigned char c = (unsigned char)*t;
    if (c >= 0x80 || !c) continue;
    if (strchr(a, c)) typed_text[w++] = (char)c;
    else if (isupper(c) && strchr(a, tolower(c))) typed_text[w++] = (char)tolower(c);
    else if (islower(c) && strchr(a, toupper(c))) typed_text[w++] = (char)toupper(c);
  }
  typed_text[w] = 0;
  text_len = w;
}

static void close_keyboard(int r) {
  is_open = 0;
  celb_fechar_dono(CELB_TECLADO);
  current_result = r;
  st_fechar(ST_TECLADO);
}

static void open_ime_now(void) {
  st_ime_tipo(ime_type);
  st_ime_abrir(ST_TECLADO, typed_text, max_chars);
}

static void bar_ok(void) {
  if (col == KB_T_PHONE) {
    // QR opens IN HERE, under segmented (celb_desenhar_em). OK with code on
    // screen does nothing (user is "confirming"); with expired address or
    // no network, generates another.
    st_fechar(ST_TECLADO);
    if (!celb_embutido(CELB_TECLADO) || celular_estado() != CEL_ESPERANDO)
      celb_abrir_embutido(CELB_TECLADO, current_title);
    return;
  }
  celb_fechar_dono(CELB_TECLADO);   // one mode at a time
  if (col == KB_T_VOICE) st_voz_iniciar(ST_TECLADO);
  else open_ime_now();
}

// Modes that exist now: label and focus target of each, in order drawn.
static int kb_modes(const char **labels, int *cols) {
  int k = 0;
  if (ime_ok())   { labels[k] = "Teclado da TV";         cols[k++] = KB_T_IME; }
  if (voice_ok()) { labels[k] = "Falar";                 cols[k++] = KB_T_VOICE; }
  if (phone_ok()) { labels[k] = "Digitar pelo celular"; cols[k++] = KB_T_PHONE; }
  return k;
}

// Does target `c` of left column exist now?
static int bar_is_valid(int c) {
  const char *labels[3];
  int cols[3], k = kb_modes(labels, cols), i;
  if (c == KB_T_FIELD) return field_is_focusable();
  for (i = 0; i < k; i++) if (cols[i] == c) return 1;
  return 0;
}

static int bar_default(void) {
  const char *labels[3];
  int cols[3], k = kb_modes(labels, cols);
  return field_is_focusable() ? KB_T_FIELD : (k ? cols[0] : KB_T_FIELD);
}

// Enters left column saving where focus left; returns to target user was on
// last time (field first time).
static void enter_bar(int target) {
  if (!has_bar()) return;
  if (row >= 0) { return_row = row; return_col = col; }
  if (target < 0 || !bar_is_valid(target))
    target = bar_is_valid(prev_bar_target) ? prev_bar_target : bar_default();
  row = -1; col = target;
}

static int cols_of_row(int r);
static void exit_bar(void) {
  int r = return_row, c = return_col;
  prev_bar_target = col;
  if (r < 0 || r >= num_rows || cols_of_row(r) < 1) { r = 0; c = 0; }
  if (c >= cols_of_row(r)) c = cols_of_row(r) - 1;
  row = r; col = c < 0 ? 0 : c;
}

static void focus_bar(int c, int b) { (void)b; if (is_open) enter_bar(c); }
static void focus_key(int r, int c) {
  if (!is_open || r < 0 || r >= num_rows) return;
  if (row < 0) prev_bar_target = col;
  else if (!is_char_row(r) && is_char_row(row)) prev_char_col = col;
  row = r; col = c;
}

// How many keys row has IN CURRENT VISIBLE LAYER. Can be 0: symbols layer
// usually has fewer rows than letters, and navigation skips empty rows
// (next_row) rather than landing on non-existent cell.
static int cols_of_row(int r) {
  if (r >= num_rows - 1) return is_password ? 4 : 3;   // backspace / clear / (show) / done
  if (is_shortcut_row(r)) return num_shortcuts;
  if (is_layer_row(r)) return num_layer_keys;
  return r >= 0 ? num_cells[layer][r] : 0;
}

static int next_row(int r, int d) {
  do r += d; while (r >= 0 && r < num_rows && cols_of_row(r) < 1);
  return r;
}

// LAYER KEYS OCCUPY TWO COLUMNS EACH, starting from left, and space
// (always last) goes from there to end of grid — like modifiers on corner
// of a real keyboard, aligned with keys above. Coming from character row,
// DOWN falls on key that is UNDER that column.
#define KB_LAYER_COLS 2
static int layer_col_of(int c) {
  int i = c / KB_LAYER_COLS;
  return i < num_layer_keys ? i : num_layer_keys - 1;
}

static GfxRect key_rect(int r, int c) {
  GfxRect rect;
  rect.y = kb_y() + KB_ISLAND_PY + (float)r * KB_STEP;
  rect.h = KB_KEY;
  if (is_char_row(r)) {
    rect.x = kb_grid_x() + (float)c * KB_STEP;
    rect.w = KB_KEY;
  } else if (r == num_rows - 1) {
    // backspace / clear / (show) / DONE: last one is 1.3x width of others.
    int k = cols_of_row(r), i;
    float unit = (grid_w() - (float)(k - 1) * KB_GAP) / ((float)(k - 1) + 1.3f);
    rect.y = kb_y() + KB_ISLAND_PY + (float)(num_rows - 1) * KB_STEP - KB_GAP + 20.0f;
    rect.h = KB_EXTRA_H;
    rect.x = kb_grid_x();
    for (i = 0; i < c; i++) rect.x += unit + KB_GAP;
    rect.w = c == k - 1 ? unit * 1.3f : unit;
  } else if (is_layer_row(r)) {
    rect.x = kb_grid_x() + (float)(c * KB_LAYER_COLS) * KB_STEP;
    rect.w = layer_keys[c] == KB_K_SPACE ? kb_grid_x() + grid_w() - rect.x
                                         : (float)KB_LAYER_COLS * KB_STEP - KB_GAP;
  } else {
    int k = cols_of_row(r);
    rect.w = (grid_w() - (float)(k - 1) * KB_GAP) / (float)k;
    rect.x = kb_grid_x() + (float)c * (rect.w + KB_GAP);
  }
  return rect;
}

// Label for last row. The three keys are only point of modal with word instead
// of character, and therefore all three are in i18n table.
static const char *extra_label(int c) {
  if (c == 0) return "apagar";
  if (c == 1) return "limpar";
  if (is_password && c == 2) return mask_password ? "mostrar" : "ocultar";
  return "Concluir";
}

static void apply_key(void) {
  if (is_char_row(row)) {
    if (col >= cols_of_row(row)) return;
    if (text_len < max_chars) {
      typed_text[text_len++] = apply_case(cell[layer][row][col]);
      typed_text[text_len] = 0;
    }
    if (text_case == 1) text_case = 0;   // one touch applies to ONE character
    return;
  }
  if (is_layer_row(row)) {
    int k = layer_keys[col < num_layer_keys ? col : 0];
    if (k == KB_K_CASE) text_case = (text_case + 1) % 3;
    else if (k == KB_K_SYMBOLS) layer = !layer;
    else if (text_len < max_chars) { typed_text[text_len++] = ' '; typed_text[text_len] = 0; }
    return;
  }
  if (is_shortcut_row(row)) {
    const char *a = EMAIL_SHORTCUTS[col < num_shortcuts ? col : 0];
    // "@gmail.com" after an already typed "@" doesn't duplicate the at-sign.
    if (a[0] == '@' && strchr(typed_text, '@')) a++;
    if (text_len + (int)strlen(a) <= max_chars) {
      memcpy(typed_text + text_len, a, strlen(a) + 1);
      text_len += (int)strlen(a);
    }
    return;
  }
  if (col == 0) { if (text_len > 0) typed_text[--text_len] = 0; return; }
  if (col == 1) { text_len = 0; typed_text[0] = 0; return; }
  if (is_password && col == 2) { mask_password = !mask_password; return; }
  // "done" with empty field is not confirmation of anything: user reaching
  // there without typing wanted to look at keyboard, and closing modal with
  // READY result would make friends screen try to bind empty string.
  if (text_len < 1) return;
  close_keyboard(KEYBOARD_READY);
}

void keyboard_event(const SDL_Event *e) {
  SDL_Keycode k;
  if (is_open && st_evento(e)) return;   // TV keyboard: full value through sistexto
  if (!is_open || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    close_keyboard(KEYBOARD_CANCELLED);
    return;
  }
  if (row < 0) {
    const char *labels[3];
    int cols[3], nm = kb_modes(labels, cols), i, start, end;
    if (!bar_is_valid(col)) col = bar_default();   // mode disappeared with focus on it
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !e->key.repeat) { bar_ok(); return; }
    if (col == KB_T_FIELD) {
      // FIELD on top: DOWN descends to segmented, RIGHT returns to grid.
      if (k == SDLK_DOWN && nm) col = cols[0];
      else if (k == SDLK_RIGHT) exit_bar();
      return;
    }
    for (i = 0; i < nm && cols[i] != col; i++) {}
    if (i >= nm) return;
    // Row of segmented in which this segment fell (measured during drawing).
    for (start = i; start > 0 && seg_row[start - 1] == seg_row[i]; start--) {}
    for (end = i; end + 1 < nm && seg_row[end + 1] == seg_row[i]; end++) {}
    if (k == SDLK_LEFT) { if (i > start) col = cols[i - 1]; }
    else if (k == SDLK_RIGHT) { if (i < end) col = cols[i + 1]; else exit_bar(); }
    else if (k == SDLK_UP) {
      // Row above in segmented, at same position; from first, field.
      if (start > 0) {
        int pstart = start - 1, j;
        while (pstart > 0 && seg_row[pstart - 1] == seg_row[start - 1]) pstart--;
        j = pstart + (i - start);
        col = cols[j < start ? j : start - 1];
      } else if (field_is_focusable()) col = KB_T_FIELD;
    } else if (k == SDLK_DOWN && end + 1 < nm) {
      int pend = end + 1, j;
      while (pend + 1 < nm && seg_row[pend + 1] == seg_row[end + 1]) pend++;
      j = end + 1 + (i - start);
      col = cols[j > pend ? pend : j];
    }
    return;
  }
  if (k == SDLK_UP) {
    // UP from first row does nothing: field column is to LEFT, not above.
    int nr = next_row(row, -1);
    if (nr < 0) return;
    if (!is_char_row(row) && is_char_row(nr)) col = prev_char_col;
    row = nr;
    if (col >= cols_of_row(row)) col = cols_of_row(row) - 1;
    return;
  }
  if (k == SDLK_DOWN) {
    int nr = next_row(row, 1);
    if (nr >= num_rows) return;
    if (is_char_row(row) && !is_char_row(nr)) prev_char_col = col;
    if (is_char_row(row) && is_layer_row(nr)) col = layer_col_of(col);
    row = nr;
    if (col >= cols_of_row(row)) col = cols_of_row(row) - 1;
    return;
  }
  // LEFT on first column of ANY row enters field column.
  if (k == SDLK_LEFT)  { if (col > 0) col--; else enter_bar(-1); return; }
  if (k == SDLK_RIGHT) { if (col + 1 < cols_of_row(row)) col++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat) return;   // holding OK doesn't type same letter repeatedly
    apply_key();
    return;
  }
}

void keyboard_update(float dt, Uint32 now) {
  int r, c;
  (void)now;
  if (is_open) {
    char t[KEYBOARD_LONG + 1];
    int voice = st_estado() == ST_OUVINDO || st_estado() == ST_PERMISSAO || st_estado() == ST_VOZ_SISTEMA;
    int res;
    if (celb_pegar(CELB_TECLADO, t, sizeof t)) {
      // Phone text is ENTIRE VALUE of field, like TV keyboard: passes through
      // same alphabet filter. Does not confirm by itself — user sees what
      // arrived, and focus moves to "done": one OK saves.
      st_fechar(ST_TECLADO);
      set_from_system(t);
      memset(t, 0, sizeof t);
      phone_received = 1;
      row = num_rows - 1; col = done_col();
    }
    res = st_ler(ST_TECLADO, t, sizeof t);
    if (res == ST_PEDE_TECLADO) {
      if (row < 0) col = field_is_focusable() ? KB_T_FIELD : KB_T_IME;
      open_ime_now();
    } else if (res == ST_TEXTO || res == ST_FIM) {
      set_from_system(t);
      // "Done" on TV keyboard is modal's "done"; end of SPEECH is not — user
      // checks what speech recognizer understood before submitting.
      if (res == ST_FIM && !voice && text_len > 0) close_keyboard(KEYBOARD_READY);
    }
  }
  {
    float target = (is_open && celb_embutido(CELB_TECLADO)) ? 1.0f : 0.0f;
    anim_qr = ajustes_animacoes_reduzidas() ? target : anim_mola(anim_qr, target, dt, NV_MOLA_TELA);
  }
  for (c = 0; c < KB_T_COUNT; c++) {
    bar_anim[c] = anim_mola(bar_anim[c], (is_open && row < 0 && col == c) ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);
  }
  if (!is_open && anim < 0.002f) { anim = 0.0f; return; }
  anim = ajustes_animacoes_reduzidas()
           ? (is_open ? 1.0f : 0.0f)
           : anim_mola(anim, is_open ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  for (r = 0; r < num_rows; r++) {
    for (c = 0; c < num_cols; c++) {
      float target = (is_open && r == row && c == col) ? 1.0f : 0.0f;
      focus_anim[r][c] = ajustes_animacoes_reduzidas()
        ? target : anim_mola(focus_anim[r][c], target, dt, NV_MOLA_FOCO);
    }
  }
}

// Island material (same as ajustes_ux_desenho.inc): glass at 86% with corner
// highlight, solid #15161A; key/chip focus = filled in accent.
static void kb_neutral(GfxRect rect, float radius_px, float glass, float sr, float sg, float sb, float a) {
  if (ajustes_vidro()) gfx_cor(rect, radius_px / rect.h, 1, 1, 1, glass * a);
  else gfx_cor(rect, radius_px / rect.h, sr, sg, sb, a);
}
static void kb_accent(GfxRect rect, float radius_px, float k, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ rect.x - 12, rect.y - 2, rect.w + 24, rect.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * k * a);
  gfx_cor(rect, radius_px / rect.h, ar, ag, ab, k * a);
}

// Uppercase label: if translated text exceeds column, letter tracking decreases,
// and if it still exceeds, clipping stops at column boundary.
static float kb_caps(const char *s, float x, float y, float ew, float a) {
  char up[200];
  float trk = 2.1f, w;
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  w = txt_tracking(TXT_MINI, up, 243, 242, 239, -1, 0, 0, trk);
  if (w > ew) { trk = 0.8f; w = txt_tracking(TXT_MINI, up, 243, 242, 239, -1, 0, 0, trk); }
  if (w > ew) {
    float r;
    gfx_recorte(x, y - 4, ew, 30);
    r = txt_tracking(TXT_MINI, up, 243, 242, 239, x, y, 0.45f * a, trk);
    gfx_sem_recorte();
    return r;
  }
  return txt_tracking(TXT_MINI, up, 243, 242, 239, x, y, 0.45f * a, trk);
}

static void kb_hint(float *x, float y, const char *k, const char *l, float a) {
  TxtLinha tk = txt_linha(TXT_AJ_KBD, k, 243, 242, 239, 255);
  TxtLinha tl = txt_linha(TXT_ILHA_APOIO, l, 243, 242, 239, 255);
  float kw = tk.w + 18.0f < 34.0f ? 34.0f : tk.w + 18.0f;
  kb_neutral((GfxRect){ *x, y, kw, 30 }, 15, 0.09f, 0.141f, 0.149f, 0.173f, a);
  txt_desenhar_alpha(tk, *x + (kw - tk.w) * 0.5f, y + (30 - tk.h) * 0.5f, 0.82f * a);
  txt_desenhar_alpha(tl, *x + kw + 9, y + (30 - tl.h) * 0.5f, 0.45f * a);
  *x += kw + 9 + tl.w + 20;
}

static const char *KB_PHRASE_PHONE = "Pelo celular, o texto chega aqui para você conferir antes de concluir.";
static float kb_hint_width(const char *k, const char *l) {
  TxtLinha tk = txt_linha(TXT_AJ_KBD, k, 243, 242, 239, 255);
  TxtLinha tl = txt_linha(TXT_ILHA_APOIO, l, 243, 242, 239, 255);
  float kw = tk.w + 18.0f < 34.0f ? 34.0f : tk.w + 18.0f;
  return kw + 9 + tl.w + 20;
}

// Rows that n hints of width w[] occupy in ew (last doesn't pay margin).
static int kb_hints_rows(const float *w, int n, float ew) {
  int i, count = 1;
  float x = 0;
  for (i = 0; i < n; i++) {
    if (x > 0 && x + w[i] - 20.0f > ew) { count++; x = 0; }
    x += w[i];
  }
  return count;
}

static float kb_chips_width(TxtEstilo e, float pad, const char **labels, int a, int b) {
  float w = 10.0f;
  int i;
  for (i = a; i < b; i++)
    w += txt_linha(e, labels[i], 0, 0, 0, 255).w + 2 * pad + (i > a ? 4.0f : 0.0f);
  return w;
}

static void kb_measure(void) {
  const char *labels[3];
  int cols[3], k = kb_modes(labels, cols), i;
  float base = kb_left_w0(), ceiling = NV_TELA_W - 80.0f - 2 * KB_ISLAND_PX - KB_COL_GAP - grid_w();
  float ew, seg_need = k ? kb_chips_width(TXT_AJ_SEG, KB_SEG_PAD, labels, 0, k) : 0.0f;
  float wA[3], wB[3], hint_max = 0, h;
  const char *lc[3] = { "Digitar pelo celular", "Falar", "Teclado da TV" };
  // Hints: row with focus in bar (OK changes label: largest wins) and key row.
  for (i = 0; i < 3; i++) {
    float w = kb_hint_width("OK", lc[i]);
    if (w > hint_max) hint_max = w;
  }
  wA[0] = hint_max; wA[1] = kb_hint_width("→", "Teclado"); wA[2] = kb_hint_width("Voltar", "Cancelar");
  wB[0] = kb_hint_width("Setas", "Navegar"); wB[1] = kb_hint_width("OK", "Digitar"); wB[2] = wA[2];
  if (in_layers) {
    float w = kb_hint_width("OK", "Maiúsculas");
    if (w > wB[1]) wB[1] = w;
  }
  {
    float na = wA[0] + wA[1] + wA[2] - 20.0f, nb = wB[0] + wB[1] + wB[2] - 20.0f;
    float need = seg_need > na ? seg_need : na;
    if (nb > need) need = nb;
    ew = need > base ? need : base;
    if (ew > ceiling) ew = ceiling;
  }
  ew_measured = ew;
  // Segmented: padding, font step-down, then rows.
  seg_style = TXT_AJ_SEG; seg_pad = KB_SEG_PAD; seg_num_rows = 1;
  for (i = 0; i < 3; i++) seg_row[i] = 0;
  if (k && seg_need > ew) {
    if (kb_chips_width(TXT_AJ_SEG, KB_SEG_PAD_MIN, labels, 0, k) <= ew) {
      seg_pad = KB_SEG_PAD_MIN;
    } else if (kb_chips_width(TXT_ILHA_SEG, KB_SEG_PAD_MIN, labels, 0, k) <= ew) {
      seg_style = TXT_ILHA_SEG; seg_pad = KB_SEG_PAD_MIN;
    } else {
      int start = 0;
      for (i = 1; i <= k; i++) {
        if (i == k || kb_chips_width(TXT_AJ_SEG, KB_SEG_PAD, labels, start, i + 1) > ew) {
          int j;
          for (j = start; j < i; j++) seg_row[j] = seg_num_rows - 1;
          if (i < k) { seg_num_rows++; start = i; }
        }
      }
    }
  }
  hints_rows = kb_hints_rows(wA, 3, ew);
  {
    int fb = kb_hints_rows(wB, 3, ew);
    if (fb > hints_rows) hints_rows = fb;
  }
  // Left column height, summed in drawing order.
  h = 0;
  if (kicker_current[0]) h += 22.0f;
  h += 48.0f;
  if (current_hint[0]) h += 10.0f + txt_bloco(TXT_AJ_SUB, current_hint, 243, 242, 239, 0, 0, ew, 28.5f, 0.0f, 3);
  h += 30.0f;
  h += (!mask_password && max_chars > 0 && (ew - (float)(max_chars - 1) * KB_BOX_GAP) / (float)max_chars >= KB_BOX_W_MIN) ? KB_BOX_H : 76.0f;
  if (k) h += 22.0f + 55.0f + (float)(seg_num_rows - 1) * (55.0f + KB_ROW_GAP);
  {
    float base_h = 24.0f + 30.0f + (float)(hints_rows - 1) * (30.0f + KB_ROW_GAP);
    if (k && phone_ok()) {
      // Under segmented lives phone phrase and, with QR open, QR itself:
      // column (and modal) expands for it to fit standard size, up to
      // what screen allows — celb chooses arrangement that fits.
      float phrase = txt_bloco(TXT_ILHA_GENERO, KB_PHRASE_PHONE, 243, 242, 239, 0, 0, ew, 25.0f, 0.0f, 3);
      float cell_ceiling = NV_TELA_H - 48.0f - 2 * KB_ISLAND_PY - h - 16.0f - base_h;
      qr_h = celb_embutido(CELB_TECLADO) ? celb_embutido_altura(CELB_TECLADO, ew, cell_ceiling) : 0.0f;
      if (qr_h > 0) qr_h_prev = qr_h;   // when closing, height shrinks from previous
      h += 16.0f + anim_mistura(phrase, qr_h_prev, anim_suave(anim_qr));
    }
    h += base_h;
  }
  left_h_measured = h;
  measured = 1;
}

// Draws n hints (key label, text) wrapping across rows of width ew;
// last row is at `by`, others above it.
static void kb_hints(const char **ks, const char **ls, int n, float x0, float by, float ew, float a) {
  float w[4], x;
  int i, count, r = 0;
  for (i = 0; i < n; i++) w[i] = kb_hint_width(ks[i], ls[i]);
  count = kb_hints_rows(w, n, ew);
  x = x0;
  for (i = 0; i < n; i++) {
    if (x > x0 && x + w[i] - 20.0f > x0 + ew) { r++; x = x0; }
    {
      float xx = x;
      kb_hint(&xx, by - (float)(count - 1 - r) * (30.0f + KB_ROW_GAP), ks[i], ls[i], a);
    }
    x += w[i];
  }
}

static void kb_draw_internal(Uint32 now);
// Keyboard stays at 1080p in any "Interface size": at 100% it already occupies
// screen width (see math above), and enlarged would not fit.
void keyboard_draw(Uint32 now) {
  ESCALA_REAL_INI();
  kb_draw_internal(now);
  ESCALA_REAL_FIM();
}

static void kb_draw_internal(Uint32 now) {
  float a = anim_suave(anim), dy, x, y, ew;
  int r, c, i;
  kb_measure();
  if (anim < 0.01f) return;
  dy = (1.0f - a) * 36.0f;
  if (is_open && pointer_is_active()) {
    pointer_layer();
    pointer_target(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  }
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.84f * anim);
  {
    GfxRect p = { kb_x(), kb_y() + dy, kb_w(), kb_h() };
    float radius = 36.0f / p.h;
    gfx_rect((GfxRect){ p.x - 20, p.y - 6, p.w + 40, p.h + 46 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.40f * a);
    if (ajustes_vidro()) {
      gfx_cor(p, radius, 0.055f, 0.059f, 0.071f, 0.86f * a);
      gfx_luz_canto(p, radius, p.w * 0.22f, -p.h * 0.40f, p.h * 0.62f, 1, 1, 1, 0.10f * a);
    } else {
      gfx_cor(p, radius, 0.082f, 0.086f, 0.102f, a);
    }
  }

  // LEFT COLUMN
  x = kb_x() + KB_ISLAND_PX; ew = kb_left_w();
  y = kb_y() + dy + KB_ISLAND_PY;
  if (kicker_current[0]) { kb_caps(kicker_current, x, y + 2, ew, a); y += 22.0f; }
  {
    TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, current_title, 243, 242, 239, 255, ew);
    txt_desenhar_alpha(t, x, y, a); y += 48.0f;
  }
  if (current_hint[0]) {
    y += 10.0f;
    y += txt_bloco(TXT_AJ_SUB, current_hint, 243, 242, 239, x, y, ew, 28.5f, 0.58f * a, 3);
  }
  y += 30.0f;
  {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    // WHAT WAS TYPED: boxes when each fits glyph (short codes), else text field with cursor.
    if (!mask_password && (ew - (float)(max_chars - 1) * KB_BOX_GAP) / (float)max_chars >= KB_BOX_W_MIN) {
      float bw = (ew - (float)(max_chars - 1) * KB_BOX_GAP) / (float)max_chars, bx = x;
      if (bw > KB_BOX_W) bw = KB_BOX_W;
      for (i = 0; i < max_chars; i++) {
        GfxRect b = { bx, y, bw, KB_BOX_H };
        kb_neutral(b, 20, i < text_len ? 0.16f : 0.05f, i < text_len ? 0.20f : 0.12f, i < text_len ? 0.205f : 0.125f, i < text_len ? 0.23f : 0.145f, a);
        if (i < text_len) {
          char ch[2] = { typed_text[i], 0 };
          TxtLinha t = txt_linha(TXT_TITULO2, ch, 243, 242, 239, 255);
          txt_desenhar_alpha(t, b.x + (b.w - t.w) * 0.5f, b.y + (b.h - t.h) * 0.5f, a);
        }
        bx += bw + KB_BOX_GAP;
      }
      y += KB_BOX_H;
    } else {
      GfxRect field = { x, y, ew, 76.0f };
      float tx = field.x + 24.0f, cursor_x = tx, width_avail;
      char q[48];
      TxtLinha lq;
      kb_neutral(field, 24, 0.07f, 0.125f, 0.129f, 0.153f, a);
      // Field focus: same brightness step as before, slightly higher —
      // now lights up ALONE (TV keyboard segment is separate target)
      // and needs to be readable from 3m without adjacent pill.
      if (bar_anim[KB_T_FIELD] > 0.01f) {
        float kf = bar_anim[KB_T_FIELD];
        gfx_rect((GfxRect){ field.x - 12, field.y - 2, field.w + 24, field.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.45f * kf * a);
        kb_neutral(field, 24, 0.07f, 0.125f, 0.129f, 0.153f, a);   // over its own shadow
        kb_neutral(field, 24, 0.17f * kf, 0.255f, 0.263f, 0.298f, kf * a);
      }
      if (ime_ok() && pointer_is_active()) pointer_target(field.x, field.y, field.w, field.h, focus_bar, NULL, KB_T_FIELD, 0);
      snprintf(q, sizeof q, text_len == 1 ? i18n("%d caractere") : i18n("%d caracteres"), text_len);
      lq = txt_linha(TXT_ILHA_APOIO, q, 243, 242, 239, 255);
      txt_desenhar_alpha(lq, field.x + field.w - 24 - lq.w, field.y + (field.h - lq.h) * 0.5f, 0.4f * a);
      width_avail = field.w - 48.0f - lq.w - 20.0f;
      if (text_len) {
        char bullets[KEYBOARD_LONG * 3 + 1];
        TxtLinha t;
        float ox;
        if (mask_password) {
          int i2;
          for (i2 = 0; i2 < text_len && i2 < KEYBOARD_LONG; i2++) memcpy(bullets + i2 * 3, "\xE2\x80\xA2", 3);
          bullets[i2 * 3] = 0;
        }
        t = txt_linha(TXT_AJ_INSP, mask_password ? bullets : typed_text, 243, 242, 239, 255);
        ox = t.w > width_avail ? t.w - width_avail : 0.0f;
        gfx_recorte(tx, field.y, width_avail, field.h);
        txt_desenhar_alpha(t, tx - ox, field.y + (field.h - t.h) * 0.5f, a);
        gfx_sem_recorte();
        cursor_x = tx + (t.w - ox);
      }
      {
        float op = ajustes_animacoes_reduzidas() ? 0.95f : (((now / 500) % 2) ? 0.35f : 0.95f);
        gfx_cor((GfxRect){ cursor_x + 3.0f, field.y + 20.0f, 2.0f, 36.0f }, 0.5f, ar, ag, ab, op * a);
      }
      y += 76.0f;
    }
  }

  // MODES: TV keyboard, voice and phone, in a segmented control (whatever exists).
  {
    const char *labels[3];
    int cols[3], k = kb_modes(labels, cols);
    if (k) {
      float h = 55.0f, ih = 45.0f;
      int r_seg, start = 0;
      y += 22.0f;
      for (r_seg = 0; r_seg < seg_num_rows; r_seg++) {
        int end = start;
        float sx = x, py = y + (float)r_seg * (h + KB_ROW_GAP);
        while (end < k && seg_row[end] == r_seg) end++;
        kb_neutral((GfxRect){ sx, py, kb_chips_width(seg_style, seg_pad, labels, start, end), h }, h * 0.5f, 0.06f, 0.114f, 0.118f, 0.137f, a);
        sx += 5.0f;
        for (i = start; i < end; i++) {
          int focused = is_open && row < 0 && col == cols[i];
          int ti = focused ? ajustes_tinta_foco() : 243;
          TxtLinha t = txt_linha(seg_style, labels[i], ti, ti, ti, 255);
          GfxRect rect = { sx, py + 5.0f, t.w + 2 * seg_pad, ih };
          if (is_open && pointer_is_active()) pointer_target(rect.x, rect.y, rect.w, rect.h, focus_bar, NULL, cols[i], 0);
          if (focused) kb_accent(rect, ih * 0.5f, bar_anim[cols[i]] > 0.5f ? 1.0f : bar_anim[cols[i]] * 2.0f, a);
          else if (cols[i] == KB_T_PHONE && (phone_received || celb_embutido(CELB_TECLADO)))
            kb_neutral(rect, ih * 0.5f, 0.14f, 0.204f, 0.212f, 0.243f, a);   // mode in use stays marked
          txt_desenhar_alpha(t, rect.x + seg_pad, rect.y + (ih - t.h) * 0.5f, (focused ? 1.0f : 0.55f) * a);
          sx += rect.w + 4.0f;
        }
        start = end;
      }
      y += h + (float)(seg_num_rows - 1) * (h + KB_ROW_GAP);
      if (phone_ok()) {
        float t = anim_suave(anim_qr);
        y += 16.0f;
        if (t < 0.99f) txt_bloco(TXT_ILHA_GENERO, KB_PHRASE_PHONE, 243, 242, 239, x, y, ew, 25.0f, 0.45f * a * (1.0f - t), 3);
        // PHONE QR HERE: in place of phrase, under segment that opened it.
        // Was loose card in corner of screen (celb_desenhar), unlinked to modal.
        if (qr_h > 0 && celb_embutido(CELB_TECLADO))
          celb_desenhar_em(CELB_TECLADO, (GfxRect){ x, y, ew, qr_h }, a * t);
      }
    }
  }

  // Hints at column base.
  {
    const char *notice = st_dono() == ST_TECLADO ? st_aviso() : "";
    float by = kb_y() + dy + kb_h() - KB_ISLAND_PY - 30.0f;
    if (notice[0]) {
      TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, notice, 240, 196, 140, 255, ew);
      txt_desenhar_alpha(t, x, by + (30 - t.h) * 0.5f, a);
    } else if (phone_received && row == num_rows - 1) {
      TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, "Recebido do celular. Confira e aperte Concluir.", 243, 242, 239, 255, ew);
      txt_desenhar_alpha(t, x, by + (30 - t.h) * 0.5f, 0.62f * a);
    } else if (row < 0) {
      const char *ks[3] = { "OK", "→", "Voltar" };
      const char *ls[3] = { col == KB_T_PHONE ? "Digitar pelo celular" : col == KB_T_VOICE ? "Falar" : "Teclado da TV", "Teclado", "Cancelar" };
      kb_hints(ks, ls, 3, x, by, ew, a);
    } else {
      const char *ks[3] = { "Setas", "OK", "Voltar" };
      const char *ls[3] = { "Navegar", "Digitar", "Cancelar" };
      // On uppercase key, OK says what it is: key only has arrow.
      if (is_layer_row(row) && layer_keys[col < num_layer_keys ? col : 0] == KB_K_CASE) ls[1] = "Maiúsculas";
      kb_hints(ks, ls, 3, x, by, ew, a);
    }
  }

  // RIGHT COLUMN: the grid.
  for (r = 0; r < num_rows; r++) {
    for (c = 0; c < cols_of_row(r); c++) {
      float k = focus_anim[r][c];
      GfxRect base = key_rect(r, c);
      int extra = r == num_rows - 1, done_key = extra && c == cols_of_row(r) - 1;
      int is_ly = is_layer_row(r), kc = is_ly ? layer_keys[c] : -1;
      // Uppercase active (one touch or locked) and symbols layer visible:
      // key stays RAISED, it is its state, not focus.
      int active = (kc == KB_K_CASE && text_case) || (kc == KB_K_SYMBOLS && layer);
      // Layer keys are wide: 10% of 848 px would exceed island.
      float scale = 1.0f + (extra ? 0.0f : KB_SCALE * k * (is_ly ? 0.3f : 1.0f));
      float text_alpha = 0.9f;
      TxtEstilo style = TXT_AJ_TIT28;
      GfxRect target_r;
      const char *s;
      char ch[2];
      int tone;
      base.y += dy;
      if (is_open && pointer_is_active()) pointer_target(base.x, base.y, base.w, base.h, focus_key, NULL, r, c);
      target_r.w = base.w * scale; target_r.h = base.h * scale;
      target_r.x = base.x - (target_r.w - base.w) * 0.5f;
      target_r.y = base.y - (target_r.h - base.h) * 0.5f;
      if (extra) kb_neutral(target_r, target_r.h * 0.5f, 0.08f, 0.141f, 0.149f, 0.173f, a);
      else if (active) kb_neutral(target_r, 20.0f * scale, 0.17f, 0.235f, 0.243f, 0.275f, a);
      else kb_neutral(target_r, 20.0f * scale, 0.07f, 0.125f, 0.129f, 0.153f, a);
      if (k > 0.01f) kb_accent(target_r, extra ? target_r.h * 0.5f : 20.0f * scale, k, a);
      if (is_shortcut_row(r)) { s = EMAIL_SHORTCUTS[c]; style = TXT_AJ_ESTADO; }
      else if (is_ly) {
        style = TXT_AJ_SEG;
        if (kc == KB_K_CASE) {
          // Shift arrow, and Caps Lock arrow when LOCKED (with dot in highlight color);
          // single touch only raises key. No word: key has two columns and
          // "Großbuchstaben" doesn't fit — word is in base hint. On symbols layer
          // there is no letter to change: arrow dims.
          s = text_case == 2 ? "\xE2\x87\xAA" : "\xE2\x87\xA7";   // U+21EA, U+21E7 (Inter)
          style = TXT_AJ_TIT28;
          text_alpha = layer ? 0.4f : (text_case ? 1.0f : 0.85f);
        } else if (kc == KB_K_SYMBOLS) { s = layer ? "abc" : "#+="; text_alpha = 0.85f; }
        else { s = i18n("espaço"); text_alpha = 0.85f; }
        // Translated label on fixed-width key: one font step down before cutting.
        if (txt_linha(style, s, 243, 243, 243, 255).w > target_r.w - 28.0f) style = TXT_AJ_ESTADO;
      } else if (is_char_row(r)) {
        ch[0] = apply_case(cell[layer][r][c]); ch[1] = 0;
        s = ch;
        if (ch[0] == ' ') { s = i18n("espaço"); style = TXT_AJ_ESTADO; }
      } else { s = extra_label(c); style = TXT_AJ_SEG; text_alpha = 0.85f; }
      // STEPPED text color (key of text.c line cache).
      tone = k >= 0.5f ? ajustes_tinta_foco() : 243;
      {
        TxtLinha l = is_ly ? txt_linha_corta(style, s, tone, tone, tone, 255, target_r.w - 28.0f)
                           : txt_linha(style, s, tone, tone, tone, 255);
        const char *icon = extra ? (c == 0 ? "aj_delete" : done_key ? "aj_check" : NULL) : NULL;
        int dot = kc == KB_K_CASE && text_case == 2;
        float icon_w = icon ? 22.0f + 10.0f : dot ? 8.0f + 10.0f : 0.0f;
        float lx = target_r.x + (target_r.w - l.w - icon_w) * 0.5f;
        if (icon) gfx_icone((GfxRect){ lx, target_r.y + (target_r.h - 22) * 0.5f, 22, 22 }, icon, tone / 255.0f, tone / 255.0f, tone / 255.0f, (k >= 0.5f ? 1.0f : 0.85f) * a);
        if (dot) {
          float ar, ag, ab;
          ajustes_acento(&ar, &ag, &ab);
          if (k >= 0.5f) ar = ag = ab = (float)tone / 255.0f;   // over highlight, dot matches ink
          gfx_cor((GfxRect){ lx, target_r.y + (target_r.h - 8.0f) * 0.5f, 8.0f, 8.0f }, 0.5f, ar, ag, ab, a);
        }
        txt_desenhar_alpha(l, lx + icon_w, target_r.y + (target_r.h - l.h) * 0.5f, (k >= 0.5f ? 1.0f : text_alpha) * a);
      }
    }
  }
}

// Backward-compatibility shims
const char *teclado_alfabeto(void) { return keyboard_alphabet(); }
void teclado_abrir(const char *titulo, const char *dica, int max) { keyboard_open(titulo, dica, max); }
void teclado_abrir_com(const char *titulo, const char *dica, int max,
                       const char *alfabeto, const char *inicial) {
  keyboard_open_with(titulo, dica, max, alfabeto, inicial);
}
void teclado_tipo(int tipo) { keyboard_type(tipo); }
void teclado_mascarar(int liga) { keyboard_mask(liga); }
void teclado_contexto(const char *kicker) { keyboard_context(kicker); }
int  teclado_mascarado(void) { return keyboard_is_masked(); }
int  teclado_aberto(void) { return keyboard_is_open(); }
int  teclado_foco_campo(void) { return keyboard_focus_field(); }
int  teclado_camada(void) { return keyboard_layer(); }
int  teclado_caixa(void) { return keyboard_case(); }
void teclado_teste_modos(int ime, int voz, int cel) { keyboard_test_modes(ime, voz, cel); }
int  teclado_foco_tipo(void) { return keyboard_focus_type(); }
void teclado_teste_quebra(int f0, int f1, int f2) { keyboard_test_wrap(f0, f1, f2); }
void teclado_evento(const SDL_Event *e) { keyboard_event(e); }
void teclado_atualizar(float dt, Uint32 agora) { keyboard_update(dt, agora); }
void teclado_desenhar(Uint32 agora) { keyboard_draw(agora); }
int  teclado_resultado(void) { return keyboard_result(); }
const char *teclado_texto(void) { return keyboard_text(); }
void teclado_esquecer(void) { keyboard_forget(); }
