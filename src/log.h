// On-screen local log panel, with a first-use explanation of the shortcut.
// webOS reads the end of /tmp/nuvio.log. Linux logs to the terminal unless
// NUVIO_LOG selects a file. The panel opens with the red TV key, F9, or Settings.
// With the panel closed, no file is opened or line formatted.
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

#ifdef REGISTRO_TESTE
void log_test_text(const char *t);
void log_test_no_source(int s);
void log_test_state(int a, int ed, int pause, int focus_line, int det, int nov);
int  log_test_find(const char *snippet, int last);
void log_test_notice(int a);
void log_test_stage_focus(int f);
#endif

// Backward-compatibility shims
const char *registro_arquivo(void);
int  registro_evento(const SDL_Event *e);
void registro_desenhar(void);
void registro_aviso_primeira_vez(void);
int  registro_aberto(void);
void registro_abrir(void);
int  registro_ja_aberto(void);

#ifdef REGISTRO_TESTE
void registro_teste_texto(const char *t);
void registro_teste_sem_fonte(int s);
void registro_teste_estado(int a, int ed, int pausa, int focoLinha, int det, int nov);
int  registro_teste_achar(const char *trecho, int ultima);
void registro_teste_aviso(int a);
void registro_teste_foco_etapa(int f);
#endif

#endif
