/* Retained C source-status, channel fallback and retry checks.
 * Addon model, storage and manifest assertions have moved to Rust tests. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "addons.h"
#include "addon_registry.h"
#include "support/addon_context.h"
#include "fontecache.h"

// Provider discovery is tested in ondever_lookup.c. This isolated addon
// transport fixture must not start an unrelated TMDB request.
void ondever_pedir(const char *id, int series, long tmdb) {
  (void)id; (void)series; (void)tmdb;
}

// ---------------------------------------------------------------- duble
//
// A BUSCA DE FONTES DE VERDADE (addons_buscar -> fio -> addons_estado), com a
// rede e o parser trocados por dubles. O que se confere e o RESUMO que a folha
// de fontes vazia usa para dizer a causa (B6/#107, D5): quem foi consultado,
// quem respondeu vazio, quem nao respondeu.
//
// resp[0] e o corpo de exemplo.test, resp[1] o de antigo.test; NULL = sem
// resposta (rede_baixar devolve NULL em falha e em 4xx).
static const char *resp[2];
static int nDebridNovaBusca;

// canal.test imita o FrostView TV medido no #112: 200 {"streams":[]} para
// /stream/tv/ e a lista de verdade para /stream/channel/. `pedidosCanal`
// guarda o tipo de cada pedido, na ordem, para conferir quem foi primeiro.
static const char *respCanalTv, *respCanalChannel;
static char pedidosCanal[200];
// lento.test (#182): as primeiras `falhasLento` requisicoes NAO respondem (o
// timeout do AIOStreams frio); as seguintes respondem com uma fonte.
static int falhasLento, chamadasLento, prazoLento[2], prazoCanal;
const char *rede_ultimo_erro(void) { return ""; }
char *rede_baixar(const char *url, int s) {
  const char *r;
  if (strstr(url, "lento.test")) {
    if (chamadasLento < 2) prazoLento[chamadasLento] = s;
    if (++chamadasLento <= falhasLento) return NULL;
    return strdup("{\"streams\":[{\"url\":\"https://x/l.mp4\"}]}");
  }
  if (strstr(url, "canal.test")) {
    prazoCanal = s;
    int tv = strstr(url, "/stream/tv/") != NULL;
    strncat(pedidosCanal, tv ? "tv," : "channel,",
            sizeof pedidosCanal - strlen(pedidosCanal) - 1);
    r = tv ? respCanalTv : respCanalChannel;
    return r ? strdup(r) : NULL;
  }
  r = strstr(url, "exemplo.test") ? resp[0]
    : strstr(url, "antigo.test")  ? resp[1] : NULL;
  return r ? strdup(r) : NULL;
}
// conta um "url" por fonte; o bastante para distinguir lista vazia de cheia
int stream_extrair(const char *json, const char *prov, Stream **saida) {
  int n = 0; const char *p = json;
  (void)prov;
  while ((p = strstr(p, "\"url\"")) != NULL) { n++; p += 5; }
  *saida = n ? calloc((size_t)n, sizeof(Stream)) : NULL;
  return n;
}
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
void stream_definir_lista(const Stream *l, int n) { (void)l; (void)n; }
void stream_definir_lista_idade(const Stream *l, int n, Uint32 idade) {
  (void)idade; stream_definir_lista(l, n); }
void stream_lista_acrescentar(const Stream *l, int n, int o) { (void)l; (void)n; (void)o; }
void stream_invalidar(const char *p) { (void)p; }
int stream_n(void) { return 0; }
int stream_lista_do_alvo(const char *id) { (void)id; return 0; }
Uint32 SDL_GetTicks(void) { return 1000; }
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
void debrid_definir_episodio(int t, int e) { (void)t; (void)e; }
void debrid_nova_busca(void) { nDebridNovaBusca++; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  snprintf(dst, tam, "%s", url ? url : ""); return dst; }
void marco(const char *s) { (void)s; }
int  fontecache_pegar(const char *id, const char *tipo, Stream **l, int *n) {
  (void)id; (void)tipo; *l = NULL; *n = 0; return FC_NADA; }
void fontecache_guardar(const char *id, const char *tipo, const Stream *l, int n) {
  (void)id; (void)tipo; (void)l; (void)n; }
void fontecache_ceder(void) {}
void fontecache_encerrar(void) {}
void fontecache_avancar(void) {}
unsigned fontecache_vod_geracao(void) { return 0; }
void fontecache_vod_limpar(void) {}
void fontecache_vod_apagar(const char *id, const char *tipo, const char *origem,
                          const FontecacheEscopo *escopo) {
  (void)id; (void)tipo; (void)origem; (void)escopo;
}
void fontecache_vod_guardar(const char *id, const char *tipo, const char *origem,
                           const FontecacheEscopo *escopo,
                           const Stream *l, int n, Uint32 quando) {
  (void)id; (void)tipo; (void)origem; (void)escopo; (void)l; (void)n; (void)quando;
}
int fontecache_vod_pegar(const char *id, const char *tipo, const char *origem,
                        const FontecacheEscopo *escopo,
                        Stream **l, int *n, Uint32 *idade) {
  (void)id; (void)tipo; (void)origem; (void)escopo;
  *l = NULL; *n = 0; *idade = 0; return FC_NADA;
}

// Uma busca inteira, esperando o fio acabar, e a frase da folha.
static const char *buscarMotivo(const char *id) {
  static char m[200];
  addons_buscar(id, "movie");
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  if (!addons_motivo_vazio(m, sizeof m)) snprintf(m, sizeof m, "(sem causa)");
  return m;
}

static int falhas;

static void conferir(const char *o_que, int obtido, int esperado) {
  if (obtido == esperado) return;
  printf("FALHOU %s: obtido %d, esperado %d\n", o_que, obtido, esperado);
  falhas++;
}

static void conferirTexto(const char *o_que, const char *obtido, const char *esperado) {
  if (!strcmp(obtido, esperado)) return;
  printf("FALHOU %s: obtido \"%s\", esperado \"%s\"\n", o_que, obtido, esperado);
  falhas++;
}

int main(void) {
  test_addons_set("[{\"name\":\"So legenda\",\"url\":\"https://opensubtitles-v3.strem.io\",\"enabled\":true},{\"name\":\"Fonte e catalogo\",\"url\":\"https://exemplo.test/manifest.json\",\"enabled\":true},{\"name\":\"Formato antigo\",\"url\":\"https://antigo.test\",\"enabled\":true}]");
  addons_manifesto_lido(0, "{\"resources\":[\"subtitles\"]}");
  addons_manifesto_lido(1, "{\"resources\":[\"catalog\",\"stream\"]}");
  addons_manifesto_lido(2, "{\"resources\":[\"stream\"]}");
  addon_registry_refresh();

  // ---- a causa da folha vazia
  // 1) id 1504 (#107): todos responderam {"streams":[]}
  resp[0] = "{\"streams\":[]}"; resp[1] = "{\"streams\":[]}";
  conferirTexto("todos vazios", buscarMotivo("tt0000001"),
                "2 add-ons responderam: nenhum tem este título");
  conferir("debrid_nova_busca por busca", nDebridNovaBusca, 1);
  // 2) um vazio, um mudo
  resp[1] = NULL;
  conferirTexto("vazio + mudo", buscarMotivo("tt0000002"),
                "1 sem este título · 1 sem resposta");
  // 3) algum trouxe fonte: a lista nao esta vazia por culpa dos addons
  resp[0] = "{\"streams\":[{\"url\":\"https://x/a.mp4\"}]}";
  conferir("com fonte nao explica vazio",
           strcmp(buscarMotivo("tt0000003"), "(sem causa)"), 0);
  // 4) so um consultado, e ele nao responde
  addons_alternar(1);                       // "Fonte e catalogo" desligado
  conferirTexto("um mudo", buscarMotivo("tt0000004"), "Formato antigo não respondeu");
  // 5) so um, e responde vazio
  resp[1] = "{\"err\":\"Invalid debrid key\"}";
  conferirTexto("um vazio", buscarMotivo("tt0000005"),
                "Formato antigo respondeu: não tem este título");
  // 6) D5: os de fonte desligados
  addons_alternar(2);
  conferirTexto("desligados", buscarMotivo("tt0000006"),
                "Os add-ons de fontes estão desligados");

  // ---- canal ao vivo: o segundo nome de tipo (issue #112)
  // 7) FrostView sem manifesto lido: "tv" responde 200 vazio. Antes o segundo
  //    nome so saia quando o primeiro NAO respondia, e o canal ficava sem
  //    fonte com o addon tendo quatro. Agora a lista vazia tambem dispara.
  conferir("addon de canal entrou", addons_adicionar("Canal TV", "https://canal.test/manifest.json"), 1);
  respCanalTv = "{\"streams\":[]}";
  respCanalChannel = "{\"streams\":[{\"url\":\"https://x/axn.m3u8\"}]}";
  pedidosCanal[0] = 0;
  addons_definir_origem("https://canal.test");
  addons_buscar("cs:channel:axn", "tv");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("tv vazio -> channel traz a fonte", addons_estado(), ADD_PRONTO);
  conferirTexto("ordem sem manifesto", pedidosCanal, "tv,channel,");
  // 8) Com o manifesto do FrostView (catalogo "channel"), "channel" vai
  //    primeiro e sozinho: uma viagem por canal, nao duas.
  addons_manifesto_lido(3,
    "{\"id\":\"com.frostview\",\"name\":\"Canal TV\","
    "\"resources\":[\"catalog\",\"meta\",\"stream\"],\"types\":[\"channel\"],"
    "\"catalogs\":[{\"id\":\"froststream-channels\",\"type\":\"channel\",\"name\":\"Canais\"}]}");
  pedidosCanal[0] = 0;
  addons_definir_origem("https://canal.test");
  addons_buscar("cs:channel:globo", "tv");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("channel declarado traz a fonte", addons_estado(), ADD_PRONTO);
  conferirTexto("ordem com manifesto", pedidosCanal, "channel,");
  // 9) Os dois nomes vazios: a causa fala de CANAL, e nao de titulo.
  respCanalChannel = "{\"streams\":[]}";
  addons_definir_origem("https://canal.test");
  { static char m[200];
    addons_buscar("cs:channel:bleach", "tv");
    addons_definir_origem(NULL);
    while (addons_estado() == ADD_BUSCANDO) usleep(1000);
    if (!addons_motivo_vazio(m, sizeof m)) snprintf(m, sizeof m, "(sem causa)");
    conferirTexto("canal vazio", m, "Canal TV não tem fonte para este canal agora"); }
  // 10) Segundo nome sem resposta nao apaga o "respondeu vazio" do primeiro.
  respCanalChannel = NULL;
  respCanalTv = "{\"streams\":[]}";
  addons_definir_origem("https://canal.test");
  { static char m[200];
    addons_buscar("cs:channel:sbt", "tv");
    addons_definir_origem(NULL);
    while (addons_estado() == ADD_BUSCANDO) usleep(1000);
    if (!addons_motivo_vazio(m, sizeof m)) snprintf(m, sizeof m, "(sem causa)");
    conferirTexto("channel mudo, tv vazio", m, "Canal TV não tem fonte para este canal agora"); }

  // ---- #182: addon lento aparece sozinho, sem recarregar a mao
  conferir("addon lento entrou", addons_adicionar("Lento", "https://lento.test/manifest.json"), 1);
  // 11) nao respondeu na 1a tentativa, respondeu na 2a: a lista ja o traz.
  falhasLento = 1; chamadasLento = 0;
  addons_definir_origem("https://lento.test");
  addons_buscar("tt0000011", "movie");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("lento respondeu na segunda tentativa", addons_estado(), ADD_PRONTO);
  conferir("duas requisicoes", chamadasLento, 2);
  // #202: filme/serie espera como o Nuvio web (60 s por addon): 30 s na 1a
  // rodada e 30 s na 2a, e nao 12 + 20. Canal ao vivo continua com 12 s.
  conferir("prazo da 1a rodada (filme)", prazoLento[0], 30);
  conferir("prazo da 2a rodada (filme)", prazoLento[1], 30);
  conferir("prazo de canal ao vivo", prazoCanal, 12);
  // 12) fora do ar de verdade: duas consultas gastam a 2a tentativa, a terceira
  //     nao (um addon morto nao pode dobrar o prazo de toda abertura).
  falhasLento = 1000; chamadasLento = 0;
  { int k, esperado[3] = { 2, 2, 1 };
    for (k = 0; k < 3; k++) {
      int antes = chamadasLento;
      addons_definir_origem("https://lento.test");
      addons_buscar("tt0000012", "movie");
      addons_definir_origem(NULL);
      while (addons_estado() == ADD_BUSCANDO) usleep(1000);
      conferir("addon morto: requisicoes da consulta", chamadasLento - antes, esperado[k]);
    } }

  addons_encerrar();
  test_addons_release();
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("source status: ok\n");
  return 0;
}
