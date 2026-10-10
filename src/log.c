#include "app_id.h"
#include "log.h"
#include "data.h"
#include "gfx.h"
#include "text.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "ajustes.h"
#include "redesaude.h"
#include "tex_cache.h"
#define NV_ESCALA_TELA   // whole file measures by virtual screen (escala.h)
#include "escala.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

// RED KEY ON webOS: 486, VERIFIED in SDK header compiling ARM target
// (SDL_webOS.h: SDL_WEBOS_SCANCODE_RED 486, in same table from which
// layout.h took BACK=482 and BLUE=489). Not in layout.h because this file
// does not own it.
//
// What was NOT verified: whether key reaches app on physical hardware. BACK needed
// its own hint on window creation (SDL_WEBOS_ACCESS_POLICY_KEYS_BACK);
// for color keys no equivalent hint exists. If red doesn't open panel on LG TV,
// that is most likely cause rather than keycode number — which is why
// Settings > About and help > "View log on screen" exists (log_open).
#define LOG_SCANCODE_RED 486

// F9 opens the panel on Linux without conflicting with search input.
#define LOG_KEY_KEYBOARD SDLK_F9

// How many bytes from END of file are read, and how many lines are kept.
// What matters is always the end. Lines not fitting in 200 still count
// in area COUNTS ("All 214"): panel shows 200 of last N.
//
// LOG_COL is 512 and not width that FITS on screen: keeping entire line allows
// is_failure to find "<<< NO PERSISTENCE" living at END of FPS line, and
// detail view to display entire line.
#define LOG_WINDOW 40960
#define LOG_MAX    200
#define LOG_COL    512

// LIVE (owner, 03/10): with panel open file is re-read every second —
// only when file size changed. Scrolling up PAUSES (list stops moving under
// focus) and counts new lines; descending to end resumes following.
#define LOG_RELOAD_MS 1000u

static int is_open;
static int notice_visible;
static int notice_decided;
// Release of SAME touch that opened/closed something here. PITFALL PAID IN THIS
// REPO: KEYUP of a touch that closed a layer leaks to background screen
// (issue #8). Every time this module acts on KEYDOWN it stores key and
// swallows KEYUP. SYM AND SCANCODE: webOS color keys arrive by SCANCODE.
static SDL_Keycode soltarSym;
static int soltarScan = -1;
static void engolirSoltura(const SDL_Event *e) {
  soltarSym  = e->key.keysym.sym;
  soltarScan = e->key.keysym.scancode;
}

// --- THE LINES ---------------------------------------------------------------
// Areas: bracketed tag of printf ([net], [video]...) grouped into five.
// Every line falls into exactly one (unrecognized goes to System), so counts
// of the five sum up to "All".
enum { RG_TUDO = 0, RG_PROB, RG_VIDEO, RG_REDE, RG_FONTES, RG_IMAGENS, RG_SISTEMA, RG_N };
typedef struct {
  char t[LOG_COL];          // entire line, with URLs cut at host
  short msg;                // where text starts, after "[area] "
  char area[24];            // "video", "quadros" (FPS= line), "" without tag
  unsigned char sev;        // 0 normal, 1 warning, 2 error
  unsigned char grupo;      // RG_VIDEO..RG_SISTEMA
  unsigned char fps;        // FPS= line from main.c
} LogLine;
typedef LogLine RegLinha;

static LogLine lin[LOG_MAX];
static int nLin;            // stored lines (last LOG_MAX)
static int nLidas;          // lines in read window (>= nLin)
static int cont[RG_N];      // per area, over read window
static int nErros, nAvisos;
static int semFonte;        // 1 = nowhere to read from in this build
static long marcaLida;      // file size on last read
static Uint32 ultLeitura;

static char cru[LOG_WINDOW + 1];

const char *log_file_path(void) {
  static char caminho[256];
  static int resolvido;
  if (!resolvido) {
    const char *env = getenv("NUVIO_LOG");
    resolvido = 1;
    if (env && env[0]) snprintf(caminho, sizeof caminho, "%s", env);
#if defined(NV_LINUX_DESKTOP)
    // Linux logs to the terminal unless a local log file is requested:
    // NUVIO_LOG=/tmp/nuvio.log bash tools/linux.sh
    else caminho[0] = 0;
#else
#ifdef NV_DTS_DEBUG
    else snprintf(caminho, sizeof caminho, "/tmp/" NV_APP_ID ".log");
#else
    else snprintf(caminho, sizeof caminho, "/tmp/nuvio.log");
#endif
#endif
  }
  return caminho[0] ? caminho : NULL;
}

// Cuts URL at HOST. See privacy note in log.h: debrid key travels in URL PATH,
// and this panel appears on living room TV. Only masks when path is 4+ bytes
// (size of "/…" in UTF-8): replacement never grows the line.
static void mascarar(char *s) {
  char *p = s;
  while ((p = strstr(p, "://")) != NULL) {
    char *h = p + 3, *fim;
    while (*h && *h != '/' && *h != '?' && *h != ' ') h++;
    if (*h != '/' && *h != '?') { p = h; continue; }
    fim = h;
    while (*fim && *fim != ' ') fim++;
    if (fim - h < 4) { p = fim; continue; }
    memmove(h + 4, fim, strlen(fim) + 1);
    memcpy(h, "/\xe2\x80\xa6", 4);
    p = h + 4;
  }
}

// Trims UTF-8 garbage at END: snprintf truncates by BYTE, and invalid
// sequence makes SDL_ttf return EMPTY line.
static void aparaUtf8(char *s) {
  size_t n = strlen(s), i;
  unsigned char c;
  int quer;
  if (!n) return;
  i = n;
  while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80 && n - i < 3) i--;
  if (i == 0) return;
  c = (unsigned char)s[i - 1];
  quer = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
  if ((int)(n - i + 1) < quer) s[i - 1] = 0;
}

// ERROR: what panel painted red, unchanged (heuristic, not classification:
// C doesn't know which printf was error; "Iron Man" comes out red. False
// positive costs a colored line; false negative, defect goes unnoticed on photo).
static int ehFalha(const char *s) {
  return strstr(s, "<<<") || strstr(s, "FALH") || strstr(s, "falh") ||
         strstr(s, "erro") || strstr(s, "Erro") || strstr(s, "ERRO");
}
// "HTTP 4xx" or "HTTP 5xx" anywhere on line; returns status code.
static int httpErro(const char *s) {
  const char *p = s;
  while ((p = strstr(p, "HTTP ")) != NULL) {
    p += 5;
    if ((p[0] == '4' || p[0] == '5') && p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9' &&
        !(p[3] >= '0' && p[3] <= '9')) return (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
  }
  return 0;
}
// WARNING (amber, owner decision 03/10): server or connection complained,
// but app continued. Short list deliberately: words app printfs use.
static int ehAviso(const char *s) {
  return strstr(s, "sem resposta") || strstr(s, "caiu") || strstr(s, "corte") ||
         strstr(s, "nao se despediu") ||
         strstr(s, "recusou") || httpErro(s);
}

static const char *const AREA_VIDEO[] = {
  "video", "player", "mkv", "mkvass", "legenda", "legendas", "ass", "serieaud",
  "posplay", "pg", "seekr", "trailer", "4k", "vazao", "proxy-ts", "livetv",
  "livetv-diag", "xtream", "xtepg", "stalker", "epg", "guia", "faixas", NULL };
static const char *const AREA_REDE[] = {
  "rede", "addons", "sync", "avisos", "trakt", "simkl", "recomenda", "nuvem",
  "atualizacao", "noticia", "noticias", "tmdb", "sessao", "login", "celular",
  "telemetria", "diagnostico", NULL };
static const char *const AREA_FONTES[] = {
  "fonte", "voltafonte", "fontecache", "fontes", "debrid", "desc", "p2p",
  "streams", "extras", NULL };
static const char *const AREA_IMAGENS[] = {
  "tex", "tex-trace", "tex-nitidez", "arte", "poster", "webp", "jpeg", "gif",
  "hero", "cor", "desfoque", "logo", NULL };
static int naLista(const char *const *l, const char *a) {
  for (; *l; l++) if (!strcmp(*l, a)) return 1;
  return 0;
}
static int grupoDe(const char *a) {
  if (naLista(AREA_VIDEO, a)) return RG_VIDEO;
  if (naLista(AREA_REDE, a)) return RG_REDE;
  if (naLista(AREA_FONTES, a)) return RG_FONTES;
  if (naLista(AREA_IMAGENS, a)) return RG_IMAGENS;
  return RG_SISTEMA;
}

// Tag, severity and area of an already masked line.
static void classificar(RegLinha *L) {
  const char *s = L->t;
  L->msg = 0; L->area[0] = 0; L->fps = 0;
  if (s[0] == '[') {
    const char *f = strchr(s, ']');
    if (f && f - s - 1 > 0 && f - s - 1 < (long)sizeof L->area) {
      memcpy(L->area, s + 1, (size_t)(f - s - 1));
      L->area[f - s - 1] = 0;
      f++;
      while (*f == ' ') f++;
      L->msg = (short)(f - s);
    }
  } else if (!strncmp(s, "FPS=", 4)) {
    snprintf(L->area, sizeof L->area, "quadros");
    L->fps = 1;
  }
  L->sev = ehFalha(s) ? 2 : ehAviso(s) ? 1 : 0;
  L->grupo = (unsigned char)(L->fps ? RG_SISTEMA : grupoDe(L->area));
}

// Splits read block into lines and keeps last LOG_MAX. `parcial` = first line
// may be cut in half (file read from end) and is discarded — half a log line
// confuses more than it helps.
static void fatiar(char *texto, int parcial) {
  static char *ptr[LOG_WINDOW / 2];
  int n = 0, i, k;
  char *p = texto, *q;
  nLin = 0; nLidas = 0; nErros = nAvisos = 0;
  memset(cont, 0, sizeof cont);
  if (parcial) {
    q = strchr(p, '\n');
    if (!q) return;
    p = q + 1;
  }
  while (*p && n < (int)(sizeof ptr / sizeof *ptr)) {
    q = strchr(p, '\n');
    if (q) *q = 0;
    { char *fim = p + strlen(p);
      while (fim > p && (fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0; }
    if (*p) ptr[n++] = p;
    if (!q) break;
    p = q + 1;
  }
  nLidas = n;
  k = n > LOG_MAX ? n - LOG_MAX : 0;
  for (i = 0; i < n; i++) {
    static RegLinha tmp;
    RegLinha *L = i >= k ? &lin[i - k] : &tmp;
    snprintf(L->t, LOG_COL, "%s", ptr[i]);
    aparaUtf8(L->t);
    mascarar(L->t);
    classificar(L);
    cont[L->grupo]++;
    if (L->sev) { cont[RG_PROB]++; if (L->sev == 2) nErros++; else nAvisos++; }
  }
  nLin = n - k;
  cont[RG_TUDO] = nLidas;
}


// Current source mark (file bytes); -1 without source.
static long marcaAtual(void) {
  struct stat st;
  const char *c = log_file_path();
  if (!c || stat(c, &st) != 0) return -1;
  return (long)st.st_size;
}

#ifdef REGISTRO_TESTE
static const char *regTesteTexto;
static int regTesteSemFonte;
#endif

static void carregar(void) {
  nLin = 0; nLidas = 0; semFonte = 0;
  ultLeitura = SDL_GetTicks();
  memset(cont, 0, sizeof cont);
#ifdef REGISTRO_TESTE
  if (regTesteSemFonte) { semFonte = 1; return; }
  if (regTesteTexto) {
    snprintf(cru, sizeof cru, "%s", regTesteTexto);
    fatiar(cru, 0);
    marcaLida = (long)strlen(regTesteTexto);
    return;
  }
#endif
  { const char *caminho = log_file_path();
    FILE *f;
    long tam, comeco;
    size_t n;
    if (!caminho) { semFonte = 1; return; }
    f = fopen(caminho, "rb");
    if (!f) { semFonte = 1; return; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); semFonte = 1; return; }
    tam = ftell(f);
    if (tam < 0) { fclose(f); semFonte = 1; return; }
    comeco = tam > (long)LOG_WINDOW ? tam - (long)LOG_WINDOW : 0;
    fseek(f, comeco, SEEK_SET);
    n = fread(cru, 1, LOG_WINDOW, f);
    cru[n] = 0;
    fclose(f);
    marcaLida = tam;
    fatiar(cru, comeco > 0); }
}

// --- PANEL STATE -------------------------------------------------------------
static int area;            // RG_* of tab
static int segEd;           // picking area (segmented in accent)
static int pausado;         // ↑ paused: list does not move
static int foco = -1;       // line in focus (index in vis), paused
static int detalhe;         // focused line open in full
static int novas;           // new lines arrived since pause
static long marcaPausa;
static int focoEtapa;       // System with stage trail: stage in focus

// Filtered list: index in lin and whether line is CONTEXT (dimmed, in Problems tab).
static int vis[LOG_MAX * 2];
static unsigned char visCtx[LOG_MAX * 2];
static int nVis;
static void filtrar(void) {
  int i;
  nVis = 0;
  for (i = 0; i < nLin; i++) {
    const RegLinha *L = &lin[i];
    if (area == RG_TUDO) { vis[nVis] = i; visCtx[nVis++] = 0; continue; }
    if (area == RG_PROB) {
      if (!L->sev) continue;
      if (i > 0 && !lin[i - 1].sev && !lin[i - 1].fps && (!nVis || vis[nVis - 1] != i - 1)) {
        vis[nVis] = i - 1; visCtx[nVis++] = 1;
      }
      vis[nVis] = i; visCtx[nVis++] = 0;
      continue;
    }
    if (L->grupo == area) { vis[nVis] = i; visCtx[nVis++] = 0; }
  }
}

static void recarregar(void) {
  carregar();
  filtrar();
  if (foco >= nVis) foco = nVis - 1;
}

static void pausar(int focoEm) {
  pausado = 1;
  novas = 0;
  marcaPausa = marcaLida;
  foco = focoEm < 0 ? 0 : focoEm >= nVis ? nVis - 1 : focoEm;
  detalhe = 1;
}
static void seguir(void) {
  pausado = 0; foco = -1; detalhe = 0; novas = 0;
  recarregar();
}

// Called per frame with panel open: reloads live, or counts new lines.
static void passoAoVivo(void) {
  Uint32 agora = SDL_GetTicks();
  long m;
  if (agora - ultLeitura < LOG_RELOAD_MS) return;
  ultLeitura = agora;
#ifdef REGISTRO_TESTE
  if (regTesteTexto || regTesteSemFonte) return;
#endif
  fflush(stdout);
  m = marcaAtual();
  if (m == marcaLida) return;
  if (!pausado) { recarregar(); return; }
  // Paused: count lines arriving after pause without touching list.
  { const char *c = log_file_path();
    FILE *f = c ? fopen(c, "rb") : NULL;
    if (f) {
      static char buf[LOG_WINDOW];
      long de = marcaPausa > m - (long)LOG_WINDOW ? marcaPausa : m - (long)LOG_WINDOW;
      size_t n, k;
      int q = 0;
      fseek(f, de, SEEK_SET);
      n = fread(buf, 1, sizeof buf, f);
      fclose(f);
      for (k = 0; k < n; k++) if (buf[k] == '\n') q++;
      novas = q;
    } }
  if (novas < 0) novas = 0;
}

int log_is_open(void) { return is_open; }

static void abrir(void) {
  is_open = 1;
  area = RG_TUDO; segEd = 0; pausado = 0; foco = -1; detalhe = 0;
  novas = 0; focoEtapa = 0;
  fflush(stdout);
  recarregar();
}
// "NEW" in Settings row until first opening.
static int jaAberto = -1;
int log_already_opened(void) {
  if (jaAberto < 0) { char *s = data_read("registro-aberto.txt"); jaAberto = s != NULL; free(s); }
  return jaAberto;
}
void log_open(void) {
  if (is_open) return;
  abrir();
  if (!log_already_opened()) { jaAberto = 1; data_write("registro-aberto.txt", "1\n"); }
}

static void fecharAviso(void) {
  if (!notice_visible) return;
  notice_visible = 0;
  data_write("aviso-log.txt", "1\n");
}

void log_first_time_notice(void) {
  char *s;
  if (notice_decided) return;
  notice_decided = 1;
  s = data_read("aviso-log.txt");
  if (s) { free(s); return; }
  notice_visible = 1;
}

static int ehAlternar(const SDL_Event *e) {
  if (e->key.repeat) return 0;
  if (e->key.keysym.sym == LOG_KEY_KEYBOARD) return 1;
  if (e->key.keysym.scancode == LOG_SCANCODE_RED) return 1;
  return 0;
}

static int ehVoltar(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}
static int ehOk(SDL_Keycode k) { return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE; }

static int etapasN(void);   // log_island.inc
static int temEtapas(void) { return area == RG_SISTEMA && etapasN() > 0; }

static void trocarArea(int d) {
  int a = area + d;
  if (a < 0 || a >= RG_N) return;
  area = a; focoEtapa = 0;
  if (pausado) { pausado = 0; foco = -1; detalhe = 0; }
  filtrar();
}

int log_event(const SDL_Event *e) {
  int ehTecla = (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP || e->type == SDL_TEXTINPUT);

  if (e->type == SDL_KEYUP &&
      ((soltarSym && e->key.keysym.sym == soltarSym) ||
       (soltarScan >= 0 && (int)e->key.keysym.scancode == soltarScan))) {
    soltarSym = 0;
    soltarScan = -1;
    return 1;
  }

  if (e->type == SDL_KEYDOWN && ehAlternar(e)) {
    fecharAviso();
    if (is_open) is_open = 0;
    else abrir();
    engolirSoltura(e);
    return 1;
  }

  if (!is_open && notice_visible) {
    if (e->type == SDL_KEYDOWN) { fecharAviso(); engolirSoltura(e); return 1; }
    return ehTecla;
  }

  if (!is_open) return 0;

  // Panel is modal: no key reaches background screen.
  if (e->type == SDL_KEYDOWN) {
    SDL_Keycode k = e->key.keysym.sym;
    if (ehVoltar(e)) { is_open = 0; engolirSoltura(e); return 1; }
    if (semFonte || !nLin) {
      if (ehOk(k)) { recarregar(); engolirSoltura(e); }
      return 1;
    }
    if (segEd) {
      if (k == SDLK_LEFT) trocarArea(-1);
      else if (k == SDLK_RIGHT) trocarArea(1);
      else if (ehOk(k) || k == SDLK_DOWN) { segEd = 0; engolirSoltura(e); }
      return 1;
    }
    if (temEtapas()) {
      int n = etapasN();
      if (k == SDLK_UP) { if (focoEtapa > 0) focoEtapa--; else segEd = 1; }
      else if (k == SDLK_DOWN) { if (focoEtapa < n - 1) focoEtapa++; }
      else if (k == SDLK_LEFT) { segEd = 1; trocarArea(-1); }
      else if (k == SDLK_RIGHT) { segEd = 1; trocarArea(1); }
      return 1;
    }
    if (!pausado) {
      if (k == SDLK_UP || ehOk(k)) { if (nVis) pausar(nVis - 1); if (ehOk(k)) engolirSoltura(e); }
      else if (k == SDLK_LEFT) { segEd = 1; trocarArea(-1); }
      else if (k == SDLK_RIGHT) { segEd = 1; trocarArea(1); }
      return 1;
    }
    // Paused: ↑ ↓ move lines, ← → move pages, OK opens/closes detail,
    // and descending past end resumes following.
    if (k == SDLK_UP) { if (foco > 0) foco--; else { segEd = 1; } }
    else if (k == SDLK_DOWN) { if (foco < nVis - 1) foco++; else seguir(); }
    else if (k == SDLK_LEFT || k == SDLK_PAGEUP) { foco -= 16; if (foco < 0) foco = 0; }
    else if (k == SDLK_RIGHT || k == SDLK_PAGEDOWN) { foco += 16; if (foco >= nVis) seguir(); }
    else if (ehOk(k)) { detalhe = !detalhe; engolirSoltura(e); }
    return 1;
  }
  return ehTecla;
}

#include "log_island.inc"

static void log_draw_internal(void);
void log_draw(void) {
  ESCALA_INI();
  log_draw_internal();
  ESCALA_FIM();
}
static void log_draw_internal(void) {
  if (!is_open && !notice_visible) return;
  gfx_sem_recorte();
  if (is_open) { passoAoVivo(); desenhaPainel(); }
  else desenhaAviso();
}

#ifdef REGISTRO_TESTE
void log_test_text(const char *t) { regTesteTexto = t; regTesteSemFonte = 0; }
void log_test_no_source(int s) { regTesteSemFonte = s; }
void log_test_state(int a, int ed, int pausa, int focoLinha, int det, int nov) {
  area = a; segEd = ed; focoEtapa = 0; filtrar();
  if (pausa) { pausado = 1; foco = focoLinha < 0 ? nVis + focoLinha : focoLinha; detalhe = det; novas = nov; }
  else { pausado = 0; foco = -1; detalhe = 0; novas = 0; }
}
int log_test_find(const char *trecho, int ultima) {
  int i, r = -1;
  for (i = 0; i < nVis; i++) if (strstr(lin[vis[i]].t, trecho)) { r = i; if (!ultima) break; }
  return r;
}
void log_test_notice(int a) { notice_visible = a; }
void log_test_stage_focus(int f) { focoEtapa = f; }

void registro_teste_texto(const char *t) { log_test_text(t); }
void registro_teste_sem_fonte(int s) { log_test_no_source(s); }
void registro_teste_estado(int a, int ed, int pausa, int focoLinha, int det, int nov) {
  log_test_state(a, ed, pausa, focoLinha, det, nov);
}
int  registro_teste_achar(const char *trecho, int ultima) { return log_test_find(trecho, ultima); }
void registro_teste_aviso(int a) { log_test_notice(a); }
void registro_teste_foco_etapa(int f) { log_test_stage_focus(f); }
#endif

// Backward-compatibility shims
const char *registro_arquivo(void) { return log_file_path(); }
int  registro_evento(const SDL_Event *e) { return log_event(e); }
void registro_desenhar(void) { log_draw(); }
void registro_aviso_primeira_vez(void) { log_first_time_notice(); }
int  registro_aberto(void) { return log_is_open(); }
void registro_abrir(void) { log_open(); }
int  registro_ja_aberto(void) { return log_already_opened(); }
