// Plugins Nuvio (F09): ligar/desligar, os repositorios (da conta ou digitados)
// com os scrapers de cada um, e adicionar/remover repositorio. Ver plugins.h.
// O desenho mora em ajustes_ux_ilha.inc (ajustes_desenhar_plugins), com as
// mesmas pecas de vidro da lista de addons.
#ifndef NV_PLUGINSUI_H
#define NV_PLUGINSUI_H
#include <SDL2/SDL.h>

void pluginsui_abrir(void);
void pluginsui_evento(const SDL_Event *e);
void pluginsui_atualizar(float dt, Uint32 agora);
void pluginsui_desenhar(Uint32 agora);
int  pluginsui_quer_sair(void);
#ifdef AJUSTES_TESTE
#endif

#endif
