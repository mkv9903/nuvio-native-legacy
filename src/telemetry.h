// CONSENT CARD FOR AUTOMATIC LOG UPLOAD (Tizen).
//
// The owner (20/09/2026): "we have to get all logs to resolve Samsung".
// The app never sends logs without user knowledge (avisos.h): here user is
// asked ONCE, on first opening of this version, and answer sets
// setting "Send logs automatically" (About), which can be changed later. OK =
// yes, Back = not now. Only exists on Tizen target where data is missing.
#ifndef NV_TELEMETRY_H
#define NV_TELEMETRY_H
#include <SDL2/SDL.h>

void telemetry_first_time(void);
int  telemetry_is_open(void);
void telemetry_event(const SDL_Event *e);
void telemetry_update(float dt, Uint32 now);
void telemetry_draw(Uint32 now);

#ifdef TELEMETRIA_TESTE
void telemetry_test_open(void);
#endif

// Backward-compatibility shims
void telemetria_primeira_vez(void);
int  telemetria_aberto(void);
void telemetria_evento(const SDL_Event *e);
void telemetria_atualizar(float dt, Uint32 agora);
void telemetria_desenhar(Uint32 agora);

#ifdef TELEMETRIA_TESTE
void telemetria_teste_abrir(void);
#endif

#endif
