// Local app-logo selection in Settings > Appearance > Logo and opening.
// Novo uses the gradient play mark; Classico uses the original retro TV.
// An alternative app icon takes priority on entry screens (logoapp_marca).
// The installed LG package icon stays fixed.
#ifndef NV_LOGOAPP_H
#define NV_LOGOAPP_H
#include "gfx.h"

enum { LOGO_NOVO = 0, LOGO_CLASSICO = 1, LOGO_N = 2 };
// Qual arquivo de cada logo.
enum {
  LOGO_F_SIMBOLO,     // so o simbolo (miniatura, cantos das telas de entrada)
  LOGO_F_MARCA,       // simbolo + nome empilhados (abertura)
  LOGO_F_HORIZONTAL   // simbolo + nome lado a lado (Sobre)
};

void logoapp_iniciar(const char *dirArte);
// O logo EM VIGOR (ajustes_logo_app()).
int  logoapp_atual(void);
// Nome na lista de valores ("Novo", "Clássico"); "" fora da faixa.
// Caminho absoluto da arte (buffer proprio por logo/forma).
const char *logoapp_caminho(int logo, int forma);
// Miniatura quadrada do logo em `r` (o seletor de 64 px). `ladrilho` = 1 pinta o
// mesmo fundo escuro arredondado da galeria de icones. 0 enquanto a textura nao chegou.
int  logoapp_miniatura(int logo, GfxRect r, int ladrilho, float alpha);
// A marca propria nas telas de entrada (login, perfis, "preparando"): o icone
// alternativo, se houver; senao o simbolo do logo Novo; com o Classico
// nada (como era). Devolve 1 se desenhou.
int  logoapp_marca(GfxRect r, float alpha);
#endif
