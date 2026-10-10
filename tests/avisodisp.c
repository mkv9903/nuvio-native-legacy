// Avisos dispensados (avisodisp.h): persiste, e por perfil, a chave e o evento
// (a versao seguinte ainda avisa) e o conjunto da sessao nao vai a disco.
// Sem janela e sem rede; escreve so em NUVIO_DADOS (pasta temporaria).
//
//   bash tests/avisodisp.sh
#include "avisodisp.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// O perfil ativo e do teste: o modulo so pergunta por perfis_ativo().
static int perfilTeste = 1;
int perfis_ativo(void) { return perfilTeste; }

int main(void) {
  const char *d = getenv("NUVIO_DADOS");
  char *s;
  int i;
  assert(d && strstr(d, "nuvio-avisodisp"));   // nunca na pasta de dados de verdade
  dados_iniciar(d);

  // 1. Nada dispensado; dispensar grava e repetir nao duplica.
  assert(!avisodisp_tem("update:1.8.1"));
  avisodisp_por("update:1.8.1");
  avisodisp_por("update:1.8.1");
  assert(avisodisp_tem("update:1.8.1"));
  s = dados_ler(AVD_ARQ);
  assert(s && !strcmp(s, "1 update:1.8.1\n"));
  free(s);

  // 2. Evento novo (outra versao) ainda avisa.
  assert(!avisodisp_tem("update:1.8.2"));

  // 3. Persiste: esquecida a RAM (o app reiniciou), o arquivo traz de volta.
  avisodisp_por("crash:2026-09-19 18:57");          // chave com espaco
  avisodisp_esquecer();
  assert(avisodisp_tem("update:1.8.1"));
  assert(avisodisp_tem("crash:2026-09-19 18:57"));

  // 4. Por perfil: o perfil 2 nao herda o que o 1 dispensou, e vice-versa.
  perfilTeste = 2;
  assert(!avisodisp_tem("update:1.8.1"));
  avisodisp_por("canal:profile2");
  assert(avisodisp_tem("canal:profile2") && !avisodisp_tem("update:1.8.1"));
  perfilTeste = 1;
  assert(!avisodisp_tem("canal:profile2"));
  avisodisp_esquecer();
  perfilTeste = 2;
  assert(avisodisp_tem("canal:profile2"));
  perfilTeste = 1;

  // 5. Sessao: so RAM, por perfil, e some no reinicio.
  assert(!avisodisp_sessao_tem("canal:test"));
  avisodisp_sessao_por("canal:test");
  assert(avisodisp_sessao_tem("canal:test"));
  assert(!avisodisp_tem("canal:test"));   // anunciar nao e dispensar
  perfilTeste = 2;
  assert(!avisodisp_sessao_tem("canal:test"));
  perfilTeste = 1;
  avisodisp_esquecer();
  assert(!avisodisp_sessao_tem("canal:test"));

  // 6. Teto: a mais velha sai primeiro, o arquivo nao cresce sem fim.
  for (i = 0; i < AVD_MAX + 5; i++) { char k[32]; snprintf(k, sizeof k, "canal:%d", i); avisodisp_por(k); }
  assert(!avisodisp_tem("update:1.8.1"));   // a primeira caiu
  assert(avisodisp_tem("canal:204"));
  avisodisp_esquecer();
  assert(avisodisp_tem("canal:204") && !avisodisp_tem("canal:0"));

  // 7. Chave vazia ou NULL nao grava nada.
  avisodisp_por(""); avisodisp_por(NULL);
  assert(!avisodisp_tem("") && !avisodisp_tem(NULL));

  printf("avisodisp: ok\n");
  return 0;
}
