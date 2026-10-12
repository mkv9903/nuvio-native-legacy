#include "addons.h"
#include "addon_registry.h"
#include "idbase.h"
#include "idioma.h"
#include "linguas.h"
#include "streams.h"
#include "debrid.h"
#include "rede.h"
#include "js.h"
#include "marco.h"
#include "ondever.h"
#include "addonstats.h"
#include "fontecache.h"
#include "sessao.h"
#include "perfis.h"
#include "badges.h"
// So para a cache UNICA de manifesto (desc_manifesto_cache_obter/guardar): ver
// a nota grande em sondar(), mais abaixo.
#include "descoberta.h"
#include "legextras.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

/* Immutable Rust data, pinned for each frame or native worker job. */
#define addon (addon_registry_items())
#define nAddon (addon_registry_count())
#define versaoLista (addon_registry_version())
static _Atomic AddEstado estado = ADD_PARADO;
static pthread_t fio;
static char alvoId[64], alvoTipo[16];
// BASE DO ADDON QUE PUBLICOU O ALVO, quando se sabe (canal vindo do guia).
// Com ela, a consulta vai SO a esse addon. Vazia = todos, como sempre foi.
//
// POR QUE: um canal do "Meu Futebol" perguntava fonte ao FrostView, ao
// Debridio, ao AIOStreams — e o FrostView, fora do ar com 408 o dia inteiro,
// segurava a resposta ate o timeout dele. MEDIDO na C9 em 18/09, dono no
// controle: FrostView ligado, dezenas de segundos e cartao de erro; desligado,
// 1,6 s da tecla ate a fonte. O canal nunca foi dele. Nao ha motivo para
// perguntar a quem nao publicou o canal — o id do canal e do addon que o
// declarou, e outro addon nao o conhece (o Meu Futebol responde 404 a id
// alheio, ja se mediu hoje).
static char empty_request[] = "";
static char *alvoBase = empty_request;
// A COPIA QUE O FIO LE. app.c chama addons_definir_origem(NULL) logo depois de
// addons_buscar, e o fio so acorda depois disso: lendo alvoBase direto ele
// achava a origem vazia e perguntava a TODOS os addons — MEDIDO na C9 em
// 18/09 (Debridio e AIOStreams consultados por canal do Meu Futebol com a
// origem definida). A busca copia no disparo; o que o app zere depois nao
// importa mais.
static char *fioBase = empty_request;
static void replace_request_text(char **target, const char *text) {
  char *copy = text && *text ? strdup(text) : NULL;
  if (*target != empty_request) free(*target);
  *target = copy ? copy : empty_request;
}
static int fioVivo;
static Stream *resultado;
static int nResultado;
static Uint32 resultadoQuando;
static int resultadoCacheavel;
static FontecacheEscopo fioEscopo;
static char pendId[64], pendTipo[16];
static int pendRenovar;
// O alvo corrente esta sendo buscado pelo PREFETCH do guia (fontecache.c), e
// nao por `fio`: addons_buscar o encontrou a caminho e resolveu esperar em vez
// de repetir. addons_estado e quem colhe. Ver addons_buscar.
static int adotado;
static void dispararBusca(void);
static void progDrenar(void);

// A FOLHA ENCHE A CADA ADDON QUE RESPONDE (#221). Medido no D1 (1.7.0,
// tizen-tpk, 1186 consultas): a primeira fonte chega em 0,85 s (p90), a lista
// so era publicada no fim — p90 de 12,2 s, maximo de 34,7 s —, porque
// consultar() esperava o ultimo addon, e o que nao respondia em 12 s ainda
// ganhava a segunda chance de 20 s ANTES da publicacao. Na TV do relato
// (UE55RU7170, Tizen 5.0): Torrentio com 29 fontes em 0,2 s e a lista na tela
// aos 32,9 s, segurada por um StreamViX mudo nas duas rodadas.
//
// Agora a busca real (so VOD; canal continua de uma vez, a lista dele e de um
// addon so) deixa cada resposta nesta fila, no fio de rede, e addons_estado /
// addons_drenar publicam no fio da UI com stream_lista_acrescentar. A lista
// final continua sendo montada por consultar() NA ORDEM DOS ADDONS para o
// cache; a da tela e a mesma ordem (streams.c ordena a exibicao por addon).
//
// `progEstado` diz quem falta, para a folha: 1 = esperando, 2 = respondeu,
// 3 = desistiu (sem resposta e sem segunda chance pela frente).
//
// OS PLUGINS (F09) SAO MAIS ORIGENS NA MESMA FILA. Cada scraper da origem
// extra (plugins.c) tem a sua vaga em `progEstadoEx`, com o nome dele, e as
// fontes dele entram na folha com ordem progCount + k: depois de todos os
// addons, na ordem do manifesto.
#define ADD_EXTRA_MAX 160   // = PLUG_SCRAPERS_MAX (plugins.h)
typedef struct { int idx; Stream *a; int n; } Chegada;
static pthread_mutex_t progTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *progEstado;
static int progCount, progCapacity;
static RustAddonSnapshot *progSnapshot;
static const RustAddonView *progress_items(void) {
  const RustAddonSnapshotView *view = nuvio_addons_snapshot_view(progSnapshot);
  return view ? view->addons : NULL;
}
static unsigned char progEstadoEx[ADD_EXTRA_MAX];
static char progNomeEx[ADD_EXTRA_MAX][48];
static Chegada *progFila;
static int progN;
static int progLigado, progPublicou, progExtraPublicou;
static Uint32 progInicio;

static void progMarcar(int i, const Stream *a, int n, int estadoNovo) {
  Stream *copia = NULL;
  if (i < 0 || i >= progCount) return;
  if (a && n > 0) {
    copia = malloc(sizeof(Stream) * (size_t)n);
    if (copia) memcpy(copia, a, sizeof(Stream) * (size_t)n);
  }
  pthread_mutex_lock(&progTrava);
  progEstado[i] = (unsigned char)estadoNovo;
  if (copia && progN < progCapacity) {
    progFila[progN].idx = i; progFila[progN].a = copia; progFila[progN].n = n;
    progN++; copia = NULL;
  }
  pthread_mutex_unlock(&progTrava);
  free(copia);
}

// A parte `k` da origem extra (um scraper). Mesmo contrato de progMarcar,
// com a ordem de exibicao progCount + k.
static void progMarcarEx(int k, const char *nome, const Stream *a, int n, int estadoNovo) {
  Stream *copia = NULL;
  if (k < 0 || k >= ADD_EXTRA_MAX) return;
  if (a && n > 0) {
    copia = malloc(sizeof(Stream) * (size_t)n);
    if (copia) memcpy(copia, a, sizeof(Stream) * (size_t)n);
  }
  pthread_mutex_lock(&progTrava);
  progEstadoEx[k] = (unsigned char)estadoNovo;
  if (nome) snprintf(progNomeEx[k], sizeof progNomeEx[k], "%s", nome);
  if (copia && progN < progCapacity) {
    progFila[progN].idx = progCount + k; progFila[progN].a = copia; progFila[progN].n = n;
    progN++; copia = NULL;
  }
  pthread_mutex_unlock(&progTrava);
  free(copia);
}

static void progLimpar(void) {
  int q;
  pthread_mutex_lock(&progTrava);
  for (q = 0; q < progN; q++) free(progFila[q].a);
  free(progFila); free(progEstado);
  progFila = NULL; progEstado = NULL;
  progN = progCount = progCapacity = 0;
  nuvio_addons_snapshot_destroy(progSnapshot); progSnapshot = NULL;
  memset(progEstadoEx, 0, sizeof progEstadoEx);
  pthread_mutex_unlock(&progTrava);
}
static int progPreparar(void) {
  progSnapshot = addon_registry_capture();
  progCount = nAddon;
  progCapacity = progCount * 2 + ADD_EXTRA_MAX;
  progFila = calloc((size_t)progCapacity, sizeof *progFila);
  progEstado = calloc((size_t)(progCount ? progCount : 1), sizeof *progEstado);
  if (!progFila || !progEstado) { progLimpar(); return 0; }
  return 1;
}

static void capturarEscopo(FontecacheEscopo *e) {
  memset(e, 0, sizeof *e);
  snprintf(e->conta, sizeof e->conta, "%s", sessao_usuario());
  e->perfil = perfis_ativo();
  e->addons = versaoLista;
  e->geracao = fontecache_vod_geracao();
}

static int escopoAindaAtual(const FontecacheEscopo *e) {
  FontecacheEscopo atual;
  capturarEscopo(&atual);
  return atual.perfil == e->perfil && atual.addons == e->addons &&
         atual.geracao == e->geracao && !strcmp(atual.conta, e->conta);
}

static int alvoVod(void) {
  return !strcmp(alvoTipo, "movie") || !strcmp(alvoTipo, "series");
}

AddEstado addons_estado(void) {
  AddEstado e = atomic_load(&estado);
  // As respostas que ja chegaram vao para a folha antes de tudo (#221).
  progDrenar();
  // Publica no fio da UI: nenhum desenho observa uma lista parcialmente escrita.
  if (fioVivo && e != ADD_BUSCANDO) {
    pthread_join(fio, NULL);
    fioVivo = 0;
    // O que o fio deixou na fila depois da ultima drenagem.
    progDrenar();
    if (alvoVod() && !escopoAindaAtual(&fioEscopo)) {
      // Conta/perfil/configuracao mudaram durante a rede. Essa resposta nao
      // pertence mais a tela, nem pode recriar o cache depois do logout.
      free(resultado); resultado = NULL; nResultado = 0;
      estado = ADD_PARADO;
      if (progPublicou && !pendId[0]) stream_invalidar("account or profile changed during the search");
    } else {
      if (alvoVod() && resultadoCacheavel)
        fontecache_vod_guardar(alvoId, alvoTipo, fioBase, &fioEscopo,
                              resultado, nResultado, resultadoQuando);
      // PUBLICADA AOS POUCOS, a lista da tela ja e a inteira: substitui-la
      // agora zeraria o foco da folha, a fonte tocando e a verificacao em
      // curso — exatamente o que a publicacao por addon existe para preservar.
      if (!pendId[0] && !progPublicou) {
        if (alvoVod())
          stream_definir_lista_idade(resultado, nResultado, SDL_GetTicks() - resultadoQuando);
        else stream_definir_lista(resultado, nResultado);
      }
      if (progPublicou)
        printf("[addons] busca completa em %u ms\n", (unsigned)(SDL_GetTicks() - progInicio));
    }
    progLigado = 0; progPublicou = 0; progExtraPublicou = 0;
    progLimpar();
    free(resultado); resultado = NULL; nResultado = 0;
    if (pendId[0]) {
      char id[64], tipo[16];
      int renovar = pendRenovar;
      snprintf(id, sizeof id, "%s", pendId);
      snprintf(tipo, sizeof tipo, "%s", pendTipo);
      pendId[0] = 0;
      pendRenovar = 0;
      if (renovar) addons_buscar_renovar(id, tipo);
      else addons_buscar(id, tipo);
      return ADD_BUSCANDO;
    }
    e = atomic_load(&estado);
  }
  // O alvo esta a caminho pelo prefetch: colhe quando chegar. Se o prefetch
  // cedeu ou nao trouxe nada, a busca real sai daqui — o pedido nunca fica
  // pendurado em "buscando" por causa de um atalho que nao deu certo.
  if (adotado && !fioVivo) {
    Stream *l; int n;
    int r = fontecache_pegar(alvoId, alvoTipo, &l, &n);
    if (r == FC_ACERTO) {
      adotado = 0;
      printf("[addons] %s: %d fontes do prefetch\n", alvoId, n);
      stream_definir_lista(l, n);
      free(l);
      estado = n ? ADD_PRONTO : ADD_VAZIO;
      return atomic_load(&estado);
    }
    if (r == FC_NADA) { adotado = 0; dispararBusca(); }
    return ADD_BUSCANDO;
  }
  // Busca principal ociosa: e a vez do prefetch pendente, se houver.
  if (e != ADD_BUSCANDO && !fioVivo) fontecache_avancar();
  return e;
}

// --- publicacao por addon (#221) ---------------------------------------------
// No fio da UI. Resposta de busca que ja nao e a da tela (pedido novo na fila,
// conta/perfil trocados) e jogada fora aqui, sem nunca chegar a lista.
static void progDrenar(void) {
  Chegada *local;
  int q, k;
  if (!progLigado) return;
  pthread_mutex_lock(&progTrava);
  k = progN;
  local = malloc(sizeof *local * (size_t)(k ? k : 1));
  if (!local) { pthread_mutex_unlock(&progTrava); return; }
  memcpy(local, progFila, sizeof(Chegada) * (size_t)k);
  progN = 0;
  pthread_mutex_unlock(&progTrava);
  for (q = 0; q < k; q++) {
    if (!pendId[0] && escopoAindaAtual(&fioEscopo)) {
      if (!progPublicou)
        printf("[addons] primeira resposta em %u ms: %s\n",
               (unsigned)(SDL_GetTicks() - progInicio),
               local[q].idx < progCount ? progress_items()[local[q].idx].name : local[q].a[0].provedor);
      if (local[q].idx >= progCount && !progExtraPublicou) {
        progExtraPublicou = 1;
        printf("[plugins] primeira fonte de plugin em %u ms: %s\n",
               (unsigned)(SDL_GetTicks() - progInicio), local[q].a[0].provedor);
      }
      stream_lista_acrescentar(local[q].a, local[q].n, local[q].idx);
      progPublicou = 1;
    }
    free(local[q].a);
  }
  free(local);
}

void addons_drenar(void) { progDrenar(); }

int addons_busca_parcial(void) {
  return fioVivo && progLigado && addon_registry_snapshot_current(progSnapshot) && atomic_load(&estado) == ADD_BUSCANDO;
}

unsigned addons_busca_ms(void) {
  return addons_busca_parcial() ? SDL_GetTicks() - progInicio : 0;
}

static void juntarNome(char *nomes, unsigned tam, size_t *usado, int k, const char *nome) {
  if (nomes && tam && *usado + 1 < tam) {
    int w = snprintf(nomes + *usado, tam - *usado, "%s%s", k ? ", " : "", nome);
    if (w > 0) *usado += (size_t)w;
    if (*usado >= tam) *usado = tam - 1;
  }
}

int addons_faltam_tipo(char *nomes, unsigned tam, int *plugins) {
  int i, k = 0, p = 0;
  size_t usado = 0;
  if (nomes && tam) nomes[0] = 0;
  if (plugins) *plugins = 0;
  if (!addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < progCount; i++) {
    if (progEstado[i] != 1) continue;
    juntarNome(nomes, tam, &usado, k, progress_items()[i].name);
    k++;
  }
  // Os plugins depois dos addons, como na folha.
  for (i = 0; i < ADD_EXTRA_MAX; i++) {
    if (progEstadoEx[i] != 1) continue;
    juntarNome(nomes, tam, &usado, k, progNomeEx[i]);
    k++; p++;
  }
  pthread_mutex_unlock(&progTrava);
  if (plugins) *plugins = p;
  return k;
}

int addons_faltam(char *nomes, unsigned tam) { return addons_faltam_tipo(nomes, tam, NULL); }

// QUEM NAO SEGURA MAIS A DECISAO AUTOMATICA (#202, addonstats.h). A espera de
// Ajustes ("Espera pelos add-ons") vale como limite do "lento": um add-on que
// nesta TV passa dela na maioria das buscas quase nunca chega a tempo, e
// esperar por ele so gasta o prazo inteiro. 0 = ninguem e ignorado (Instantaneo
// nao espera ninguem; "Todos os add-ons" e a pessoa pedindo para esperar).
// Ele segue sendo consultado e, quando responde, entra na lista como sempre.
static unsigned espDecisaoMs;
void addons_definir_espera_decisao(int ms) { espDecisaoMs = ms > 0 ? (unsigned)ms : 0; }
static int naoSegura(const char *nome) {
  return nome && nome[0] && addonstats_ignoravel(nome, espDecisaoMs);
}

int addons_faltam_decisivos(void) {
  int i, k = 0;
  if (!addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < progCount; i++)
    if (progEstado[i] == 1 && !naoSegura(progress_items()[i].name)) k++;
  for (i = 0; i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) k++;
  pthread_mutex_unlock(&progTrava);
  return k;
}

// O que a ilha do player diz sobre a espera (inicio.h): o primeiro que a
// decisao ainda espera, e o primeiro que desistiu nesta busca.
void addons_inicio_info(AddonsInicioInfo *o) {
  int i;
  memset(o, 0, sizeof *o);
  if (!addons_busca_parcial()) return;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < progCount; i++) {
    if (progEstado[i] == 1 && !naoSegura(progress_items()[i].name)) {
      if (!o->pendentes++) {
        snprintf(o->pendente, sizeof o->pendente, "%s", progress_items()[i].name);
        o->pendenteMudo = addonstats_mudo_seguidas(progress_items()[i].name) > 0;
      }
    } else if (progEstado[i] == 3 && !o->semResposta++)
      snprintf(o->semRespostaNome, sizeof o->semRespostaNome, "%s", progress_items()[i].name);
  }
  for (i = 0; i < ADD_EXTRA_MAX; i++) {
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) {
      if (!o->pendentes++) {
        snprintf(o->pendente, sizeof o->pendente, "%s", progNomeEx[i]);
        o->pendenteMudo = addonstats_mudo_seguidas(progNomeEx[i]) > 0;
      }
    } else if (progEstadoEx[i] == 3 && !o->semResposta++)
      snprintf(o->semRespostaNome, sizeof o->semRespostaNome, "%s", progNomeEx[i]);
  }
  pthread_mutex_unlock(&progTrava);
}

int addons_pendente_antes(int idx) {
  int i, r = 0;
  if (!addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < idx && i < progCount; i++)
    if (progEstado[i] == 1 && !naoSegura(progress_items()[i].name)) { r = 1; break; }
  for (i = 0; !r && i < idx - progCount && i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) r = 1;
  pthread_mutex_unlock(&progTrava);
  return r;
}

int addons_pendente_nome(const char *nome) {
  int i, r = 0;
  if (!nome || !*nome || !addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < progCount; i++)
    if (progEstado[i] == 1 && !strcasecmp(progress_items()[i].name, nome)) { r = 1; break; }
  for (i = 0; !r && i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !strcasecmp(progNomeEx[i], nome)) r = 1;
  pthread_mutex_unlock(&progTrava);
  return r;
}

// O MELHOR GRUPO (fonteregra.h, #202) que algum addon/plugin que ainda nao
// respondeu pode trazer: o menor `f(nome, plugin)` >= 0 entre os pendentes;
// 99 quando nenhum pendente pode mudar a escolha.
int addons_pendente_grupo_min(int (*f)(const char *nome, int plugin, void *u), void *u) {
  int i, m = 99;
  if (!f || !addons_busca_parcial()) return m;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < progCount; i++)
    if (progEstado[i] == 1 && !naoSegura(progress_items()[i].name)) { int g = f(progress_items()[i].name, 0, u); if (g >= 0 && g < m) m = g; }
  for (i = 0; i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) { int g = f(progNomeEx[i], 1, u); if (g >= 0 && g < m) m = g; }
  pthread_mutex_unlock(&progTrava);
  return m;
}

int addons_ocupado(void) {
  return fioVivo || adotado || atomic_load(&estado) == ADD_BUSCANDO;
}

// --- leitura tolerante de JSON ----------------------------------------------
// Um analisador completo nao se paga aqui: o formato e conhecido e raso, e o
// que importa e nunca travar com campo faltando. Cada funcao devolve o que
// achou ou nada, e quem chama decide.

static const char *pulaEspaco(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}

// Copia o valor textual de "chave" dentro do objeto que comeca em `obj`,
// respeitando escapes. Devolve 1 se achou.

// Acha o fim do objeto JSON que comeca em `p` (que aponta para '{').

// --- legendas ---------------------------------------------------------------

static Legenda legs[LEG_MAX];
static int nLegs;
static pthread_t fioLeg;
static int fioLegVivo, fioLegCriado, legParar;
static char legId[64], legTipo[16];
static unsigned legGeracao;
static RustAddonSnapshot *legSnapshot;
// Extras do Stremio do arquivo que toca (#201). Valem so para `id`: o titulo
// seguinte nao herda o nome nem o hash do anterior.
typedef struct {
  char id[64];
  char arquivo[256];
  unsigned long long tamanho;
  char hash[20];
  char video[4096];
  int exigeCab;
} LegExtras;
static LegExtras legExt;
// A busca em curso e um REFAZER com extras: a lista anterior fica na tela ate
// esta terminar inteira (publicarLegendas so publica no fim).
static int legManter;
// Hash ja medido, para a mesma fonte nao ir de novo ao CDN a cada busca.
static char hashDeUrl[4096], hashMedido[20];
static unsigned long long hashTam;
static pthread_mutex_t legTrava = PTHREAD_MUTEX_INITIALIZER;
#ifdef NV_SHOT_HOOKS
static int subtitle_capture_fixture;
#endif
/* Called while legTrava is held. Capture-only builds can display their
 * explicit synthetic list; real results always require their Rust context. */
static int subtitle_results_current(void) {
#ifdef NV_SHOT_HOOKS
  if (subtitle_capture_fixture) return 1;
#endif
  return addon_registry_snapshot_current(legSnapshot);
}

int addons_n_legendas(void) {
  int n;
  pthread_mutex_lock(&legTrava); n = subtitle_results_current() ? nLegs : 0; pthread_mutex_unlock(&legTrava);
  return n;
}
int addons_legendas_prontas(void) {
  int r;
  pthread_mutex_lock(&legTrava); r = !fioLegVivo && legId[0] && subtitle_results_current(); pthread_mutex_unlock(&legTrava);
  return r;
}
const Legenda *addons_legenda(int i) {
  const Legenda *r = NULL;
  pthread_mutex_lock(&legTrava);
  if (i >= 0 && i < nLegs && subtitle_results_current()) r = &legs[i];
  pthread_mutex_unlock(&legTrava);
  return r;
}

int addons_legendas_copiar(Legenda *dst, int max, unsigned *geracao, int *prontas) {
  int n, i;
  pthread_mutex_lock(&legTrava);
  n = subtitle_results_current() ? (nLegs < max ? nLegs : max) : 0;
  if (n < 0) n = 0;
  for (i = 0; i < n; i++) dst[i] = legs[i];
  if (geracao) *geracao = legGeracao;
  if (prontas) *prontas = !fioLegVivo && legId[0] && subtitle_results_current();
  pthread_mutex_unlock(&legTrava);
  return n;
}

#ifdef NV_SHOT_HOOKS
// Captures (tests/legendas_shot.c): a subtitle list without network.
void addons_shot_legendas(const Legenda *v, int n) {
  pthread_mutex_lock(&legTrava);
  if (n > LEG_MAX) n = LEG_MAX;
  if (n < 0) n = 0;
  memcpy(legs, v, (size_t)n * sizeof *v);
  nLegs = n;
  subtitle_capture_fixture = 1;
  legGeracao++;
  pthread_mutex_unlock(&legTrava);
}
#endif

// Grupos de idioma da busca de legenda, NA ORDEM em que aparecem.
//
// O QUE ESTAVA AQUI: duas listas cravadas ("pob","pt-br",... e "eng","en",...)
// com o comentario "o usuario pediu explicitamente estes dois grupos". Toda
// legenda de outro idioma era descartada sem aviso — quem instala o pacote e
// fala espanhol abria o player e nao achava legenda nenhuma.
//
// AGORA: os grupos vem da preferencia (Ajustes desta TV, senao a conta) e o
// INGLES entra por ultimo quando ha preferencia. Sem preferencia (inclusive
// "Todas" nesta TV), a lista aceita qualquer idioma ate LEG_MAX. Com idiomas
// escolhidos, o ingles continua como reserva para quem nao acha o proprio.
static int gruposIdioma(const char *g[3]) {
  const char *a = ling_legenda(), *b = ling_legenda2();
  int n = 0;
  // Empty preferences include the explicit local "All" choice. One
  // unfiltered group must keep every language, rather than only English.
  if (!a[0] && !b[0]) { g[0] = ""; return 1; }
  if (a[0] && strcasecmp(a, "none")) g[n++] = a;
  if (b[0] && strcasecmp(b, "none") && !(n && ling_casa(b, a))) g[n++] = b;
  { int i, tem = 0;
    for (i = 0; i < n; i++) if (ling_casa("en", g[i])) tem = 1;
    if (!tem) g[n++] = "en"; }
  return n;
}


static int pedidoMudou(unsigned geracao) {
  int mudou;
  pthread_mutex_lock(&legTrava);
  mudou = legParar || geracao != legGeracao;
  pthread_mutex_unlock(&legTrava);
  return mudou;
}

static void episodioPedido(const char *id, int *temporada, int *episodio) {
  // idbase_episodio, e nao o corte no primeiro ':': "kitsu:41370:5" lia
  // temporada 41370 e filtrava toda fonte fora (idbase.h).
  idbase_episodio(id, temporada, episodio);
}

static int episodioCorreto(const char *obj, const char *fim, int temporada, int episodio) {
  int t, e;
  if (temporada <= 0 || episodio <= 0) return 1;
  t = (int)js_num(obj, fim, "season", -1);
  e = (int)js_num(obj, fim, "episode", -1);
  // Alguns addons antigos nao devolvem os campos. Quando devolvem, eles sao
  // uma garantia: nunca mostre T2E3 numa busca por T2E4.
  if ((t >= 0 && t != temporada) || (e >= 0 && e != episodio)) return 0;
  if (t >= 0 || e >= 0) return 1;
  // Alguns addons omitem season/episode mas devolvem o episodio no nome do
  // arquivo. Antes aceitavamos S02E03 numa busca por T2E4 e depois fabricavamos
  // o rotulo T2E4 com base no pedido, escondendo o erro. Se o nome traz uma
  // identidade verificavel, ela precisa casar; nome sem marcador segue aceito.
  { char nome[160] = "", baixo[160]; size_t i;
    if (!js_texto(obj, fim, "subtitleFileName", nome, sizeof nome))
      js_texto(obj, fim, "movieReleaseName", nome, sizeof nome);
    for (i = 0; nome[i] && i + 1 < sizeof baixo; i++)
      baixo[i] = (char)tolower((unsigned char)nome[i]);
    baixo[i] = 0;
    for (i = 0; baixo[i]; i++) {
      int nt = -1, ne = -1;
      if (sscanf(baixo + i, "s%2de%2d", &nt, &ne) == 2 ||
          sscanf(baixo + i, "%2dx%2d", &nt, &ne) == 2)
        return nt == temporada && ne == episodio;
    }
  }
  return 1;
}

// Um balde limitado por addon, antes do teto da folha. O primeiro addon pode
// responder com centenas de legendas; isso nao lhe da todos os 12 lugares
// nem impede a consulta dos seguintes (#158).
typedef struct {
  Legenda itens[LEG_MAX];
  unsigned char grupo[LEG_MAX];
  int n;
} LegLote;

static int arrayDeLegendas(const char *corpo) {
  const char *p;
  if (!corpo) return 0;
  for (p = corpo; *p; p++) {
    const char *fim, *valor;
    if (*p != '"') continue;
    for (fim = p + 1; *fim && *fim != '"'; fim++)
      if (*fim == '\\' && fim[1]) fim++;
    if (!*fim) return 0;
    valor = pulaEspaco(fim + 1);
    if (fim - p == 10 && !strncmp(p + 1, "subtitles", 9) && *valor == ':')
      return *pulaEspaco(valor + 1) == '[';
    p = fim;
  }
  return 0;
}

static int distribuirLegendas(const LegLote *lotes, int nLotes, int nGrupos,
                              Legenda *saida) {
  int gi, n = 0;
  for (gi = 0; gi < nGrupos; gi++) {
    int *prox = calloc((size_t)(nLotes ? nLotes : 1), sizeof *prox);
    int noGrupo = 0, avancou = 1;
    if (!prox) break;
    int teto = LEG_MAX / nGrupos;
    // Um resultado por provider por volta, dentro de cada idioma. Mantem a
    // ordem principal -> secundario -> ingles e preenche lugares vagos com
    // quem ainda tem candidatos quando algum addon respondeu vazio/falhou.
    while (noGrupo < teto && avancou) {
      int i;
      avancou = 0;
      for (i = 0; i < nLotes && noGrupo < teto; i++) {
        while (prox[i] < lotes[i].n && lotes[i].grupo[prox[i]] != gi) prox[i]++;
        if (prox[i] >= lotes[i].n) continue;
        saida[n++] = lotes[i].itens[prox[i]++];
        noGrupo++; avancou = 1;
      }
    }
    free(prox);
  }
  return n;
}

// BUSCA DE LEGENDAS EM PARALELO (legenda automatica, 04/10).
//
// Era SERIAL: um addon por vez, 25 s de teto cada, e a lista so aparecia para o
// resto do app quando o ULTIMO terminava. Um addon lento segurava a legenda
// automatica (addons_legendas_prontas) e a folha por ate 75 s, e no log da TV
// os tres addons respondiam um depois do outro (0,5 + 0,4 + 0,6 s).
// Four workers share the provider queue. Each response can update the existing
// sheet; completion waits for the queue to finish. Provider count is independent
// of the concurrency limit and the subtitle sheet's display quota.
//
// 30 s e nao 8 (#202, "o add-on Subtitlesync.stream nao esta carregando"). O
// Nuvio web da 20 s por addon de legenda (subtitleRepository,
// PER_ADDON_TIMEOUT_MS) e o Stremio espera mais. O Subtitle Sync alinha a
// legenda ao video ANTES de responder e documenta "usually a few seconds, at
// most about 25" na primeira vez de cada video (faq.wait em
// subtitlesync.stream/web/i18n.mjs): com 8 s ele era cortado sempre que o
// video era novo. Como a lista sai a cada resposta, o prazo longo so pesa no
// "prontas", e a legenda automatica decide sozinha em FX_AUTO_FIM_MS (faixas.c).
#define LEG_TETO_S 30
#define SUBTITLE_WORKER_LIMIT 4

typedef struct {
  LegLote *lotes;          // um por addon
  int nLotes, nGrupos, next;
  RustAddonSnapshot *snapshot;
  int manter;              // refazer com extras: publica so no fim
  char extras[1200];       // segmento legextras_segmento; "" = sem extras
  const char *grupos[3];
  char gruposTexto[3][16];
  char id[64], tipo[16];
  int temporada, episodio;
  unsigned geracao;
  pthread_mutex_t m;       // guarda lotes
} LegBusca;

typedef struct { LegBusca *B; int i; } LegFio;

static void publicarLegendas(LegBusca *B, int final) {
  Legenda achadas[LEG_MAX] = {{0}};
  int n;
  if (B->manter && !final) return;
  pthread_mutex_lock(&B->m);
  n = distribuirLegendas(B->lotes, B->nLotes, B->nGrupos, achadas);
  pthread_mutex_unlock(&B->m);
  pthread_mutex_lock(&legTrava);
  if (!legParar && B->geracao == legGeracao && addon_registry_snapshot_current(B->snapshot)) {
    memcpy(legs, achadas, sizeof achadas);
    nLegs = n;
  }
  pthread_mutex_unlock(&legTrava);
}

static void *buscarUmAddon(void *u) {
  LegFio *F = u;
  LegBusca *B = F->B;
  int i = F->i, array = 0, recebidas = 0, gi, comExtras = 0, recuou = 0, cortadas = 0;
  char *url = NULL, *corpo = NULL;
  const char *p, *q;
  RedeMedida medida = {0};
  LegLote *lote = calloc(1, sizeof *lote);
  if (!lote) return NULL;
  if (pedidoMudou(B->geracao) || !addon_registry_reader_current()) { free(lote); return NULL; }
  // Com extras (#201) quando ha; o formato antigo continua sendo o pedido de
  // quem nao sabe o arquivo. O protocolo diz que extra e opcional, mas um addon
  // que responder 4xx/5xx ao caminho com extras ganha o pedido antigo em
  // seguida: legenda generica e melhor que nenhuma.
  if (B->extras[0] && (url = addon_registry_resource_url(i, "subtitles", B->tipo, B->id, B->extras))) {
    comExtras = 1;
    corpo = rede_baixar_medido_controle(url, LEG_TETO_S, NULL, NULL, &medida);
    nuvio_text_destroy(url); url = NULL;
    if (pedidoMudou(B->geracao) || !addon_registry_reader_current()) { free(corpo); free(lote); return NULL; }
    if (medida.status >= 400) { free(corpo); corpo = NULL; recuou = 1; }
  }
  if (!comExtras || recuou) {
    memset(&medida, 0, sizeof medida);
    url = addon_registry_resource_url(i, "subtitles", B->tipo, B->id, NULL);
    corpo = url ? rede_baixar_medido_controle(url, LEG_TETO_S, NULL, NULL, &medida) : NULL;
    nuvio_text_destroy(url); url = NULL;
  }
  if (pedidoMudou(B->geracao) || !addon_registry_reader_current()) { free(corpo); free(lote); return NULL; }
  p = js_array(corpo, NULL, "subtitles");
  // js_array devolve NULL tambem para []: o diagnostico precisa distinguir
  // um array vazio de uma resposta sem o resource esperado.
  array = arrayDeLegendas(corpo);
  for (q = p; q;) {
    const char *f = js_fim(q);
    if (!f || f <= q) break;
    recebidas++;
    q = js_prox(f);
  }
  for (gi = 0; gi < B->nGrupos; gi++) {
    int noGrupo = 0, teto = LEG_MAX / B->nGrupos;
    for (q = p; q && noGrupo < teto;) {
      const char *f = js_fim(q);
      char l[64] = "", cod[16], nome[120] = "", link[2048];
      Legenda *d = &lote->itens[lote->n];
      if (!f || f <= q) break;
      if (*q == '{' && episodioCorreto(q, f, B->temporada, B->episodio) &&
          js_texto(q, f, "lang", l, sizeof l)) {
        // "PORTUGUESE" / "Portuguese (Brazil)" / "por" / "pt-BR" viram um
        // codigo so; antes o nome entrava cortado em 8 bytes ("PORTUGU") e
        // nao casava com a preferencia.
        ling_normalizar(l, cod, sizeof cod);
        // Link lido num buffer MAIOR que o campo: o que nao cabe em d->url e
        // descartado e contado, em vez de virar um link cortado que so falha
        // na hora de baixar.
        if (ling_casa(cod, B->grupos[gi]) && js_texto(q, f, "url", link, sizeof link) &&
            !(strlen(link) >= sizeof d->url && ++cortadas)) {
          snprintf(d->url, sizeof d->url, "%s", link);
          js_texto(q, f, "subtitleFileName", nome, sizeof nome);
          if (!nome[0]) js_texto(q, f, "movieReleaseName", nome, sizeof nome);
          snprintf(d->idioma, sizeof d->idioma, "%s", cod);
          snprintf(d->provedor, sizeof d->provedor, "%s", addon[i].name);
          snprintf(d->arquivo, sizeof d->arquivo, "%s", nome);
          if (B->temporada > 0 && B->episodio > 0)
            snprintf(d->rotulo, sizeof d->rotulo, i18n("T%dE%d  \xc2\xb7  %s%s%.22s"),
                     B->temporada, B->episodio, i18n(ling_nome(cod)), nome[0] ? "  \xc2\xb7  " : "", nome);
          else
            snprintf(d->rotulo, sizeof d->rotulo, "%s%s%.36s",
                     i18n(ling_nome(cod)), nome[0] ? "  \xc2\xb7  " : "", nome);
          lote->grupo[lote->n++] = (unsigned char)gi;
          noGrupo++;
        }
      }
      q = js_prox(f);
    }
  }
  // Somente medidas e enumeracoes publicas. Nao registrar URL, id do
  // titulo, corpo, nome de arquivo, nome configurado ou cabecalhos.
  printf("[addon-recurso] addon=%d resource=subtitles tipo=%s http=%d bytes=%ld ms=%lu array=%d recebidas=%d candidatas=%d extras=%d%s cortadas=%d\n",
         i + 1, !strcmp(B->tipo, "movie") ? "movie" : !strcmp(B->tipo, "series") ? "series" : "outro",
         medida.status, medida.bytes, medida.ms, array, recebidas, lote->n,
         comExtras && !recuou, recuou ? " (recusou extras: pedido antigo)" : "", cortadas);
  // A MESMA LINHA DAS FONTES, com o nome (#202). "addon=3" nao dizia qual era o
  // Subtitle Sync num log de usuario, e "[legendas] concluida: 3" nao separava
  // "respondeu vazio" de "estourou o prazo". O nome do addon ja vai no log das
  // fontes ("[addons] X: N fontes"); URL, id e corpo continuam fora (o corpo de
  // erro pode repetir a configuracao, ver tests/addons_legendas.sh). O texto da
  // libcurl so diz o tipo da falha e o host.
  { const char *erro = medida.status ? "" : rede_ultimo_erro();
    printf("[legendas] %s: %d legendas, %d no idioma (HTTP %d, %ld bytes, %lu ms)%s%s%s\n",
           addon[i].name, recebidas, lote->n, medida.status, medida.bytes, medida.ms,
           comExtras && !recuou ? ", com extras" : "",
           erro && erro[0] ? ": " : "", erro ? erro : ""); }
  fflush(stdout);
  free(corpo);
  pthread_mutex_lock(&B->m);
  B->lotes[i] = *lote;
  pthread_mutex_unlock(&B->m);
  free(lote);
  publicarLegendas(B, 0);
  return NULL;
}

static unsigned relogioMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

// HASH DO OPENSUBTITLES PELA REDE (#201): dois Range de 64 KiB, comeco e fim.
// So 206 com o trecho inteiro vale — servidor que ignora o Range devolve o
// comeco do arquivo com 200, e somar isso como "o fim" daria um hash errado,
// pior que nenhum (o addon casaria a legenda de OUTRO arquivo).
static int medirHash(const char *url, unsigned long long tam, char out[17],
                     const char **motivo) {
  long n1 = 0, n2 = 0;
  int st1 = 0, st2 = 0, e = 0, ok = 0;
  char *a, *b = NULL;
  a = rede_baixar_trecho_st(url, 6, 0, LEGEXTRAS_BLOCO - 1, &n1, &st1, &e, NULL, 0);
  if (a && st1 == 206 && n1 == LEGEXTRAS_BLOCO)
    b = rede_baixar_trecho_st(url, 6, (long)(tam - LEGEXTRAS_BLOCO), (long)(tam - 1),
                              &n2, &st2, &e, NULL, 0);
  if (b && st2 == 206 && n2 == LEGEXTRAS_BLOCO)
    ok = legextras_hash((const unsigned char *)a, (size_t)n1, (const unsigned char *)b,
                        (size_t)n2, tam, out);
  *motivo = ok ? "measured" : !a ? "network failure" : st1 != 206 || (b && st2 != 206) ? "no Range support" : "short range";
  free(a); free(b);
  return ok;
}

// Prepara o segmento de extras de uma busca, com o hash quando da para
// medir. Roda no fio da busca, antes dos addons: o hash custa dois pedidos
// curtos (prazo de 6 s cada) e o resultado vale para todos eles.
static void prepararExtras(LegBusca *B) {
  LegExtras e;
  char hash[20] = "", nome[256];
  const char *motivo = "no source";
  pthread_mutex_lock(&legTrava);
  e = legExt;
  pthread_mutex_unlock(&legTrava);
  B->extras[0] = 0;
  if (!e.id[0] || strcmp(e.id, B->id)) return;
  snprintf(nome, sizeof nome, "%s", e.arquivo);
  if (!nome[0]) legextras_nome_da_url(e.video, nome, sizeof nome);
  if (e.hash[0]) { snprintf(hash, sizeof hash, "%s", e.hash); motivo = "from the addon"; }
  else if (!e.video[0] || !legextras_url_remota(e.video)) motivo = "local or P2P";
  else if (e.exigeCab) motivo = "source needs headers";
  else if (e.tamanho < 2ull * LEGEXTRAS_BLOCO) motivo = "no exact size";
  else {
    int cache;
    pthread_mutex_lock(&legTrava);
    cache = hashMedido[0] && hashTam == e.tamanho && !strcmp(hashDeUrl, e.video);
    if (cache) snprintf(hash, sizeof hash, "%s", hashMedido);
    pthread_mutex_unlock(&legTrava);
    if (cache) motivo = "measured (cached)";
    else if (medirHash(e.video, e.tamanho, hash, &motivo)) {
      pthread_mutex_lock(&legTrava);
      snprintf(hashDeUrl, sizeof hashDeUrl, "%s", e.video);
      snprintf(hashMedido, sizeof hashMedido, "%s", hash);
      hashTam = e.tamanho;
      pthread_mutex_unlock(&legTrava);
    }
  }
  if (legextras_segmento(B->extras, sizeof B->extras, nome, e.tamanho, hash) < 0)
    legextras_segmento(B->extras, sizeof B->extras, NULL, e.tamanho, hash);   // nome enorme: sem ele
  // Sem valores no log: nome de arquivo e hash identificam o que a pessoa ve.
  printf("[legendas] extras: nome=%d tamanho=%d hash=%d (%s)\n", nome[0] != 0,
         e.tamanho != 0, hash[0] != 0, motivo);
  fflush(stdout);
}

static void *subtitle_worker(void *pointer) {
  LegBusca *search = pointer;
  addon_registry_use(search->snapshot);
  for (;;) {
    LegFio task;
    pthread_mutex_lock(&search->m);
    task.i = search->next++;
    pthread_mutex_unlock(&search->m);
    if (task.i >= search->nLotes || !addon_registry_reader_current()) return NULL;
    if (!addon[task.i].enabled || !addon[task.i].subtitles) continue;
    task.B = search;
    buscarUmAddon(&task);
  }
}

static void *buscarLegendas(void *u) {
  (void)u;
  for (;;) {
    LegBusca B;
    pthread_t workers[SUBTITLE_WORKER_LIMIT];
    int created = 0;
    unsigned t0 = relogioMs();
    int i, nAtivos = 0;

    memset(&B, 0, sizeof B);
    pthread_mutex_init(&B.m, NULL);
    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); pthread_mutex_destroy(&B.m); return NULL; }
    snprintf(B.id, sizeof B.id, "%s", legId);
    snprintf(B.tipo, sizeof B.tipo, "%s", legTipo);
    B.geracao = legGeracao;
    B.manter = legManter;
    B.snapshot = nuvio_addons_snapshot_clone(legSnapshot);
    pthread_mutex_unlock(&legTrava);
    addon_registry_use(B.snapshot);
    if (addon_registry_reader_current()) prepararExtras(&B);
    episodioPedido(B.id, &B.temporada, &B.episodio);
    B.nGrupos = gruposIdioma(B.grupos);
    for (i = 0; i < B.nGrupos; i++) {
      snprintf(B.gruposTexto[i], sizeof B.gruposTexto[i], "%s", B.grupos[i]);
      B.grupos[i] = B.gruposTexto[i];
    }
    B.nLotes = nAddon;
    B.lotes = calloc((size_t)(B.nLotes > 0 ? B.nLotes : 1), sizeof *B.lotes);
    if (!B.lotes) printf("[legendas] candidatos: memoria indisponivel\n");

    for (i = 0; B.lotes && i < B.nLotes; i++)
      if (addon[i].enabled && addon[i].subtitles) nAtivos++;
    for (i = 0; B.lotes && i < nAtivos && i < SUBTITLE_WORKER_LIMIT; i++)
      if (pthread_create(&workers[created], NULL, subtitle_worker, &B) == 0) created++;
    if (B.lotes && !created) subtitle_worker(&B);
    for (i = 0; i < created; i++) pthread_join(workers[i], NULL);

    // Refazer com extras: a lista nova entra de uma vez, agora que todos
    // responderam (publicarLegendas descarta se a geracao ja mudou).
    if (B.manter && B.lotes) publicarLegendas(&B, 1);
    free(B.lotes);
    nuvio_addons_snapshot_destroy(B.snapshot);
    pthread_mutex_destroy(&B.m);
    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); return NULL; }
    if (B.geracao != legGeracao) { pthread_mutex_unlock(&legTrava); continue; }
    fioLegVivo = 0;
    legManter = 0;
    i = nLegs;
    pthread_mutex_unlock(&legTrava);
    printf("[subtitles] Complete: %d candidates from %d providers, up to %d concurrent requests, %u ms\n",
           i, nAtivos, SUBTITLE_WORKER_LIMIT, relogioMs() - t0);
    fflush(stdout);
    return NULL;
  }
}


// ------------------------------------------------------------ lista e sonda

/* Addon actions and manifest data are owned by Rust. Search algorithms
 * remain here until the source/subtitle capability is rewritten. */
int addons_alternar(int index) {
  int enabled = addon_registry_toggle(index);
  if (enabled >= 0) fontecache_vod_limpar();
  return enabled;
}
int addons_adicionar(const char *name, const char *url) {
  int added = addon_registry_install(name, url);
  if (added) fontecache_vod_limpar();
  return added;
}
int addons_manifesto_lido(int index, const char *body) {
  return addon_registry_attach_manifest(index, body);
}
static pthread_t fioSonda;
static _Atomic int sondaViva;
static void *sondar(void *snapshot) {
  int i;
  unsigned version;
  addon_registry_use(snapshot);
  nuvio_addons_snapshot_destroy(snapshot);
  version = addons_versao();
  for (i = 0; i < nAddon && addon_registry_reader_current(); i++) {
    char *url, *body;
    if (addon[i].known) continue;
    url = addon_registry_manifest_url(i);
    if (!url) continue;
    body = desc_manifesto_cache_obter(url, version);
    if (!body) body = rede_baixar(url, 12);
    if (body && addon_registry_attach_manifest(i, body))
      desc_manifesto_cache_guardar(url, version, body);
    free(body);
    nuvio_text_destroy(url);
  }
  atomic_store(&sondaViva, 0);
  return NULL;
}
void addons_sondar_manifestos(void) {
  RustAddonSnapshot *snapshot;
  if (nAddon <= 0 || atomic_exchange(&sondaViva, 1)) return;
  snapshot = addon_registry_capture();
  if (pthread_create(&fioSonda, NULL, sondar, snapshot) != 0) {
    nuvio_addons_snapshot_destroy(snapshot);
    atomic_store(&sondaViva, 0);
  } else pthread_detach(fioSonda);
}

static void dispararLegendas(int juntar);

void addons_buscar_legendas(const char *imdb, const char *tipo) {
  int serie, juntar = 0;
  char id[64], tp[16];
  addon_registry_refresh();
  if (!nAddon || !imdb || !*imdb) return;
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !idbase_tem_episodio(imdb))
    snprintf(id, sizeof id, idbase_e_imdb(imdb) ? "%s:1:1" : "%s:1", imdb);
  else
    snprintf(id, sizeof id, "%s", imdb);
  snprintf(tp, sizeof tp, "%s", serie ? "series" : "movie");

  pthread_mutex_lock(&legTrava);
  if (!strcmp(id, legId) && !strcmp(tp, legTipo) && (fioLegVivo || nLegs > 0) && addon_registry_snapshot_current(legSnapshot)) {
    pthread_mutex_unlock(&legTrava);
    return;
  }
  nuvio_addons_snapshot_destroy(legSnapshot);
  legSnapshot = addon_registry_capture();
#ifdef NV_SHOT_HOOKS
  subtitle_capture_fixture = 0;
#endif
  snprintf(legId, sizeof legId, "%s", id);
  snprintf(legTipo, sizeof legTipo, "%s", tp);
  legGeracao++;
  legManter = 0;
  nLegs = 0;
  if (fioLegVivo) { pthread_mutex_unlock(&legTrava); return; }
  juntar = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  dispararLegendas(juntar);
}

// Ver addons.h. Mesmo id normalizado de addons_buscar_legendas, para que os
// extras e a lista falem do MESMO pedido.
void addons_legendas_fonte(const char *imdb, const char *tipo, const char *arquivo,
                           unsigned long long tamanho, const char *hash,
                           const char *urlVideo, int exigeCabecalhos) {
  int serie, juntar, manter;
  char id[64], tp[16];
  LegExtras novo;
  addon_registry_refresh();
  if (!nAddon || !imdb || !*imdb) return;
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !idbase_tem_episodio(imdb))
    snprintf(id, sizeof id, idbase_e_imdb(imdb) ? "%s:1:1" : "%s:1", imdb);
  else
    snprintf(id, sizeof id, "%s", imdb);
  snprintf(tp, sizeof tp, "%s", serie ? "series" : "movie");
  memset(&novo, 0, sizeof novo);
  snprintf(novo.id, sizeof novo.id, "%s", id);
  snprintf(novo.arquivo, sizeof novo.arquivo, "%s", arquivo ? arquivo : "");
  novo.tamanho = tamanho;
  snprintf(novo.hash, sizeof novo.hash, "%s", hash ? hash : "");
  snprintf(novo.video, sizeof novo.video, "%s", urlVideo ? urlVideo : "");
  novo.exigeCab = exigeCabecalhos != 0;
  // Nada que um addon possa usar: o pedido sem extras ja e o que esta feito.
  if (!novo.arquivo[0] && !novo.tamanho && !novo.hash[0] &&
      !legextras_url_remota(novo.video)) return;

  pthread_mutex_lock(&legTrava);
  if (!memcmp(&novo, &legExt, sizeof novo) && !strcmp(id, legId) && !strcmp(tp, legTipo) && addon_registry_snapshot_current(legSnapshot)) {
    pthread_mutex_unlock(&legTrava);
    return;
  }
  legExt = novo;
#ifdef NV_SHOT_HOOKS
  subtitle_capture_fixture = 0;
#endif
  // Mesmo titulo: a lista atual fica ate a nova terminar. Titulo outro (a
  // fonte chegou antes da busca do episodio): busca normal.
  manter = !strcmp(id, legId) && !strcmp(tp, legTipo) && nLegs > 0 && addon_registry_snapshot_current(legSnapshot);
  nuvio_addons_snapshot_destroy(legSnapshot);
  legSnapshot = addon_registry_capture();
  snprintf(legId, sizeof legId, "%s", id);
  snprintf(legTipo, sizeof legTipo, "%s", tp);
  legGeracao++;
  legManter = manter;
  if (!manter) nLegs = 0;
  printf("[legendas] fonte conhecida: refazendo a busca com extras%s\n",
         manter ? " (lista atual fica ate a nova chegar)" : "");
  fflush(stdout);
  if (fioLegVivo) { pthread_mutex_unlock(&legTrava); return; }
  juntar = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  dispararLegendas(juntar);
}

void addons_legendas_reiniciar(void) {
  char id[64], tp[16];
  pthread_mutex_lock(&legTrava);
  snprintf(id, sizeof id, "%s", legId);
  snprintf(tp, sizeof tp, "%s", legTipo);
  // Zerar o alvo e o que desarma a guarda de "mesmo pedido" logo abaixo; a
  // geracao nova faz o fio vivo, se houver, descartar o que ja tinha juntado.
  legId[0] = 0; legTipo[0] = 0;
  nLegs = 0;
  legGeracao++;
  pthread_mutex_unlock(&legTrava);
  if (id[0]) addons_buscar_legendas(id, tp[0] ? tp : "movie");
}

static void dispararLegendas(int juntar) {
  if (juntar) pthread_join(fioLeg, NULL);
  pthread_mutex_lock(&legTrava);
  fioLegCriado = 0;
  legParar = 0;
  fioLegVivo = 1;
  if (pthread_create(&fioLeg, NULL, buscarLegendas, NULL) != 0) fioLegVivo = 0;
  else fioLegCriado = 1;
  pthread_mutex_unlock(&legTrava);
}

// UM FIO POR ADDON DE FONTE.
//
// MEDIDO NA TV, na sessao do dono: 16,5 s entre abrir o titulo e ter uma fonte
// escolhida (detail_abrir 51098 -> fonte escolhida 67659). Eram consultas em
// SERIE com 25 s de timeout cada; um addon lento atrasa todos os outros, e a
// tela fica com "buscando" o tempo todo.
//
// Os addons sao independentes e `extrair` so escreve no balde que recebe, entao
// cada um le no proprio. A ORDEM e preservada na juncao: ela decide qual fonte
// o automatico ve primeiro, e trocar a ordem trocaria a fonte escolhida.
//
// QUATRO TAMBEM NO TIZEN. Na 1.3.4-rc1 eram 2 la (#72): a hipotese era que
// os fios/Workers travavam o fio principal. Os dados a derrubaram — a causa
// era o decode de imagem, e com ele fora do fio principal a rc1 ficou fluida
// com os mesmos addons. O que sobrou de 2 fios foi o efeito colateral: com
// seis addons e um deles preso nos 25 s, a lista de fontes so fechava na
// terceira rodada ("some streams take a very long time to open, especially
// the first time", rawldon na rc1). O fio de rede passa a vida esperando o
// socket; nao e ele que custa.
// NV_ADD_FIOS por -D: a build de comparacao do #80 (rawldon: "v1.3.4, now
// it's back to freezing") volta a 2 no Tizen para isolar se foi isto.
#ifdef NV_ADD_FIOS
#define ADD_FIOS NV_ADD_FIOS
#else
// UM FIO POR ADDON (#221), ate 12. Com 4, a quinta consulta so saia quando um
// dos quatro primeiros soltasse — e um addon mudo segura o fio dele os 12 s
// inteiros. No D1 da 1.7.0 o .tpk tem de 4 a 13 addons de fonte por consulta;
// com fila, o addon rapido instalado por ultimo esperava o lento da frente.
// O fio passa a vida esperando socket (ver acima): doze nao custam CPU.
#define ADD_FIOS 12
#endif

typedef struct {
  int    idx;                 // qual addon
  Stream *achados;
  int    n;
  int    respondeu;           // 1 = veio corpo (mesmo com 0 fontes)
  unsigned ms;                // do disparo da consulta ate a resposta (addonstats)
} BaldeFonte;

// O QUE A ULTIMA BUSCA REAL VIU, para a folha de fontes vazia dizer a causa
// (B6/#107 e D5). A folha so dizia "Nenhuma fonte direta disponivel", e tres
// situacoes diferentes davam essa mesma frase:
//   - id 1504 (#107): 4 addons de fonte consultados, os 4 responderam
//     {"streams":[]} (14 bytes) — o titulo nao existe neles;
//   - id 1220 (rel. 22 §2): a conta tem 2 addons e NENHUM declara stream — nao
//     havia a quem perguntar;
//   - addon fora do ar (FrostView com 408, 18/09): "sem resposta".
// Cada uma pede uma acao diferente de quem esta no sofa (esperar, instalar
// um addon, recarregar), e a frase unica nao dizia qual.
//
// Escrito pelo fio de `buscar` ANTES do atomic_store de `estado`, lido pela UI
// depois de ver o estado mudar: a mesma ordem que ja publica `resultado`.
// `valido` = 0 quando a lista veio do cache/prefetch (nao se sabe quem
// respondeu) — ai a folha cai na frase generica.
typedef struct {
  int valido;
  int instalados, comFonte, ligadosComFonte;   // da lista
  int consultados, responderam, semResposta, comFontes;   // da consulta
  char mudo[64], vazio[64];   // um nome de exemplo de cada, para o singular
} Resumo;
static Resumo resumo;

// UMA CONSULTA INTEIRA, com tudo que os fios dela compartilham. Era um punhado
// de estaticos (baldes, proxBalde, alvoId, alvoTipo), o que amarrava o modulo
// a UMA consulta por vez: o prefetch dos vizinhos do guia (fontecache.c)
// precisa consultar sem tocar no alvo da busca real, entao o estado passou a
// viajar aqui e cada consulta tem o seu. A trava e por consulta tambem: duas
// consultas ao mesmo tempo nao disputam nada.
typedef struct {
  const char *id, *tipo, *tipoAlt;
  RustAddonSnapshot *snapshot;
  BaldeFonte *baldes;
  int nBaldes, proxBalde;
  int (*cancelado)(void *);   // NULL = nunca cancela
  void *ctx;
  int timeout;                // segundos por requisicao (12 na 1a rodada)
  int progresso;              // 1 = a busca real que publica aos poucos (#221)
  int rodada;                 // 0/1 = primeira, 2 = segunda chance
  Uint32 inicio;              // SDL_GetTicks do disparo, para o log por addon
  pthread_mutex_t trava;
} Consulta;

// O NOME DE TIPO DE CANAL QUE O MANIFESTO DO ADDON USA: "tv", "channel" ou
// NULL (manifesto nao lido, ou nenhum catalogo de canal). Sai de catalogs[],
// que capacidadesDoManifesto ja guarda para o guia: FrostView declara
// "channel", IPTV Bridge e Meu Futebol "tv". Devolve a literal, nunca o campo
// do addon, porque o fio que le nao segura trava nenhuma.
static const char *tipoCanalDeclarado(int i) {
  size_t k;
  if (i < 0 || i >= nAddon) return NULL;
  for (k = 0; k < addon[i].catalog_count; k++) {
    const char *type = addon[i].catalogs[k].media_type;
    if (!strcasecmp(type, "channel")) return "channel";
    if (!strcasecmp(type, "tv")) return "tv";
  }
  return NULL;
}

#define ADD_PRAZO_VOD_S 30   // 1a rodada de filme/serie; a 2a soma mais 30 = 60 s do oficial
// O QUE A SEGUNDA TENTATIVA DE UM ADD-ON MUDO CUSTA (#202). Medido no D1:
// 396 linhas "sem resposta: segunda tentativa (30 s)" — quem nao respondeu em
// 30 s ganhava mais 30 s, e a busca completa so fechava depois. Quem ja foi
// mudo na ultima busca desta TV tenta de novo por 10 s; mudo as ultimas tres
// vezes nem tenta (addonstats.h). Responder rapido na segunda continua
// possivel para quem so estava frio (AIOStreams, #182): a primeira falha dele
// nao o marca como mudo seguido.
static int semSegunda(int i) {
  return addon[i].silent_queries >= 2 || addonstats_segunda_s(addon[i].name, ADD_PRAZO_VOD_S) == 0;
}
static int prazoDoAddon(const Consulta *c, int i) {
  int s = c->timeout > 0 ? c->timeout : 12;
  if (c->rodada == 2) {
    int m = addonstats_segunda_s(addon[i].name, s);
    if (m > 0) s = m;
  }
  return s;
}

static void *fioFontes(void *u) {
  Consulta *c = u;
  addon_registry_use(c->snapshot);
  for (;;) {
    int meu, i, n, prazoS;
    char *url, *corpo;
    const char *t1, *t2;
    Stream *achados;
    pthread_mutex_lock(&c->trava);
    if (c->proxBalde >= c->nBaldes) { pthread_mutex_unlock(&c->trava); return NULL; }
    meu = c->proxBalde++;
    pthread_mutex_unlock(&c->trava);
    // Cancelamento entre um addon e outro: o download em curso nao se
    // interrompe (libcurl), mas o proximo nem comeca.
    if (!addon_registry_reader_current() || (c->cancelado && c->cancelado(c->ctx))) continue;
    i = c->baldes[meu].idx;
    // O NOME QUE O MANIFESTO DECLARA VAI PRIMEIRO (issue #112). Ver
    // tipoCanalDeclarado: o FrostView declara "channel" e o app perguntava
    // "tv" antes, gastando uma viagem inteira por canal aberto so para
    // receber a lista vazia que agora dispara o segundo nome logo abaixo.
    { const char *dec = c->tipoAlt && c->tipoAlt[0] ? tipoCanalDeclarado(i) : NULL;
      t1 = c->tipo; t2 = c->tipoAlt;
      if (dec && !strcmp(dec, c->tipoAlt)) { t1 = c->tipoAlt; t2 = c->tipo; } }
    // Prazo da consulta (c->timeout): 30 s para filme/serie, 12 s para canal e
    // prefetch. Ver ADD_PRAZO_VOD_S em consultar().
    prazoS = prazoDoAddon(c, i);
    url = addon_registry_resource_url(i, "stream", t1, c->id, NULL);
    corpo = url ? rede_baixar(url, prazoS) : NULL;
    nuvio_text_destroy(url);
    if (!addon_registry_reader_current()) { free(corpo); continue; }
    achados = NULL; n = 0;
    if (corpo) n = stream_extrair(corpo, addon[i].name, &achados);
    // CANAL AO VIVO TEM DOIS NOMES DE TIPO NO PROTOCOLO, e addons diferentes
    // usam nomes diferentes.
    //
    // MEDIDO em 18/09 contra o addon de um relato ("canal aparece no guia e nao
    // abre", com "Could not open the source" na tela):
    //   /stream/tv/meufutebol:premiereclubes.json       -> 200
    //   /stream/channel/meufutebol:premiereclubes.json  -> 404
    // O manifesto dele declara "types":["tv"]. O app pedia SEMPRE "channel"
    // (app.c), entao a resposta era 404, "sem resposta", nenhuma fonte, e o
    // erro aparecia sem o player nunca ter tentado.
    //
    // Trocar "channel" por "tv" e so mover o defeito para quem usa o outro
    // nome. Entao pergunta-se o SEGUNDO nome ao addon que nao trouxe fonte
    // com o primeiro — E "NAO TROUXE" INCLUI A LISTA VAZIA, nao so a falta de
    // resposta (issue #112). MEDIDO em 22/09 com curl contra o FrostView TV,
    // manifesto "types":["channel"]:
    //   /stream/tv/cs:channel:axn.json       -> 200 {"streams":[]}  (14 bytes)
    //   /stream/channel/cs:channel:axn.json  -> 200 com 4 fontes
    // O 200 vazio passava pelo `if (!corpo)` antigo, o segundo nome nunca era
    // perguntado e TODO canal do FrostView dava "nenhuma fonte serve" — nos
    // dois alvos, porque este caminho nao tem ramo de plataforma (o log do
    // #112 e de uma Samsung, mas a URL e montada igual na LG). Com e sem
    // Origin: null e User-Agent a resposta e a mesma, byte a byte.
    if (n <= 0 && t2 && t2[0] && !(c->cancelado && c->cancelado(c->ctx))) {
      char *alt;
      url = addon_registry_resource_url(i, "stream", t2, c->id, NULL);
      alt = url ? rede_baixar(url, prazoS) : NULL;
      nuvio_text_destroy(url);
      if (!addon_registry_reader_current()) { free(alt); free(achados); free(corpo); continue; }
      if (alt) {
        Stream *a2 = NULL;
        int n2 = stream_extrair(alt, addon[i].name, &a2);
        // O segundo so substitui o primeiro quando traz fonte, ou quando o
        // primeiro nem respondeu: um 404 no segundo nome nao apaga o "respondeu
        // vazio" do primeiro, que e o que a mensagem de erro vai citar.
        if (n2 > 0 || !corpo) {
          free(achados); free(corpo);
          achados = a2; n = n2; corpo = alt;
          printf("[addons] %s: respondeu como \"%s\" (nao como \"%s\")\n",
                 addon[i].name, t2, t1);
        } else { free(a2); free(alt); }
      }
    }
    if (!corpo) {
      free(achados);
      // O MOTIVO NA MESMA LINHA (#202): "curl 28: Operation timed out after
      // 12002 ms" ou o HTTP (a linha "[rede] HTTP 403 em <host>" sai antes, sem
      // o nome do addon). Prazo junto: diz se foi a 1a ou a 2a rodada.
      { const char *erro = rede_ultimo_erro();
        printf("[addons] %s: sem resposta (%u ms, prazo %d s)%s%s\n", addon[i].name,
               (unsigned)(SDL_GetTicks() - c->inicio), prazoS,
               erro && erro[0] ? ": " : "", erro ? erro : ""); }
      // Desistiu de vez quando nao ha segunda chance pela frente: a mesma regra
      // de segundaChance (mudoSeg ainda e o da consulta anterior aqui).
      if (c->progresso)
        progMarcar(i, NULL, 0, c->rodada == 2 || semSegunda(i) ? 3 : 1);
      continue;
    }
    if (c->progresso) progMarcar(i, achados, n, 2);
    c->baldes[meu].respondeu = 1;
    c->baldes[meu].ms = (unsigned)(SDL_GetTicks() - c->inicio);
    c->baldes[meu].n = n;
    c->baldes[meu].achados = achados;
    // O TEMPO DE CADA ADDON NO LOG (#221): sem ele o D1 so dava o total da
    // consulta, e "quem segura" tinha de ser adivinhado pela ordem das linhas.
    printf("[addons] %s: %d fontes (%u bytes, %u ms)\n",
           addon[i].name, c->baldes[meu].n, (unsigned)strlen(corpo),
           (unsigned)(SDL_GetTicks() - c->inicio));
    // RESPOSTA CURTA SEM FONTE VAI PARA O LOG. No registro 1504 havia
    // "Torrentio TB: 0 fontes (75 bytes)": 75 bytes nao sao {"streams":[]}
    // (14), e provavelmente e o addon dizendo por que (chave de debrid
    // invalida, limite) — mas o log nao guardava o texto. Ate 200 bytes
    // porque acima disso e lista de verdade que o parser recusou, e o que
    // falta ali e outra coisa; 80 bytes cabem numa mensagem de erro de addon.
    // Corpo de addon nao leva a chave dele (ela vai no caminho da URL, que
    // aqui nao se imprime); quebra de linha vira espaco para ficar numa linha.
    if (c->baldes[meu].n == 0 && strlen(corpo) <= 200) {
      char amostra[81];
      size_t k;
      snprintf(amostra, sizeof amostra, "%s", corpo);
      for (k = 0; amostra[k]; k++)
        if ((unsigned char)amostra[k] < ' ') amostra[k] = ' ';
      printf("[addons] %s: resposta sem fonte: %s\n", addon[i].name, amostra);
    }
    free(corpo);
  }
}

// --- saude por addon (ver addons_fora_do_ar) ------------------------------------
static pthread_mutex_t foraTrava = PTHREAD_MUTEX_INITIALIZER;
static char foraNome[64];
static unsigned foraSeq;
static void foraAnotar(const char *nome) {
  pthread_mutex_lock(&foraTrava);
  snprintf(foraNome, sizeof foraNome, "%s", nome ? nome : "");
  foraSeq++;
  pthread_mutex_unlock(&foraTrava);
}
unsigned addons_fora_do_ar(char *nome, unsigned tam) {
  unsigned s;
  pthread_mutex_lock(&foraTrava);
  s = foraSeq;
  if (nome && tam) snprintf(nome, tam, "%s", foraNome);
  pthread_mutex_unlock(&foraTrava);
  return s;
}

// O segundo nome de tipo de canal ao vivo; "" para os demais. Ver fioFontes.
static const char *tipoAlternativo(const char *tipo) {
  if (!strcmp(tipo, "tv"))      return "channel";
  if (!strcmp(tipo, "channel")) return "tv";
  return "";
}

// SEGUNDA CHANCE (#182: "addons sometimes not loading, I must reload source a
// few times to see the addon fetch (AIOStreams)"). Um addon que agrega varios
// scrapers (AIOStreams) demora mais que os 12 s na primeira consulta de um
// titulo e responde rapido na seguinte, porque ja guardou o resultado — o que
// o dono fazia a mao com Recarregar. A lista era publicada sem ele e nada mais
// o perguntava. Agora quem NAO respondeu (timeout ou erro; lista vazia conta
// como resposta) e perguntado UMA vez mais, em paralelo, com 20 s (30 s em
// filme/serie, #202), antes de a
// lista ser publicada. Custo limitado: um addon que falha nas duas rodadas
// duas consultas seguidas deixa de ganhar a segunda (mudoSeg).
static void segundaChance(Consulta *c, int fios) {
  Consulta c2;
  int q, m = 0, criados = 0, pulados = 0;
  pthread_t f[ADD_FIOS];
  int *orig;
  if (c->cancelado && c->cancelado(c->ctx)) return;
  orig = calloc((size_t)c->nBaldes, sizeof *orig);
  memset(&c2, 0, sizeof c2);
  c2.baldes = calloc((size_t)c->nBaldes, sizeof(BaldeFonte));
  if (!orig || !c2.baldes) { free(orig); free(c2.baldes); return; }
  for (q = 0; q < c->nBaldes; q++) {
    int i = c->baldes[q].idx;
    if (c->baldes[q].respondeu) continue;
    if (semSegunda(i)) { pulados++; continue; }
    c2.baldes[m].idx = i; orig[m++] = q;
  }
  if (m > 0) {
    c2.id = c->id; c2.tipo = c->tipo; c2.tipoAlt = c->tipoAlt;
    c2.snapshot = c->snapshot; /* Parent owns this until all retry workers join. */
    c2.nBaldes = m; c2.cancelado = c->cancelado; c2.ctx = c->ctx;
    c2.timeout = c->timeout > 20 ? c->timeout : 20;
    c2.progresso = c->progresso; c2.rodada = 2; c2.inicio = c->inicio;
    pthread_mutex_init(&c2.trava, NULL);
    printf("[addons] %d sem resposta: segunda tentativa (%d s%s)", m, c2.timeout,
           pulados ? "; " : "");
    if (pulados) printf("%d mudo(s) seguidos pulado(s)", pulados);
    printf("\n");
    fflush(stdout);
    if (fios > ADD_FIOS) fios = ADD_FIOS;
    for (q = 0; q < fios && q < m; q++)
      if (pthread_create(&f[criados], NULL, fioFontes, &c2) == 0) criados++;
    if (!criados) fioFontes(&c2);
    for (q = 0; q < criados; q++) pthread_join(f[q], NULL);
    for (q = 0; q < m; q++) {
      if (!c2.baldes[q].respondeu) continue;
      c->baldes[orig[q]] = c2.baldes[q];
      printf("[addons] %s: respondeu na segunda tentativa\n", addon[c2.baldes[q].idx].name);
    }
    pthread_mutex_destroy(&c2.trava);
  }
  free(c2.baldes); free(orig);
}

// MAIS UMA ORIGEM DE FONTES: os plugins Nuvio (plugins.c, F09), ligados por
// ponteiro para este modulo continuar compilando sozinho nos testes. Roda num
// fio proprio AO MESMO TEMPO que os addons; cada scraper entra na folha como
// mais uma origem (progMarcarEx) e a lista final soma as fontes dele depois
// das dos addons, com o nome do scraper como provedor.
// Ate duas origens (plugins Nuvio e o servidor Plex casado por IMDb/TMDB), cada
// uma no proprio fio: a consulta rapida ao Plex nao espera os scrapers e os
// scrapers nao seguram a fonte do servidor. As listas se somam na ordem de
// registro.
#define ORIGENS_EXTRA_MAX 2
static OrigemExtra origensExtra[ORIGENS_EXTRA_MAX];
static int (*origensExtraAtiva[ORIGENS_EXTRA_MAX])(void);
static int nOrigensExtra;
void addons_definir_origem_extra(OrigemExtra f, int (*ativa)(void)) {
  int i;
  if (!f) return;
  for (i = 0; i < nOrigensExtra; i++)
    if (origensExtra[i] == f) { origensExtraAtiva[i] = ativa; return; }
  if (nOrigensExtra < ORIGENS_EXTRA_MAX) {
    origensExtra[nOrigensExtra] = f;
    origensExtraAtiva[nOrigensExtra] = ativa;
    nOrigensExtra++;
  }
}
static int origemExtraViva(int i) {
  return origensExtra[i] && origensExtraAtiva[i] && origensExtraAtiva[i]();
}
int addons_origem_extra_ativa(void) {
  int i;
  for (i = 0; i < nOrigensExtra; i++) if (origemExtraViva(i)) return 1;
  return 0;
}

typedef struct {
  const char *id, *tipo;
  int (*cancelado)(void *); void *ctx;
  int progresso;
  Stream *l; int n;
} PedidoExtra;
static int cancelaExtra(void *u) {
  PedidoExtra *p = u;
  return p->cancelado && p->cancelado(p->ctx) ? 1 : 0;
}
static void avisoExtra(void *u, int k, const char *nome, int estado, const void *fontes, int n) {
  PedidoExtra *p = u;
  if (!p->progresso) return;
  // Scraper que terminou (2) ou desistiu (3): entra na memoria de latencia.
  if ((estado == 2 || estado == 3) && nome && !(p->cancelado && p->cancelado(p->ctx)))
    addonstats_registrar(nome, (unsigned)(SDL_GetTicks() - progInicio), estado == 2);
  progMarcarEx(k, nome, (const Stream *)fontes, n, estado);
}
typedef struct { PedidoExtra *pai; OrigemExtra f; Stream *l; int n; } UmaOrigem;
static void *fioUmaOrigem(void *u) {
  UmaOrigem *o = u;
  o->n = o->f(o->pai->id, o->pai->tipo, cancelaExtra, o->pai, avisoExtra, o->pai, &o->l);
  return NULL;
}
static void *fioExtra(void *u) {
  PedidoExtra *p = u;
  UmaOrigem o[ORIGENS_EXTRA_MAX];
  pthread_t f[ORIGENS_EXTRA_MAX];
  int vivo[ORIGENS_EXTRA_MAX], i, total = 0;
  memset(o, 0, sizeof o);
  for (i = 0; i < nOrigensExtra; i++) {
    vivo[i] = 0;
    if (!origemExtraViva(i)) continue;
    o[i].pai = p; o[i].f = origensExtra[i];
    // A ultima origem roda neste fio; as anteriores em fios proprios.
    if (i + 1 < nOrigensExtra) {
      pthread_attr_t a;
      pthread_attr_init(&a);
      pthread_attr_setstacksize(&a, 512u * 1024);
      vivo[i] = pthread_create(&f[i], &a, fioUmaOrigem, &o[i]) == 0;
      pthread_attr_destroy(&a);
      if (!vivo[i]) fioUmaOrigem(&o[i]);
    } else fioUmaOrigem(&o[i]);
  }
  for (i = 0; i < nOrigensExtra; i++) {
    if (vivo[i]) pthread_join(f[i], NULL);
    if (o[i].n > 0) total += o[i].n;
  }
  if (total > 0) {
    Stream *l = malloc(sizeof(Stream) * (size_t)total);
    int k = 0;
    if (l) {
      for (i = 0; i < nOrigensExtra; i++)
        if (o[i].n > 0) { memcpy(l + k, o[i].l, sizeof(Stream) * (size_t)o[i].n); k += o[i].n; }
      p->l = l; p->n = k;
    }
  }
  // Scraper que o corte (30 s) deixou sem resposta: mudo nesta busca. Cancelada
  // a busca, nao prova nada.
  if (p->progresso && !(p->cancelado && p->cancelado(p->ctx))) {
    pthread_mutex_lock(&progTrava);
    for (i = 0; i < ADD_EXTRA_MAX; i++)
      if (progEstadoEx[i] == 1 && progNomeEx[i][0])
        addonstats_registrar(progNomeEx[i], (unsigned)(SDL_GetTicks() - progInicio), 0);
    pthread_mutex_unlock(&progTrava);
  }
  for (i = 0; i < nOrigensExtra; i++) free(o[i].l);
  return NULL;
}

static int consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida,
                     Resumo *rs, int progresso) {
  Consulta c;
  Stream *achados = NULL;
  int n = 0, i, q;
  PedidoExtra extra;
  pthread_t fioEx;
  int temExtra = 0;
  *saida = NULL;
  if (!id || !*id || !tipo || !*tipo) return 0;
  // So na busca ampla: canal com origem conhecida pergunta a um addon so.
  memset(&extra, 0, sizeof extra);
  if (!(base && *base) && addons_origem_extra_ativa()) {
    pthread_attr_t a;
    extra.id = id; extra.tipo = tipo; extra.cancelado = cancelado; extra.ctx = ctx;
    extra.progresso = progresso;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 512u * 1024);
    temExtra = pthread_create(&fioEx, &a, fioExtra, &extra) == 0;
    pthread_attr_destroy(&a);
    if (!temExtra) fioExtra(&extra);
  }
  if (nAddon <= 0) {
    if (temExtra) pthread_join(fioEx, NULL);
    if (cancelado && cancelado(ctx)) { free(extra.l); return -1; }
    if (extra.n > 0) { *saida = extra.l; return extra.n; }
    free(extra.l);
    return 0;
  }
  memset(&c, 0, sizeof c);
  c.id = id; c.tipo = tipo; c.tipoAlt = tipoAlternativo(tipo);
  c.snapshot = addon_registry_capture();
  c.cancelado = cancelado; c.ctx = ctx;
  c.progresso = progresso;
  // PRAZO DA BUSCA DE FILME/SERIE (paridade com o Nuvio oficial, #202). O
  // Nuvio web espera 60 s por addon (streamRepository,
  // STREAM_SOURCE_REQUEST_TIMEOUT_MS) num pedido so. Aqui eram 12 s e, para
  // quem nao respondeu, um pedido NOVO de 20 s: addon que raspa indexadores na
  // hora (StreamFusion, WAStream, Comet sem cache) era cortado no meio do
  // trabalho e perguntado de novo do zero. Log do .tpk (2.0.0):
  // "StreamFusionReborn falha 28 ... after 12002 milliseconds with 0 bytes",
  // e so a segunda volta trouxe fonte. Com a lista publicada aos poucos (#221)
  // o prazo longo nao segura quem ja respondeu, e a fonte automatica tem prazo
  // proprio (fonteauto). Canal ao vivo e prefetch continuam com 12 s.
  c.timeout = progresso ? ADD_PRAZO_VOD_S : 12;
  c.inicio = SDL_GetTicks();
  pthread_mutex_init(&c.trava, NULL);
  c.baldes = calloc((size_t)nAddon, sizeof(BaldeFonte));
  if (c.baldes) {
    // SO O ADDON DE ORIGEM, quando se sabe qual e. Comparacao por base
    // normalizada — "<base>" e "<base>/manifest.json" sao o mesmo addon.
    if (base && *base) {
      char *target = nuvio_addon_normalize_url(base);
      for (i = 0; target && i < nAddon; i++) {
        if (addon[i].enabled && addon[i].stream && !strcmp(addon[i].base, target))
          c.baldes[c.nBaldes++].idx = i;
      }
      nuvio_text_destroy(target);
    }
    // Sem origem conhecida, ou origem que nao esta (mais) na lista: todos,
    // como sempre. Melhor perguntar a mais do que nao perguntar a ninguem.
    if (c.nBaldes == 0)
      for (i = 0; i < nAddon; i++)
        if (addon[i].enabled && addon[i].stream) c.baldes[c.nBaldes++].idx = i;
  }

  // QUEM FICOU DE FORA, E POR QUE (issue #83).
  //
  // Um addon nao consultado nao imprime NADA: nem "N fontes", nem "sem
  // resposta". No log do usuario ele some, e "desligado na conta" fica
  // indistinguivel de "consultado e nao respondeu" — que e exatamente a
  // duvida do #83 ("either the addons aren't being searched or frostview
  // might be overriding the two others"). MEDIDO nos registros 1.3.12 do D1:
  // no id 1383 o manifesto do PenguPlay diz stream=1 e ele nao aparece em
  // NENHUMA das sete consultas da sessao; nao ha como dizer, pelo log, se foi
  // `ativo` ou `fonte` que o barrou.
  //
  // UMA VEZ POR VERSAO DE LISTA, e nao por consulta: a mesma resposta em toda
  // abertura de titulo seria ruido, e a lista so muda quando muda.
  //
  // SO NA BUSCA AMPLA. Com origem conhecida (canal do guia) a consulta vai de
  // proposito a um addon so, e contar "1 de 16" ali seria alarme falso.
  //
  // Os dois estaticos ficam sem trava de proposito: o prefetch e a busca real
  // podem entrar aqui ao mesmo tempo, e o pior que acontece e a folha sair
  // duas vezes. Uma trava para nao repetir uma linha de log custaria mais do
  // que vale.
  { static unsigned ultimaFolha;
    static int folhaFeita;
    if (!(base && *base) && (!folhaFeita || ultimaFolha != versaoLista)) {
      ultimaFolha = versaoLista; folhaFeita = 1;
      for (i = 0; i < nAddon; i++) {
        if (addon[i].enabled && addon[i].stream) continue;
        printf("[addons] fora da busca de fontes: %s (%s)\n", addon[i].name,
               !addon[i].enabled ? "desligado" :
               addon[i].known ? "o manifesto nao declara stream"
                                : "ainda sem manifesto");
      }
      printf("[addons] %d de %d consultados por fonte\n", c.nBaldes, nAddon);
      fflush(stdout);
    } }

  if (progresso && c.baldes) {
    pthread_mutex_lock(&progTrava);
    for (q = 0; q < c.nBaldes; q++)
      if (c.baldes[q].idx < progCount) progEstado[c.baldes[q].idx] = 1;
    pthread_mutex_unlock(&progTrava);
  }

  if (c.baldes && c.nBaldes > 0) {
    pthread_t f[ADD_FIOS];
    int criados = 0;
    if (fios > ADD_FIOS) fios = ADD_FIOS;
    for (q = 0; q < fios && q < c.nBaldes; q++)
      if (pthread_create(&f[criados], NULL, fioFontes, &c) == 0) criados++;
    if (!criados) fioFontes(&c);          // sem fios: em serie, mesmo resultado
    for (q = 0; q < criados; q++) pthread_join(f[q], NULL);
    segundaChance(&c, fios);
    // Junta NA ORDEM DOS ADDONS, que e a ordem em que o dono os instalou.
    for (q = 0; q < c.nBaldes; q++) {
      int k = c.baldes[q].n;
      // A LATENCIA DESTA BUSCA fica na memoria desta TV (addonstats.h): so a
      // busca real e nao cancelada — prefetch e busca interrompida nao provam
      // nada sobre o add-on.
      if (c.progresso && !(c.cancelado && c.cancelado(c.ctx)))
        addonstats_registrar(addon[c.baldes[q].idx].name,
                             c.baldes[q].respondeu ? c.baldes[q].ms
                                                   : (unsigned)(SDL_GetTicks() - c.inicio),
                             c.baldes[q].respondeu);
      {
        int64_t silent = addon_registry_report_response(c.baldes[q].idx, c.baldes[q].respondeu);
        if (rs && silent == 1 && !(c.cancelado && c.cancelado(c.ctx)))
          foraAnotar(addon[c.baldes[q].idx].name);
      }
      if (rs) {
        const char *nome = addon[c.baldes[q].idx].name;
        rs->consultados++;
        if (!c.baldes[q].respondeu) {
          if (!rs->semResposta++) snprintf(rs->mudo, sizeof rs->mudo, "%s", nome);
        } else if (k > 0) rs->comFontes++;
        else if (!rs->responderam++) snprintf(rs->vazio, sizeof rs->vazio, "%s", nome);
      }
      if (k > 0) {
        Stream *tmp = realloc(achados, sizeof(Stream) * (size_t)(n + k));
        if (tmp) { achados = tmp;
          memcpy(achados + n, c.baldes[q].achados, sizeof(Stream) * (size_t)k);
          n += k;
        } else printf("[addons] memoria insuficiente para %d fontes\n", k);
      }
      free(c.baldes[q].achados);
    }
  }
  free(c.baldes);
  nuvio_addons_snapshot_destroy(c.snapshot);
  pthread_mutex_destroy(&c.trava);
  if (temExtra) pthread_join(fioEx, NULL);
  if (progresso) addonstats_salvar();
  if (extra.n > 0) {
    Stream *tmp = realloc(achados, sizeof(Stream) * (size_t)(n + extra.n));
    if (tmp) { achados = tmp; memcpy(achados + n, extra.l, sizeof(Stream) * (size_t)extra.n); n += extra.n; }
  }
  free(extra.l);
  // Cancelada, a lista pode estar pela metade: nao e resposta, e lixo.
  if (!addon_registry_reader_current() || (cancelado && cancelado(ctx))) { free(achados); return -1; }
  *saida = achados;
  return n;
}

int addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida) {
  addon_registry_refresh();
  return consultar(id, tipo, base, fios, cancelado, ctx, saida, NULL, 0);
}

// Contagens da LISTA (nao da consulta), para "nao ha a quem perguntar".
static void resumoDaLista(Resumo *rs) {
  int i;
  memset(rs, 0, sizeof *rs);
  rs->valido = 1;
  rs->instalados = nAddon;
  for (i = 0; i < nAddon; i++) {
    if (!addon[i].stream) continue;
    rs->comFonte++;
    if (addon[i].enabled) rs->ligadosComFonte++;
  }
}

// Frases curtas, uma por causa. `responderam` conta so quem respondeu SEM
// fonte: quem trouxe fonte nao explica lista vazia (se ha fonte e a lista
// esta vazia, a causa e o descarte de torrent sem debrid, e quem diz isso e
// streams.c).
//
// CANAL TEM FRASE PROPRIA (issue #112). "nao tem este titulo" le como se o
// canal nao existisse no addon; o que o FrostView diz com {"streams":[]} e que
// AGORA nao ha link para ele — os mesmos canais voltam a ter fonte depois.
int addons_motivo_vazio(char *dst, unsigned n) {
  const Resumo *r = &resumo;
  int canal = !strcmp(alvoTipo, "tv") || !strcmp(alvoTipo, "channel");
  if (!dst || !n || !r->valido) return 0;
  if (!r->instalados || !r->comFonte)
    snprintf(dst, n, "%s", i18n("Nenhum add-on de fontes instalado"));
  else if (!r->ligadosComFonte)
    snprintf(dst, n, "%s", i18n("Os add-ons de fontes estão desligados"));
  else if (!r->consultados || r->comFontes)
    return 0;
  else if (!r->semResposta && canal)
    r->responderam == 1
      ? snprintf(dst, n, i18n("%s não tem fonte para este canal agora"), r->vazio)
      : snprintf(dst, n, i18n("%d add-ons responderam: nenhum tem fonte para este canal agora"), r->responderam);
  else if (!r->semResposta)
    r->responderam == 1
      ? snprintf(dst, n, i18n("%s respondeu: não tem este título"), r->vazio)
      : snprintf(dst, n, i18n("%d add-ons responderam: nenhum tem este título"), r->responderam);
  else if (!r->responderam)
    r->semResposta == 1
      ? snprintf(dst, n, i18n("%s não respondeu"), r->mudo)
      : snprintf(dst, n, i18n("%d add-ons não responderam"), r->semResposta);
  else
    snprintf(dst, n, i18n("%d sem este título · %d sem resposta"),
             r->responderam, r->semResposta);
  return 1;
}

static void *buscar(void *u) {
  Stream *achados = NULL;
  int n;
  addon_registry_use(u);
  nuvio_addons_snapshot_destroy(u);
  Resumo rs;
  marco("addons: consulta inicio");
  resumoDaLista(&rs);
  n = consultar(alvoId, alvoTipo, fioBase, ADD_FIOS, NULL, NULL, &achados, &rs,
                progLigado);
  resultadoQuando = SDL_GetTicks();
  resultadoCacheavel = n > 0 && rs.semResposta == 0;
  if (n < 0) n = 0;
  resumo = rs;
  marco(n ? "addons: fontes recebidas" : "addons: nenhuma fonte");
  // O canal fica no cache para o zap de volta. VOD so entra na publicacao
  // pela UI, depois de conferir a conta/perfil/configuracao capturados.
  if (addon_registry_reader_current() && escopoAindaAtual(&fioEscopo))
    fontecache_guardar(alvoId, alvoTipo, achados, n);
  resultado = achados; nResultado = n;
  printf("[addons] total %d\n", n);
  fflush(stdout);
  atomic_store(&estado, n ? ADD_PRONTO : ADD_VAZIO);
  return NULL;
}

// A busca real do alvo corrente vai a rede. Chamado com fioVivo == 0.
static void dispararBusca(void) {
  // Pedido real tem prioridade: o prefetch em curso (de OUTRO canal — o deste
  // teria sido adotado em addons_buscar) larga os addons que faltam.
  fontecache_ceder();
  estado = ADD_BUSCANDO;
  fioVivo = 1;
  progLimpar();
  progLigado = alvoVod() && progPreparar();
  progPublicou = 0; progExtraPublicou = 0;
  progInicio = SDL_GetTicks();
  capturarEscopo(&fioEscopo);
  replace_request_text(&fioBase, alvoBase);
  replace_request_text(&alvoBase, NULL);   // consumida: origem e do pedido, nao de sessao
  {
    RustAddonSnapshot *snapshot = addon_registry_capture();
    if (pthread_create(&fio, NULL, buscar, snapshot) != 0) {
      nuvio_addons_snapshot_destroy(snapshot);
      fioVivo = 0; progLigado = 0; estado = ADD_PARADO;
    }
  }
}

// Diz de que addon o PROXIMO alvo veio. Chamar ANTES de addons_buscar; a
// busca seguinte consome e zera — origem e do pedido, nao de sessao.
void addons_definir_origem(const char *base) {
  replace_request_text(&alvoBase, base);
}

static void buscarPedido(const char *imdb, const char *tipo, int forcar) {
  int serie, renovar;
  addon_registry_refresh();
  if (!imdb || !*imdb) return;
  ondever_pedir(imdb, tipo && (!strcmp(tipo, "tv") || !strcmp(tipo, "series")), 0);
  resumo.valido = 0;
  // Recusa de conta do debrid vale por busca: a nova volta a tentar todos.
  debrid_nova_busca();
  if (!nAddon && !addons_origem_extra_ativa()) { stream_definir_lista(NULL, 0); resumoDaLista(&resumo); estado = ADD_VAZIO; return; }
  if (fioVivo) {
    if (forcar || strcmp(imdb, alvoId) || strcmp(tipo ? tipo : "movie", alvoTipo)) {
      snprintf(pendId, sizeof pendId, "%s", imdb);
      snprintf(pendTipo, sizeof pendTipo, "%s", tipo ? tipo : "movie");
      pendRenovar = forcar;
      // O alvo mudou com uma busca em curso: a lista em memoria e do alvo velho
      // e nao deve ser exibida na folha nem usada pelo alvo novo.
      if (stream_n() > 0 && !stream_lista_do_alvo(imdb)) {
        stream_invalidar("novo alvo pendente nos addons");
      }
    }
    return;
  }
  // Um pedido novo desfaz a espera pelo prefetch do anterior; o prefetch em si
  // segue ou cede conforme o que vem abaixo.
  adotado = 0;
  serie = tipo && !strcmp(tipo, "series");
  // Serie SEM episodio devolve lista vazia, com HTTP 200 e sem erro nenhum
  // (medido: 14 bytes de resposta). O identificador tem de ser
  // "tt1234567:temporada:episodio". Como o catalogo ainda nao traz lista de
  // episodios, assume T1E1 — e o mesmo lugar onde o episodio real entra quando
  // houver.
  // Serie de addon de anime ("kitsu:41370") pede "id:episodio", nao "id:1:1".
  if (serie && !idbase_tem_episodio(imdb))
    snprintf(alvoId, sizeof alvoId, idbase_e_imdb(imdb) ? "%s:1:1" : "%s:1", imdb);
  else
    snprintf(alvoId, sizeof alvoId, "%s", imdb);
  snprintf(alvoTipo, sizeof alvoTipo, "%s", tipo && *tipo ? tipo : "movie");
  // Pedir de novo a lista que ainda esta ativa e renovar/recarregar, inclusive
  // depois de falha de reproducao: esse pedido continua indo a rede.
  renovar = forcar || (stream_n() > 0 && stream_lista_do_alvo(alvoId));
  stream_definir_lista(NULL, 0);
  { int t = 0, e = 0;
    idbase_episodio(alvoId, &t, &e);
    debrid_definir_episodio(t, e); }
  if (alvoVod()) {
    FontecacheEscopo escopo;
    Stream *l;
    int n;
    Uint32 idade;
    capturarEscopo(&escopo);
    if (renovar) fontecache_vod_apagar(alvoId, alvoTipo, alvoBase, &escopo);
    else if (fontecache_vod_pegar(alvoId, alvoTipo, alvoBase, &escopo,
                            &l, &n, &idade) == FC_ACERTO) {
      stream_definir_lista_idade(l, n, idade);
      free(l);
      replace_request_text(&alvoBase, NULL);
      estado = ADD_PRONTO;
      printf("[addons] %d fontes VOD reaproveitadas (%u ms)\n", n, (unsigned)idade);
      return;
    }
  }
  // O CACHE ANTES DA REDE. Canal que o guia engatilhou (ou que acabou de sair
  // do ar) responde daqui, sem fio nenhum; canal cujo prefetch esta na rede
  // AGORA e adotado — esperar o que ja esta a caminho e mais curto que repetir
  // as mesmas requisicoes, e addons_estado publica quando chegar.
  { Stream *l; int n;
    int r = fontecache_pegar(alvoId, alvoTipo, &l, &n);
    if (forcar && r == FC_ACERTO) { free(l); r = FC_NADA; }
    if (forcar && r == FC_EM_CURSO) r = FC_NADA;
    if (r == FC_ACERTO) {
      printf("[addons] %s: %d fontes do cache\n", alvoId, n);
      stream_definir_lista(l, n);
      free(l);
      estado = n ? ADD_PRONTO : ADD_VAZIO;
      return;
    }
    if (r == FC_EM_CURSO) {
      printf("[addons] %s: prefetch em curso, esperando por ele\n", alvoId);
      adotado = 1;
      estado = ADD_BUSCANDO;
      return;
    } }
  dispararBusca();
}

void addons_buscar(const char *imdb, const char *tipo) { buscarPedido(imdb, tipo, 0); }
void addons_buscar_renovar(const char *imdb, const char *tipo) { buscarPedido(imdb, tipo, 1); }

void addons_encerrar(void) {
  int juntarLeg;
  fontecache_encerrar();
  adotado = 0;
  if (fioVivo) pthread_join(fio, NULL);
  fioVivo = 0;
  progLigado = 0; progPublicou = 0;
  progLimpar();
  pthread_mutex_lock(&legTrava);
  legParar = 1; legGeracao++; juntarLeg = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  if (juntarLeg) pthread_join(fioLeg, NULL);
  pthread_mutex_lock(&legTrava);
  fioLegCriado = fioLegVivo = 0; nLegs = 0;
#ifdef NV_SHOT_HOOKS
  subtitle_capture_fixture = 0;
#endif
  nuvio_addons_snapshot_destroy(legSnapshot); legSnapshot = NULL;
  pthread_mutex_unlock(&legTrava);
  free(resultado); resultado = NULL; nResultado = 0;
  replace_request_text(&alvoBase, NULL); replace_request_text(&fioBase, NULL);
  estado = ADD_PARADO;
}
