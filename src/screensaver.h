// SCREENSAVER (2.0): what appears when screen remains idle, on top of
// black fade (esmaecer.h decides WHEN; this module decides WHAT).
//
//   SHOWCASE a catalog title in full screen, with slow zoom, and information
//            entering one at a time: title (logo when available),
//            year/duration/genre and a line of synopsis. Every SCREENSAVER_ITEM_S
//            screen breathes in dark and transitions. OK opens title (Home only).
//   CLOCK    large time in thin numerals, date and
//            a line that fills across the minute. The block moves slowly across
//            the screen (OLED: nothing stays on same pixel).
//
// Update requests textures and selects titles; draw only reads GLuint.
// Empty catalog (or art without backdrop) falls back to clock.
#ifndef NV_SCREENSAVER_H
#define NV_SCREENSAVER_H

#define SCREENSAVER_ITEM_S      20.0f   // how long each title stays in showcase
#define SCREENSAVER_ENTER_S      2.2f   // art emerges from black
#define SCREENSAVER_EXIT_S       1.6f   // and returns to black at end
#define SCREENSAVER_QUEUE_MAX     48    // titles drawn per screensaver session

// Showcase sources
#define SCREENSAVER_SOURCE_CATALOG 0
#define SCREENSAVER_SOURCE_LIST    1    // on list + in progress ("Continue")

// Once per frame, after esmaecer_quadro. `active` = esmaecer_descanso().
void screensaver_frame(unsigned now, float dt, int active, int style, int source);
// On top of black veil. Called by esmaecer_desenhar.
void screensaver_draw(unsigned now);
// Catalog index of title in showcase right now, or -1 (clock, between two
// titles, or title still emerging from black).
int  screensaver_onscreen_index(void);
// OK in showcase: stores request; app.c consumes and opens title.
void screensaver_request_open(void);
int  screensaver_requested_open(void);   // index or -1; CONSUMES

// Pure tests: does item fit source filter?
int  screensaver_item_valid(int has_backdrop, int has_title, int on_list, int progress, int source);
// Position along clock path (0..1 on both axes) at timestamp `s`.
void screensaver_path(float s, float *px, float *py);

// Backward-compatibility shims
#define DESC_ITEM_S          SCREENSAVER_ITEM_S
#define DESC_ENTRA_S         SCREENSAVER_ENTER_S
#define DESC_SAI_S           SCREENSAVER_EXIT_S
#define DESC_FILA_MAX        SCREENSAVER_QUEUE_MAX
#define DESC_FONTE_CATALOGO  SCREENSAVER_SOURCE_CATALOG
#define DESC_FONTE_LISTA     SCREENSAVER_SOURCE_LIST

void descanso_quadro(unsigned agora, float dt, int ativo, int estilo, int fonte);
void descanso_desenhar(unsigned agora);
int  descanso_indice_em_tela(void);
void descanso_pedir_abrir(void);
int  descanso_pedido_abrir(void);
int  descanso_item_serve(int temFundo, int temTitulo, int naLista, int progresso, int fonte);

#endif
