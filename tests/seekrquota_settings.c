// Compile the real Settings module, retain only the Seekr paths at link time.
#define NV_SEEKR_API_KEY "package-test-key"
#define dados_gravar seekrTesteGravar
#define dados_apagar seekrTesteApagar
#include "../src/ajustes.c"
#undef dados_gravar
#undef dados_apagar
#include <assert.h>
int dados_gravar(const char *, const char *);
int dados_apagar(const char *);
static int falharChave;
int seekrTesteGravar(const char *nome, const char *texto) {
  return falharChave && !strcmp(nome, "seekr.txt") ? 0 : dados_gravar(nome, texto);
}
int seekrTesteApagar(const char *nome) {
  return falharChave && !strcmp(nome, "seekr.txt") ? 0 : dados_apagar(nome);
}
static char efetiva[96];
void seekr_definir_chave(const char *c) { snprintf(efetiva, sizeof efetiva, "%s", c); }
int seekr_validar(const char *c) { return !strcmp(c, efetiva); }
const char *i18n(const char *s) { return s; }
int recomenda_ativo(void) { return 0; }
static SeekrUso uso = {.usadas=12, .restantes=38, .limite=50, .persistente=1, .reinicioUtc=2000073600};
static int estadoSeekr = SEEKR_PRONTO;
void seekr_uso(SeekrUso *u) { *u = uso; }
int seekr_estado(void) { return estadoSeekr; }
const char *seekr_estado_rotulo(int e) {
  return e == SEEKR_ARMAZENAMENTO_INDISPONIVEL ? "Não foi possível guardar o uso do Seekr" :
         e == SEEKR_LIMITE_PROVEDOR ? "Limite temporário do Seekr" : "Miniaturas disponíveis";
}
int seekr_horario_local(long long t, char *saida, size_t n) {
  if (!t) return 0;
  snprintf(saida, n, "%s", "19/05 21:00"); return 1;
}
int main(void) {
  dados_iniciar("/tmp");
  seekrCarregar();
  assert(!strcmp(efetiva, NV_SEEKR_API_KEY));
  assert(!strstr(seekrMascarada(), NV_SEEKR_API_KEY));
  valor[AJ_AVANCADAS] = 0;   /* advanced options shown (global toggle on) */
  int visiveis = 0;
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && (TELA[i].op == AJ_SEEKR_CHAVE || TELA[i].op == AJ_SEEKR_TESTAR))
      visiveis += visivel(i);
  assert(visiveis == 2); // A package key never hides the personal controls.
  seekrDefinir("personal-test-key"); assert(!strcmp(efetiva, "personal-test-key"));
  assert(!strstr(seekrMascarada(), "personal-test-key"));
  falharChave = 1;
  seekrDefinir("replacement-test-key");
  assert(seekrSalvarFalhou && !strcmp(efetiva, "personal-test-key"));
  char *persistida = dados_ler("seekr.txt");
  assert(persistida && !strcmp(persistida, "personal-test-key")); free(persistida);
  assert(strstr(seekrAjuda(AJ_SEEKR_CHAVE), "chave anterior foi mantida"));
  seekrDefinir(""); assert(seekrSalvarFalhou && !strcmp(efetiva, "personal-test-key"));
  falharChave = 0;
  seekrDefinir("personal-test-key"); assert(!seekrSalvarFalhou);
  seekrCarregar(); assert(!strcmp(efetiva, "personal-test-key"));
  skTesteIniciar(); pthread_join(skFio, NULL); skFioVivo = 0;
  assert(skTesteRes == 1 && !strcmp(skTesteChave, "personal-test-key"));
  seekrDefinir("new-personal-test-key");
  assert(!strcmp(skTesteTexto(), "OK testa")); // A previous key's result is never reused.
  seekrDefinir("personal-test-key");
  assert(strstr(textoLeitura(AJ_SEEKR_TESTAR), "12/50"));
  assert(strstr(seekrAjuda(AJ_SEEKR_LIGADO), "12 de 50 consultas hoje (UTC)"));
  assert(strstr(seekrAjuda(AJ_SEEKR_LIGADO), "19/05 21:00 (hora local)"));
  estadoSeekr = SEEKR_LIMITE_PROVEDOR; uso.retryUtc = 2000073600;
  assert(strstr(seekrAjuda(AJ_SEEKR_TESTAR), "Tente após"));
  uso.relogioAtrasado = 1;
  assert(strstr(seekrAjuda(AJ_SEEKR_TESTAR), "contador não foi reiniciado"));
  assert(!strstr(seekrAjuda(AJ_SEEKR_TESTAR), "hora local"));
  uso.persistente = 0;
  assert(!strstr(textoLeitura(AJ_SEEKR_TESTAR), "12/50"));
  assert(strstr(seekrAjuda(AJ_SEEKR_TESTAR), "Uso do Seekr indisponível"));
  assert(!strstr(seekrAjuda(AJ_SEEKR_TESTAR), "personal-test-key"));
  seekrDefinir(""); assert(!strcmp(efetiva, NV_SEEKR_API_KEY));
  assert(!dados_ler("seekr.txt"));
  seekrCarregar(); assert(!strcmp(efetiva, NV_SEEKR_API_KEY));
  puts("seekrquota settings: PASS visible controls, personal priority, mask, persistence, validation and removal");
  puts("seekrquota settings: PASS key write/delete failures, UTC usage, local reset/retry, clock and storage states");
  return 0;
}
