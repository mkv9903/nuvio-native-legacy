// Icone do app para apoiadores. Ver iconeapp.h.
#include "iconeapp.h"
#include "ajustes.h"
#include "dados.h"
#include "tex_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A ORDEM E A DA GALERIA e o INDICE gravado em ajustes.txt (iconeAppLocal):
// so acrescentar no fim. Bate com V_ICONE_APP (ajustes.c), com LISTA em
// tools/icones-app.sh.
static const char *ID[ICONEAPP_N] = {
  "original", "fenix", "nverde", "tvlaranja", "npixel",
  "tricolor", "arco", "tvviva", "cluberetro", "arcaden",
};

static char dirArteIc[512] = ".";
static char caminhos[ICONEAPP_N][600];
static int  apoiadorLido = -1;

int apoiador_ativo(void) {
  const char *e;
  char *s;
  if (apoiadorLido >= 0) return apoiadorLido;
  // O PORTAO DE VERDADE (Patreon, conta) entra AQUI e so aqui. Hoje: ninguem,
  // a nao ser forcado pelo dono para ver a galeria.
  e = getenv("NUVIO_APOIADOR");
  if (e && e[0] == '1') return apoiadorLido = 1;
  // Sem pasta de dados ainda, nao guarda a resposta: perguntar de novo depois.
  if (!dados_dir() || !dados_dir()[0]) return 0;
  s = dados_ler("apoiador.txt");
  apoiadorLido = (s && s[0] == '1');
  free(s);
  if (apoiadorLido) { printf("[app-icon] supporter enabled through local override\n"); fflush(stdout); }
  return apoiadorLido;
}
void apoiador_reler(void) { apoiadorLido = -1; }

void iconeapp_iniciar(const char *dirArte) {
  snprintf(dirArteIc, sizeof dirArteIc, "%s", dirArte && dirArte[0] ? dirArte : ".");
  memset(caminhos, 0, sizeof caminhos);
}

int iconeapp_atual(void) {
  int v = ajustes_icone_app();
  if (!apoiador_ativo() || v < 0 || v >= ICONEAPP_N) return 0;
  return v;
}

const char *iconeapp_id(int i) {
  return (i >= 0 && i < ICONEAPP_N) ? ID[i] : "";
}

const char *iconeapp_caminho(int i) {
  // Um buffer por icone: a galeria pede os dez no mesmo quadro e o tex_cache
  // guarda o ponteiro so durante a chamada, mas assim ninguem depende disso.
  if (i < 0 || i >= ICONEAPP_N) return "";
  if (!caminhos[i][0]) snprintf(caminhos[i], sizeof caminhos[i], "%s/icones-app/%s.png", dirArteIc, ID[i]);
  return caminhos[i];
}

int iconeapp_desenhar(int i, GfxRect r, int ladrilho, float alpha) {
  GLuint t;
  if (i < 0 || i >= ICONEAPP_N || alpha <= 0.004f) return 0;
  // O mesmo fundo do icone original (13,16,30) e o mesmo raio de ladrilho que a
  // TV da a um icone quadrado na grade de apps.
  if (ladrilho) gfx_cor(r, 0.22f, 13.0f/255.0f, 16.0f/255.0f, 30.0f/255.0f, alpha);
  t = tex_obter_larg(iconeapp_caminho(i), r.w);
  if (!t) return 0;
  // GFX_TEXTO: RGB e ALPHA da textura (a marca e recortada); o GFX_SNAP
  // ignoraria o alpha e pintaria o quadrado inteiro.
  gfx_rect(r, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, alpha);
  return 1;
}

int iconeapp_marca(GfxRect r, float alpha) {
  int i = iconeapp_atual();
  if (i == 0) return 0;
  return iconeapp_desenhar(i, r, 0, alpha);
}
