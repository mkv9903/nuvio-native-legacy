#include "addonsui.h"
#include "idioma.h"
#include "descoberta.h"
#include "addons.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "anim.h"
#include "sync.h"
#include "ajustes.h"
#include <stdio.h>
#include <string.h>

#define LINHA_H   96.0f
#define LINHA_GAP  8.0f
#define LISTA_X   ajustes_conteudo_x()
#define LISTA_W  1120.0f
#define TOPO     (NV_MARGEM_Y + 118.0f)
#define BASE     (NV_TELA_H - NV_MARGEM_Y - 48.0f)
#define RAIO       0.13f

static int   foco, sair;
// 1 quando alguem ligou ou desligou algum addon nesta visita. Ver a nota em
// addonsui_quer_sair.
static int   mexeu;
static float scrollY;
static float animFoco[64];

void addonsui_abrir(void) {
  foco = 0; sair = 0; mexeu = 0; scrollY = 0.0f;
  // Sonda aqui e nao no arranque: sao N viagens de rede, e elas so interessam
  // a quem abriu esta tela. Quem ja respondeu nao e consultado de novo.
  addons_sondar_manifestos();
}

// LIGAR UM ADDON SO VALIA NA PROXIMA ABERTURA DO APP, e o relato foi esse:
// "se eu tiver com os addons desativados e ativar, eles nao ativam; tenho que
// fechar o app e abrir de novo".
//
// addons_alternar muda a lista em memoria e sync_sujar_addons so marca que ela
// precisa SUBIR para a conta — nenhum dos dois refaz a descoberta. Sem um ciclo
// novo, os manifestos nao sao lidos de novo e a home continua com os catalogos
// do conjunto antigo. desc_repetir() e quem refaz o ciclo inteiro, que e o que
// esta troca exige de verdade: ler manifesto de addon que acabou de entrar nao
// e reordenar fileira.
//
// NA SAIDA DA TELA, e nao a cada tecla: quem liga cinco addons de uma vez paga
// um ciclo e nao cinco. O ciclo leva ~20 s nesta TV, entao a diferenca nao e
// teorica.
int addonsui_quer_sair(void) {
  if (sair && mexeu) { mexeu = 0; desc_tirar_fileiras_de_addons_ausentes(); desc_repetir(); }
  return sair;
}

void addonsui_evento(const SDL_Event *e) {
  int n = addons_n();
  if (e->type != SDL_KEYDOWN) return;
  switch (e->key.keysym.sym) {
    case SDLK_UP:     if (foco > 0) foco--; break;
    case SDLK_DOWN:   if (foco < n - 1) foco++; break;
    case SDLK_RETURN:
      if (n > 0) {
        addons_alternar(foco);
        mexeu = 1;
        // Sobe para a conta: desligar um addon aqui e desliga-lo no celular
        // tambem, que e o que a pessoa espera de uma conta sincronizada.
        sync_sujar_addons();
      }
      break;
    case SDLK_AC_BACK: case SDLK_LEFT: sair = 1; break;
    default: break;
  }
}

void addonsui_atualizar(float dt, Uint32 agora) {
  int i, n = addons_n();
  float alvo;
  (void)agora;
  for (i = 0; i < n && i < 64; i++)
    animFoco[i] = anim_mola(animFoco[i], i == foco ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  // Rolagem que mantem a linha focada dentro da area util, como em Ajustes.
  alvo = (float)foco * (LINHA_H + LINHA_GAP);
  if (alvo - scrollY > BASE - TOPO - LINHA_H) scrollY = alvo - (BASE - TOPO - LINHA_H);
  if (alvo - scrollY < 0.0f) scrollY = alvo;
}

// "catalogo · fontes · legendas" com o que o manifesto respondeu. Enquanto a
// sonda nao voltou, dizer "nao fornece" seria afirmar o que nao se sabe.

// A TELA DE ADDONS NO GLASS UI (mockup de Ajustes, quadro "addons"): o mesmo
// arranjo de ilhas de Ajustes, desenhado por ajustes.c (que tem as pecas).
void addonsui_desenhar(Uint32 agora) {
  (void)agora;
  ajustes_desenhar_addons(foco);
}

#ifdef AJUSTES_TESTE
void addonsui_teste_foco(int f) { foco = f; }
#endif
