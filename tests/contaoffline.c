/* Retained C collection/library/history sync survives account outages.
 * Addon cache, edits, count/URL limits and restart cases now run in Rust.
 * This fixture verifies only capabilities still implemented in C. */
#include "sync.h"
#include "addons.h"
#include "catordem.h"
#include "contacache.h"
#include "traktauth.h"
#include "perfis.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ------------------------------------------------------------ disco dublado

static const char *pasta(void) {
  const char *d = getenv("NV_T_DIR");
  return d && *d ? d : "/tmp";
}
static void caminho(char *dst, size_t tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", pasta(), nome);
}
const char *dados_dir(void) { return pasta(); }
void dados_uuid(char *dst, unsigned tam) {
  static unsigned seq;
  snprintf(dst, tam, "%08x-0000-4000-8000-%012x", (unsigned)getpid(), ++seq);
}
char *dados_ler(const char *nome) {
  char c[600];
  FILE *f;
  long n;
  char *b;
  caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  n = (long)fread(b, 1, (size_t)n, f);
  b[n] = 0;
  fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[600];
  FILE *f;
  caminho(c, sizeof c, nome);
  f = fopen(c, "wb");
  if (!f) return 0;
  fputs(conteudo ? conteudo : "", f);
  fclose(f);
  return 1;
}
int dados_apagar(const char *nome) {
  char c[600];
  caminho(c, sizeof c, nome);
  return remove(c) == 0;
}
const char *dados_cliente_id(void) { return "tv-teste"; }

// ------------------------------------------------------------ perfis dublados

int  perfis_ativo(void)          { return 1; }
int  perfis_ativo_addons(void)   { return 1; }
int  perfis_n(void)              { return 1; }
int  perfis_precisa_escolher(void) { return 0; }
const char *perfis_dono(void)    { return "dono-a"; }
void perfis_esquecer(void)       { }
const ContaPerfil *perfis_item(int i) { (void)i; return NULL; }
const char *i18n(const char *s) { return s; }
int ajustes_hist_conta(void) { return 1; }
void ajustes_perfil_guardar(int perfil) { (void)perfil; }
int  ajustes_perfil_restaurar(int perfil) { (void)perfil; return 0; }
void ajustes_perfil_esquecer(void) { }
void arteesc_esquecer(void) { }
void sorg_esquecer(void) { }
int  perfis_puxar(void) { return 1; }

// ------------------------------------------------------------ servidor dublado
// `modo`: 200 responde; 504/429/502 falha com esse HTTP; 0 = sem resposta.

static int modo = 200;
// "parcial": a tabela de addons responde, as RPCs dao 429 (so a ordem e as
// so-leitura caem — caem uma a uma, cada uma para a sua copia).
static int rpcModo = -1;
static const char *usuario = "conta-a";
static int rpcs, addon_tables;

static char *falha(int *st) {
  *st = modo;
  if (!modo) return NULL;
  return strdup(modo == 429 ? "{\"message\":\"Too Many Requests\"}" : "error code: 504");
}
char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  (void)corpo;
  rpcs++;
  if (rpcModo >= 0 && rpcModo != 200) { int m = modo; char *r; modo = rpcModo; r = falha(st); modo = m; return r; }
  if (modo != 200) return falha(st);
  *st = 200;
  if (!strcmp(funcao, "sync_pull_collections")) return strdup("[{\"marca\":\"colecao-conta\"}]");
  if (!strcmp(funcao, "sync_pull_library"))     return strdup("[{\"marca\":\"bib-conta\"}]");
  if (!strcmp(funcao, "sync_pull_watched_items")) return strdup("[{\"marca\":\"visto-conta\"}]");
  return strdup("[]");
}
char *sessao_tabela(const char *t, const char *q, int *st) {
  (void)q;
  if (!strcmp(t, "addons")) addon_tables++;
  if (modo != 200) return falha(st);
  *st = 200;
  (void)t; return strdup("[]");
}
int  sessao_logada(void)          { return 1; }
const char *sessao_usuario(void)  { return usuario; }
int  nuvem_freio_ativo(void)      { return 0; }
int  nuvem_erro_ausente(const char *c) { (void)c; return 0; }
const char *nuvem_trakt_cliente(void) { return ""; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }

// ------------------------------------------------------------ o app dublado

int addons_n(void) { return 0; }
void fontecache_vod_limpar(void) {}
static int colConta, bibConta, vistoConta;
int  col_definir_json(const char *j) { if (j && strstr(j, "colecao-conta")) colConta++; return 0; }
int  contalib_ler_biblioteca(const char *j) { if (j && strstr(j, "bib-conta")) bibConta++; return 0; }
int  contalib_ler_vistos(const char *j) { if (j && strstr(j, "visto-conta")) vistoConta++; return 0; }
void desc_remontar_fileiras(void) {}
// 1.8 sync.c also talks to plugins.c, colfileiras.c and colecoes.c revision/validation;
// none of them is under test here, so they are inert: no plugin edit pending, no
// collection-row revision change.
#include "plugins.h"
#include "colecoes.h"
#include "colfileiras.h"
void plugins_retrato(PlugRetrato *s) { memset(s, 0, sizeof *s); }
int  plugins_confirmar(unsigned rev, unsigned g) { (void)rev; (void)g; return 1; }
int  plugins_definir_da_conta(const PlugRepo *l, int n, unsigned g) { (void)l; (void)n; (void)g; return 0; }
int  plugins_ler_conta(const char *j, PlugRepo *o, int max) { (void)j; (void)o; (void)max; return 0; }
void plugins_perfil_mudou(void) {}
void plugins_esquecer(void) {}
int  col_resposta_valida(const char *j) { return j && *j; }
unsigned col_revisao(void) { return 0; }
void colfileiras_sincronizar(void) {}
int  colfileiras_receber(const char *j) { return col_definir_json(j); }   // real one forwards the blob to colecoes.c
void colfileiras_contexto(void) {}
void desc_repetir_silencioso(void) {}
void desc_repetir(void) {}
void desc_repetir_addons(void) {}
void desc_refazer_continuar(void) {}
void desc_loc_apagar(void) {}
void desc_esquecer(void) {}
void desc_tmdb_definir(const char *c) { (void)c; }

// ------------------------------------------------------------ o resto, mudo

void releaseinfo_forget(void) {}
void lembrete_esquecer_todos(void) {}
int  ajustes_aplicar_blob(const char *j) { (void)j; return 0; }
void ajustes_definir_ocultar_nao_lancados(int l) { (void)l; }
int  ajustes_mesclar_blob(const char *b, char **s) { (void)b; *s = NULL; return 0; }
void ajustes_tmdb_idioma_relatar(const char *b) { (void)b; }
void buscasrec_esquecer(void) {}
void cachearte_limpar_referencias(void) {}
int  cat_apagar_cache(void) { return 0; }
int  contalib_aplicar_catalogo(void) { return 0; }
int  contalib_aplicar_vistos(void) { return 0; }
void contalib_esquecer(void) {}
unsigned contalib_vistos_revisao(void) { return 0; }   // #199
int  simkl_ativo(void) { return 0; }
void contalib_reconciliar(void) {}
void debrid_definir_chave(const char *s, const char *c) { (void)s; (void)c; }
void debrid_esquecer(void) {}
void extras_definir_chave(const char *c) { (void)c; }
void fontepref_esquecer(void) {}
void homeestado_esquecer(void) {}
void prog_esquecer_tudo(void) {}
void perfilcont_esquecer(void) {}
void psparede_esquecer(void) {}   // sync.c 2.0 chama no logout; faltava aqui e o teste nao ligava
void recomenda_esquecer(void) {}
void salvos_esquecer(void) {}
void stalker_esquecer(void) {}
int  syncprog_aplicar(int *c) { (void)c; return 0; }
int  syncprog_empurrar(void) { return 0; }
void syncprog_esquecer(void) {}
int  syncprog_puxadas(void) { return 0; }
int  syncprog_puxar(void) { return 0; }
int  trakt_ativo(void) { return 0; }
int  trakt_definir(const char *t, const char *c) { (void)t; (void)c; return 0; }
int  trakt_credencial_igual(const char *t, const char *c) { (void)t; (void)c; return 0; }
void trakt_esquecer(void) {}
TraEstado traktauth_estado(void) { return TRA_LIGADO; }
void vistoep_esquecer(void) {}
void xtream_esquecer(void) {}

// ------------------------------------------------------------ roteiro

static int falhas;
static unsigned agora = 1000;
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static void ateTerminar(void) {
  int i;
  for (i = 0; i < 4000 && sync_estado() == SYNC_RODANDO; i++) { sync_passo(agora += 16); usleep(1000); }
  sync_passo(agora += 16);
  sync_passo(agora += 16);
}
static int existe(const char *nome) {
  char *b = dados_ler(nome);
  int ok = b != NULL;
  free(b);
  return ok;
}

// argv: <modo HTTP> [<conta>] [volta|sair]
//   volta = depois do ciclo, o servidor volta a 200 e sync_periodico tem de
//           partir em SYNC_RETENTA_MS;
//   sair  = depois do ciclo, sync_esquecer_usuario (logout).
// argv[2] = "-" mantem a conta padrao. O que se espera vem do .sh.
int main(int argc, char **argv) {
  const char *extra = argc > 3 ? argv[3] : "";
  int copia;
  setvbuf(stdout, NULL, _IOLBF, 0);
  modo = argc > 1 ? atoi(argv[1]) : 200;
  if (argc > 1 && !strcmp(argv[1], "parcial")) { modo = 200; rpcModo = 429; }
  if (argc > 2 && strcmp(argv[2], "-")) usuario = argv[2];
  printf("-- sessao: servidor %d, conta %s%s%s\n", modo, usuario, *extra ? ", " : "", extra);
  copia = existe("conta-colecoes-p1.json");
  sync_iniciar();
  ateTerminar();

  if (modo == 200 && rpcModo != 429) {
    confere("server responded", sync_servidor_fora() == 0 && !sync_usando_copia());
    confere("collections, library and watched cache saved",
            existe("conta-colecoes-p1.json") && existe("conta-biblioteca-p1.json") && existe("conta-vistos-p1.json"));
    { char *c = contacache_ler(CC_COLECOES, 1, usuario, NULL);
      char *other = contacache_ler(CC_COLECOES, 1, "other-account", NULL);
      confere("cached collections belong to the correct account", c && !other);
      free(c); free(other); }
  } else {
    int expected = rpcModo == 429 ? 429 : modo ? modo : -1;
    confere("outage status retains HTTP cause", sync_servidor_fora() == expected);
    confere("retained capabilities make their own independent requests", rpcs > 0);
    if (copia && !strcmp(usuario, "conta-a")) {
      confere("cached collections, library and watched items applied", colConta && bibConta && vistoConta);
      confere("cache-use status and timestamp published", sync_usando_copia() && sync_copia_quando() > 0);
    } else {
      confere("no content invented without this account's cache", !colConta && !bibConta && !vistoConta);
      confere("no cache-use status without a valid cache", !sync_usando_copia());
    }
  }
  confere("C sync never fetches the Rust-owned addon table", addon_tables == 0);

  if (!strcmp(extra, "volta")) {
    int r;
    modo = 200;
    r = sync_periodico(agora + 1000);
    confere("antes de SYNC_RETENTA_MS nao repete", r == 0);
    r = sync_periodico(agora + SYNC_RETENTA_MS + 1);
    confere("servidor fora: volta seguinte em SYNC_RETENTA_MS, nao em 5 min", r == 1);
    ateTerminar();
    confere("o servidor voltou: estado volta a respondeu",
            sync_servidor_fora() == 0 && !sync_usando_copia());
    confere("fresh retained content applied after recovery", colConta && bibConta && vistoConta);
  }
  if (!strcmp(extra, "sair")) {
    sync_esquecer_usuario();
    confere("logout apaga as copias da conta",
            !existe("conta-colecoes-p1.json") &&
            !existe("conta-biblioteca-p1.json") && !existe("conta-vistos-p1.json"));
    confere("logout zera o estado de servidor fora", sync_servidor_fora() == 0);
  }
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}

void cat_historico_contexto(const char *u, int p) { (void)u; (void)p; }
