#include "support/profile_context.h"
// Legacy tab preferences must preserve Saved/Notices and skip deleted IDs.
#include "../src/salvospainel.c"
#include <assert.h>

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char *saved;
  assert(dir && strstr(dir, "nuvio-spainel-tabs-"));
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  assert(perfis_ativo() == 1);
  assert(dados_gravar("salvos-abas-p1.txt",
    "aba\t4\t1\naba\t1\t1\naba\t2\t1\naba\t3\t0\naba\t0\t1\n"));
  abasCarregar();
  aba = primeiraAba();
  assert(aba == SP_ABA_SALVOS && !abaLigada(SP_ABA_AVISOS));
  assert(proximaAba(aba, 1) == aba && proximaAba(aba, -1) == aba);
  assert(!abaExiste(1) && !abaExiste(2) && !abaExiste(4));
  abasGravar();
  saved = dados_ler("salvos-abas-p1.txt");
  assert(saved && strstr(saved, "aba\t3\t0\n") && strstr(saved, "aba\t0\t1\n"));
  assert(!strstr(saved, "aba\t1\t") && !strstr(saved, "aba\t2\t") && !strstr(saved, "aba\t4\t"));
  free(saved);

  test_select_profile(2);
  assert(dados_gravar("salvos-abas-p2.txt", "aba\t3\t1\naba\t4\t1\naba\t0\t1\n"));
  abasCarregar();
  aba = primeiraAba();
  assert(aba == SP_ABA_AVISOS && proximaAba(aba, 1) == SP_ABA_SALVOS);
  assert(proximaAba(SP_ABA_SALVOS, -1) == SP_ABA_AVISOS);
  test_select_profile(1);
  abasCarregar();
  assert(primeiraAba() == SP_ABA_SALVOS && !abaLigada(SP_ABA_AVISOS));
  puts("salvospainel tabs: ok");
  return 0;
}
