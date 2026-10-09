// ON-SCREEN LOG PANEL, and the first-run banner teaching the gesture to open it.
//
// WHY IN C AND NOT HTML SHELL: On Tizen target there already is a diagnostic
// panel built in DOM (tools/tizen-shell.html), born from a real need: the TV
// showed a black screen and no DevTools were at hand. But on webOS NO SHELL
// EXISTS — the app is a native ARM binary, printf goes to /tmp/nuvio.log due
// to startup freopen, and reading that file requires telnet or photographing the screen.
// In other words: without a C version the user would have log on one TV and
// not on the other, with two different gestures. This module provides the SAME
// gesture (red key) on both.
//
// WHERE THE LINES COME FROM: from the log that the app ALREADY produces, not from
// a new mechanism. The app has hundreds of printfs scattered around; rewriting
// them to go through a custom macro would be the riskiest possible change and,
// worse, a ring buffer fed only by CONVERTED printfs would show half a log — and
// a diagnostic panel that hides half the lines lies about what happened. So:
//   - webOS (and Mac with NUVIO_LOG): reads the END of the file when the panel opens.
//     With the panel closed it costs nothing: no file is opened, no line is formatted.
//   - Tizen: no useful file exists (freopen doesn't even happen there). printf goes
//     to Module.print, which the shell sends to console AND stores as plain text in
//     window.__nvLinhas — and from there this panel reads. Same source, no second
//     capture mechanism.
//
// WHAT IS CLIPPED ON DISPLAY: The log of a video app carries watched titles
// and source URLs, and addon/debrid URLs contain the debrid key embedded in
// the PATH (see debrid.c: "[debrid] %s -> %.60s"). A panel on the living room
// TV exposes this to anyone in the room. Decision: the TITLE stays (it's already
// on the app screen and is the information making the log useful), the URL
// is cut at the HOST — from "https://casa.exemplo/d/KEY/file.mkv" remains
// "https://casa.exemplo/...". The host matters for diagnosing ("which addon
// responded"), the path is a credential.
#ifndef NV_LOG_H
#define NV_LOG_H
#include <SDL2/SDL.h>

// File path where app redirects stdout/stderr, or NULL when this build redirects nothing.
// main.c uses THIS for startup freopen: a single place decides the path, and it is
// the same one the panel reads. Without this both sides would match "/tmp/nuvio.log"
// by coincidence.
#define LOG_WINDOW 40960
#define LOG_MAX    200
#define LOG_COL    512

#define REG_JANELA LOG_WINDOW
#define REG_MAX    LOG_MAX
#define REG_COL    LOG_COL

const char *log_file_path(void);

// 1 when event was CONSUMED. Caller must return immediately — same convention
// as other layers in app.c.
int  log_event(const SDL_Event *e);

// Draws on top of EVERYTHING. With panel closed and no notice pending, costs an
// integer check: no file reading or per-frame formatting.
void log_draw(void);

// Opens first-run notice, if this installation never displayed it. Can be called
// on every frame: decision happens once and is cached.
void log_first_time_notice(void);

// 1 while panel is open. app.c uses to FREEZE background screen: panel is glass
// (Glass UI, 03/10), so the last screen frame is painted ONCE to a texture on
// opening and subsequent frames simply copy that texture.
int  log_is_open(void);

// Opens panel without the red key (Settings > About & help > "View log on screen"):
// not every LG remote has the color row.
void log_open(void);
// 1 after panel was opened via Settings once ("NEW" badge on the row).
int  log_already_opened(void);

// LOG SEND PANEL (Settings > About & help > Send log, and "Send now" in panel):
// modal island with the end of the log, progress, six-character receipt CODE
// (regcodigo.h) or failure reason (server, TV offline, timeout). Opening triggers
// transmission immediately, unless offline: then it explains and waits for network.
void log_send_open(void);
int  log_send_is_open(void);
int  log_send_event(const SDL_Event *e);   // 1 = consumed
void log_send_draw(void);
// End of log in monospace, in a box ("package" in mockup): preview for "Send log"
// row in Settings inspector and on left of send panel.
// `n` = number of lines; `right_label` is text to the right of file name.
void log_packet(float x, float y, float w, float h, int n, const char *right_label, float a);

#ifdef REGISTRO_TESTE
void log_test_text(const char *t);
void log_test_no_source(int s);
void log_test_state(int a, int ed, int pause, int focus_line, int det, int nov);
int  log_test_find(const char *snippet, int last);
void log_test_notice(int a);
void log_test_stage_focus(int f);
void log_test_send(int open);
void log_test_send_focus(int f);
#endif

// Backward-compatibility shims
const char *registro_arquivo(void);
int  registro_evento(const SDL_Event *e);
void registro_desenhar(void);
void registro_aviso_primeira_vez(void);
int  registro_aberto(void);
void registro_abrir(void);
int  registro_ja_aberto(void);
void registro_envio_abrir(void);
int  registro_envio_aberto(void);
int  registro_envio_evento(const SDL_Event *e);
void registro_envio_desenhar(void);
void registro_pacote(float x, float y, float w, float h, int n, const char *rotDir, float a);

#ifdef REGISTRO_TESTE
void registro_teste_texto(const char *t);
void registro_teste_sem_fonte(int s);
void registro_teste_estado(int a, int ed, int pausa, int focoLinha, int det, int nov);
int  registro_teste_achar(const char *trecho, int ultima);
void registro_teste_aviso(int a);
void registro_teste_foco_etapa(int f);
void registro_teste_envio(int abrir);
void registro_teste_envio_foco(int f);
#endif

#endif
