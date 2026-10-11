#include "avisos.h"
#include "queda.h"
#include "ilha.h"
#include "ilhasalvar.h"
#define AV_ILHA_CHAVE "avisos"   // o "N avisos novos" da central na ilha (ilha.c junta)
#include "dados.h"
#include "gfx.h"
#include "seguro.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "atualizacao.h"
#include <math.h>
#include "avisodisp.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#define AV_MAX        40
#define AV_VISTOS_ARQ "avisos-vistos.txt"
#define AV_MARCA_ARQ  "sessao-viva.txt"
// 20 s, nao 6 (dono, 20/09/2026: "teria que ficar mais tempo"). Quem esta
// olhando um card do outro lado da tela leva um tempo para notar o canto.
#define AV_TOAST_MS   20000.0f

enum { AV_UPDATE, AV_LOCAL, AV_CRASH };
typedef struct {
  char id[72];
  int  tipo;
  char titulo[160];
  char texto[420];
  int  visto;
  int  anunciar;        // novo: a ilha ainda nao disse o assunto (anunciarNaIlha)
} Aviso;

static Aviso itens[AV_MAX];
static int   n;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// Ids ja vistos, um por linha em avisos-vistos.txt. Um id que ja foi visto nao
// Prevent already-read notices from repeating their toast after restart.
static char vistos[120][72];
static int  nVistos, vistosLidos;

// Painel e toast.
static int   aberto, foco;
static Uint32 okDesde;              // OK afundado numa linha da central (soltarOk)
static void soltarOk(int longo);
static float entrada, rol;
static int   toastN;
static int   vistosSujos;

// Acoes entregues a app.c.
static int  pediuCodigo;

// Previous session interruption.
static int   crashDetectado;
static char  crashQuando[40];
static void vistosLer(void) {
  char *t, *p;
  if (vistosLidos) return;
  vistosLidos = 1;
  t = dados_ler(AV_VISTOS_ARQ);
  if (!t) return;
  p = t;
  while (*p && nVistos < 120) {
    char *fim = strchr(p, '\n');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    if (len > 0 && len < sizeof vistos[0]) { memcpy(vistos[nVistos], p, len); vistos[nVistos][len] = 0; nVistos++; }
    if (!fim) break;
    p = fim + 1;
  }
  free(t);
}
static int foiVisto(const char *id) {
  int i;
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], id)) return 1;
  return 0;
}
static void marcarVisto(const char *id) {
  if (foiVisto(id)) return;
  if (nVistos >= 120) { memmove(vistos[0], vistos[1], sizeof vistos[0] * 119); nVistos = 119; }
  snprintf(vistos[nVistos++], sizeof vistos[0], "%s", id);
  vistosSujos = 1;
}
static void vistosGravar(void) {
  char buf[120 * 72 + 8];
  size_t u = 0;
  int i;
  if (!vistosSujos) return;
  vistosSujos = 0;
  buf[0] = 0;
  for (i = 0; i < nVistos && u + 74 < sizeof buf; i++)
    u += (size_t)snprintf(buf + u, sizeof buf - u, "%s\n", vistos[i]);
  dados_gravar(AV_VISTOS_ARQ, buf);
}

// --- itens ------------------------------------------------------------------------
// Mantem um item por id, atualizando titulo/texto quando ja existe. Devolve 1
// se e NOVO (para o toast).
static int por(const char *id, int tipo, const char *titulo, const char *texto) {
  int i;
  // DISPENSADO NAO VOLTA (avisodisp.h): nem na lista, nem na ilha, nem depois
  // de reiniciar. A chave e o evento; um evento novo tem outra chave.
  if (avisodisp_tem(id)) return 0;
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) {
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "%s", titulo);
    snprintf(itens[i].texto, sizeof itens[i].texto, "%s", texto);
    return 0;
  }
  if (n >= AV_MAX) { memmove(&itens[0], &itens[1], sizeof itens[0] * (AV_MAX - 1)); n = AV_MAX - 1; }
  memset(&itens[n], 0, sizeof itens[n]);
  snprintf(itens[n].id, sizeof itens[n].id, "%s", id);
  itens[n].tipo = tipo;
  snprintf(itens[n].titulo, sizeof itens[n].titulo, "%s", titulo);
  snprintf(itens[n].texto, sizeof itens[n].texto, "%s", texto);
  itens[n].visto = foiVisto(id);
  itens[n].anunciar = !itens[n].visto;
  n++;
  return !itens[n - 1].visto;
}
static void tirar(const char *id) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) {
    memmove(&itens[i], &itens[i + 1], sizeof itens[0] * (size_t)(n - i - 1));
    n--; return;
  }
}
// O RELOGIO DO TOAST COMECA NO PRIMEIRO QUADRO EM QUE ELE PODE SER VISTO, e
// nao quando o aviso chega: o do crash nasce no arranque, com a tela de perfil
// na frente, e com o relogio correndo dali ele ja tinha expirado quando a home
// aparecia (medido na previa do Mac: 32 s de arranque, toast de 6 s).
static int toastPendente;
static void toast(int novos) {
  if (novos <= 0) return;
  toastN += novos;
  toastPendente = 1;
}

void avisos_idioma_definido(const char *codigo, const char *texto) {
  char id[72];
  int novo;
  snprintf(id, sizeof id, "idioma:%s", codigo ? codigo : "");
  pthread_mutex_lock(&trava);
  novo = por(id, AV_LOCAL, i18n("Idioma"), texto ? texto : "");
  pthread_mutex_unlock(&trava);
  toast(novo);
}

int avisos_sessao_anterior_caiu(void) { return crashDetectado; }

void avisos_modo_seguro(const char *id, const char *titulo, const char *texto) {
  int novo;
  pthread_mutex_lock(&trava);
  novo = por(id, AV_LOCAL, titulo ? titulo : "", texto ? texto : "");
  pthread_mutex_unlock(&trava);
  toast(novo);
}

int avisos_n_novos(void) {
  int i, k = 0;
  for (i = 0; i < n; i++) if (!itens[i].visto) k++;
  return k;
}

// --- crash e registro ---------------------------------------------------------
// A MARCA TEM DUAS LINHAS. A primeira ("1.4.1 2026-09-22 18:40") e a de
// sempre: existir na abertura seguinte quer dizer que a sessao nao passou por
// avisos_encerrar. A segunda e o ULTIMO SINAL DE VIDA — evento de janela,
// segundos de sessao e rss — e existe porque "nao se despediu" sozinho nao
// separava nada: 17 registros de 6 pessoas na 1.4.0 e nenhuma linha dizendo se
// a TV matou o app em segundo plano (ultimo=oculto), se ele caiu no meio do uso
// (ultimo=vivo com rss alto) ou se a TV foi desligada. O relatorio 22b pedia
// exatamente isto.
static char marcaCab[64];
static char marcaSinal[96];
static void marcaEscrever(int leve) {
  char buf[180];
  snprintf(buf, sizeof buf, "%s\n%s\n", marcaCab, marcaSinal);
  if (leve) dados_gravar_leve(AV_MARCA_ARQ, buf);
  else      dados_gravar(AV_MARCA_ARQ, buf);
}
static void marcaGravar(void) {
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(marcaCab, sizeof marcaCab, "%s %04d-%02d-%02d %02d:%02d", NV_VERSAO,
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
  snprintf(marcaSinal, sizeof marcaSinal, "ultimo=arranque t=0s");
  marcaEscrever(0);
}
// `evento` NULL = batida periodica: so reescreve a cada 60 s e pela gravacao
// LEVE (no Tizen, o relogio de 15 s do IDBFS em vez do de 700 ms). Evento de
// janela grava na hora e pela gravacao normal: e o sinal que mais importa e o
// que tem menos tempo para chegar ao disco antes de a TV matar o processo.
void avisos_sinal(const char *evento, float rssMb) {
  static Uint32 ultBatida;
  Uint32 agora = SDL_GetTicks();
  if (!marcaCab[0]) return;                    // antes de avisos_iniciar
  if (!evento) {
    if (ultBatida && agora - ultBatida < 60000) return;
    ultBatida = agora;
  }
  snprintf(marcaSinal, sizeof marcaSinal, "ultimo=%s t=%us rss=%.0fMB",
           evento ? evento : "vivo", (unsigned)(agora / 1000), rssMb);
  marcaEscrever(evento == NULL);
}

#ifdef NV_WEBOS
// "chave":"valor" de um JSON raso do nyx, sem depender de js.c: 1 se achou.
static int nyxCampo(const char *txt, const char *chave, char *out, size_t cap) {
  char alvo[64];
  const char *p, *f;
  snprintf(alvo, sizeof alvo, "\"%s\"", chave);
  out[0] = 0;
  if (!txt || !(p = strstr(txt, alvo))) return 0;
  p = strchr(p + strlen(alvo), '"');
  if (!p || !(f = strchr(++p, '"')) || (size_t)(f - p) >= cap) return 0;
  memcpy(out, p, (size_t)(f - p)); out[f - p] = 0;
  return 1;
}
// A LINHA [tv] DO webOS. Android e .tpk ja mandam modelo e sistema; a LG nao
// mandava nada, e a issue #265 (50NANO80ASA, travas e quedas na 2.0.0) nao
// tinha como ser casada com log nenhum. Os dois arquivos sao do nyx; se a
// jaula do app nao deixar ler, a linha diz isso em vez de sumir.
static void tvWebos(void) {
  static const char *const arqs[] = { "/var/run/nyx/device_info.json", "/var/run/nyx/os_info.json" };
  char buf[2][4096] = { "", "" }, modelo[64], placa[64], versao[48], build[64];
  for (int i = 0; i < 2; i++) {
    FILE *f = fopen(arqs[i], "rb");
    if (!f) continue;
    buf[i][fread(buf[i], 1, sizeof buf[i] - 1, f)] = 0;
    fclose(f);
  }
  if (!buf[0][0] && !buf[1][0]) { printf("[tv] webos: /var/run/nyx ilegivel\n"); return; }
  if (!nyxCampo(buf[0], "product_id", modelo, sizeof modelo)) nyxCampo(buf[0], "device_name", modelo, sizeof modelo);
  nyxCampo(buf[0], "hardware_id", placa, sizeof placa);
  nyxCampo(buf[1], "webos_release", versao, sizeof versao);
  nyxCampo(buf[1], "webos_manufacturing_version", build, sizeof build);
  printf("[tv] modelo=%s host=webos-%s placa=%s fw=%s app=%s\n", modelo[0] ? modelo : "?",
         versao[0] ? versao : "?", placa[0] ? placa : "?", build[0] ? build : "?", NV_VERSAO);
}
#endif

void avisos_iniciar(void) {
  char *m;
#ifdef NV_WEBOS
  tvWebos();
  // Record the previous crash in the local log and rearm crash capture.
  { char qd[700];
    if (dados_caminho(qd, sizeof qd, "queda.txt")) { queda_relatar(qd); queda_armar(qd); } }
#endif
  vistosLer();
  m = dados_ler(AV_MARCA_ARQ);
  if (m) {
    char v[24] = "", sinal[96] = "";
    const char *nl;
    // "1.3.1 2026-09-19 18:40" e, desde a 1.4.1, uma segunda linha com o
    // ultimo sinal de vida (ver marcaGravar). Marca antiga nao tem a segunda.
    sscanf(m, "%23s %39[^\n]", v, crashQuando);
    nl = strchr(m, '\n');
    if (nl && nl[1]) sscanf(nl + 1, "%95[^\n]", sinal);
    crashDetectado = 1;
    free(m);
    printf("[avisos] a sessao anterior (%s, %s) nao se despediu: marca presente; %s\n",
           v, crashQuando, sinal[0] ? sinal : "no last signal (old mark)");
    fflush(stdout);
    { char id[72], tit[160], txt[420];
      snprintf(id, sizeof id, "crash:%s", crashQuando);
      snprintf(tit, sizeof tit, "%s", i18n("O app fechou sozinho"));
      snprintf(txt, sizeof txt, i18n("Em %s o Nuvio parou sem avisar. O registro daquela sessão permanece neste aparelho."), crashQuando);
      // Keep startup crash notices in the list without interrupting navigation.
      pthread_mutex_lock(&trava);
      (void)por(id, AV_CRASH, tit, txt);
      pthread_mutex_unlock(&trava); }
  }
  marcaGravar();
#ifndef NV_WEBOS
  // Desktop-only fixture notices. Real update/crash notices remain on the TV.
  { const char *demo = getenv("NUVIO_AVISOS_DEMO");
    if (demo && demo[0] == '1') {
      int novos = 0;
      pthread_mutex_lock(&trava);
      // Fixture notices for update and local recovery UI previews.
      novos += por("demo:update", AV_UPDATE, i18n("Atualização disponível"),
                   "Version 1.3.2 is available. You are on 1.3.1.");
      novos += por("demo:local", AV_LOCAL, i18n("Ajuste desfeito"),
                   "A previous setting was restored after an interrupted session.");
      novos += por("demo:crash", AV_CRASH, i18n("O app fechou sozinho"),
                   "The app stopped unexpectedly. The previous session log remains on this device.");
      pthread_mutex_unlock(&trava);
      toast(novos);
      if (!crashQuando[0]) snprintf(crashQuando, sizeof crashQuando, "2026-09-19 18:57");
    } }
#endif
}

void avisos_encerrar(void) {
  dados_apagar(AV_MARCA_ARQ);
  vistosGravar();
  seguro_encerrar();   // confirma o que estava em prova e fecha a sessao no diario
}

// Fontes que o app ja tem: um item por estado, atualizado a cada volta.
static void colherLocais(void) {
  int novos = 0;
  pthread_mutex_lock(&trava);
  // Atualizacao
  { const char *v = atualizacao_nova();
    if (v && v[0]) {
      char id[72], txt[200];
      snprintf(id, sizeof id, "update:%s", v);
      snprintf(txt, sizeof txt, i18n("Versão %s disponível. Você está na %s."), v, NV_VERSAO);
      novos += por(id, AV_UPDATE, i18n("Atualização disponível"), txt);
    } }
  pthread_mutex_unlock(&trava);
  toast(novos);
}

void avisos_atualizar(float dt, Uint32 agora) {
  static Uint32 ultColheita;
  entrada = anim_mola(entrada, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (agora - ultColheita > 2000) { ultColheita = agora; colherLocais(); }
  // O limiar do OK longo na central, com o dedo ainda no botao.
  if (aberto && okDesde && SDL_GetTicks() - okDesde >= NV_HOLD_MS) soltarOk(1);
  if (!aberto) okDesde = 0;
  if (vistosSujos && !aberto) vistosGravar();
}

int  avisos_aberto(void) { return aberto; }
// A CENTRAL ABERTA LE TUDO: os avisos dela saem da ilha (os que diziam o
// assunto e o "N avisos novos" que juntou o resto).
void avisos_abrir(void)  { aberto = 1; foco = 0; rol = 0.0f; toastN = 0; ilha_retirar_grupo(); }
int avisos_pediu(void) { int c = pediuCodigo; pediuCodigo = 0; return c; }

static void fechar(void) {
  avisos_marcar_lidos();
  aberto = 0;
}

// SEGURAR OK NUMA LINHA DA CENTRAL DISPENSA (06/10). O painel proprio fica por
// cima de tudo e nao hospeda o menu do cartaz (ctxmenu.c so se desenha sobre o
// painel de Salvos): aqui o gesto longo e a acao direta, e a dica na linha em
// foco diz isso. O toque curto continua sendo o OK de sempre, decidido na
// soltura. NV_HOLD_MS e a mesma medida da home e de Salvos.
static void soltarOk(int longo) {
  okDesde = 0;
  if (longo) {
    avisos_lista_dispensar(foco);
    if (foco >= avisos_lista_linhas()) foco = avisos_lista_linhas() > 0 ? avisos_lista_linhas() - 1 : 0;
  } else if (avisos_lista_ok(foco)) fechar();
  else if (foco >= avisos_lista_linhas()) foco = 0;
}

int avisos_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int sc;
  if (e->type == SDL_KEYUP && aberto && okDesde) {
    k = e->key.keysym.sym;
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)
      soltarOk(SDL_GetTicks() - okDesde >= NV_HOLD_MS);
    return 1;
  }
  if (e->type != SDL_KEYDOWN) return aberto;
  k = e->key.keysym.sym; sc = e->key.keysym.scancode;
  if (!aberto) {
    // O AVISO DA CENTRAL NA ILHA e a unica hora em que AZUL/CH+ vem para ca:
    // fora dela as duas teclas continuam sendo o que sempre foram (Salvos,
    // secao do guia). Aviso com modal proprio (recomendacao, versao nova) ja
    // foi atendido antes, em ilha_evento: a pilula cresce para ele.
    if (ilha_tecla_central() && !e->key.repeat &&
        (k == SDLK_s || sc == NV_SCANCODE_BLUE || sc == NV_SCANCODE_CH_UP || k == SDLK_PAGEUP)) {
      avisos_abrir();
      return 1;
    }
    return 0;
  }
  if (e->key.repeat && k != SDLK_UP && k != SDLK_DOWN) return 1;
  if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE) okDesde = 0;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || sc == NV_SCANCODE_BACK ||
      k == SDLK_s || sc == NV_SCANCODE_BLUE) { fechar(); return 1; }
  if (k == SDLK_UP)   { if (foco > 0) foco--; return 1; }
  if (k == SDLK_DOWN) { if (foco + 1 < avisos_lista_linhas()) foco++; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    // Linha de aviso: decide na soltura (curto abre, longo dispensa). A
    // ultima, "Dispensar todos", nao tem o que segurar.
    if (foco < avisos_lista_n()) { okDesde = SDL_GetTicks(); if (!okDesde) okDesde = 1; }
    else soltarOk(0);
    return 1;
  }
  return 1;
}

// --- desenho -----------------------------------------------------------------------
#define AVP_W    760.0f
#define AVP_X    (NV_TELA_W - AVP_W)
#define AVP_MARG  48.0f
#define AVP_TOPO 176.0f

static const char *icone(int tipo) {
  switch (tipo) {
    case AV_UPDATE: return "avancar";
    case AV_CRASH:  return "fluxo";
    default:        return "menu_settings";
  }
}

// Updates and local session notices appear in the clock island.
static void anunciarItem(const Aviso *it) {
  char chave[96], txt[240];
  IlhaAvisoEx e;
  static IlhaModal m;
  memset(&e, 0, sizeof e);
  memset(&m, 0, sizeof m);
  snprintf(chave, sizeof chave, "av:%s", it->id);
  e.chave = chave; e.grupo = 1; e.texto = txt;
  switch (it->tipo) {
    case AV_UPDATE:
      // GLASS UI v2 (mockup ajustes-v2 "v2-upd-aviso", 03/10): o aviso de duas
      // linhas, e a tecla nao abre mais um modal da ilha — o proprio CARTAO DA
      // ATUALIZACAO nasce da pilula (atualizacao.c), pela acao do aviso.
      snprintf(txt, sizeof txt, "%s", it->texto);
      e.titulo = it->titulo;
      e.tipo = ILHA_ACENTO; e.icone = "aj_download"; e.ms = 10000u;
      e.acao = 1; e.dica = i18n("Ver o que mudou");
      break;
    case AV_CRASH:
      // The previous session log stays local; no upload action is offered.
    default:
      // Local notices show their full text in a dismissible modal.
      snprintf(txt, sizeof txt, "%s", it->titulo);
      e.tipo = it->tipo == AV_CRASH ? ILHA_ERRO : ILHA_INFO;
      e.icone = it->tipo == AV_CRASH ? "aj_triangle-alert" : "aj_info";
      e.ms = it->tipo == AV_CRASH ? 9000u : 10000u;
      if (it->tipo == AV_CRASH) e.prior = ILHA_P2;
      if (it->texto[0]) {
        snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Aviso"));
        snprintf(m.titulo, sizeof m.titulo, "%s", it->titulo);
        snprintf(m.texto, sizeof m.texto, "%s", it->texto);
        snprintf(m.icone, sizeof m.icone, "%s", e.icone);
        m.tipo = e.tipo;
        if (it->tipo == AV_CRASH) m.cabecalho = 1;
        // Fechar = lido (a linha fica); Dispensar = some e nao volta (avisodisp.h).
        m.nBotoes = 2;
        snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Fechar"));
        snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Dispensar"));
        snprintf(m.botaoIcone[1], sizeof m.botaoIcone[1], "aj_x");
        e.modal = &m;
      }
      break;
  }
  ilha_avisar_ex(&e);
}

// Os itens com `anunciar` vao para a ilha, cada um uma vez. Sem nenhum
// marcado e com contagem (o toast(N) dos testes), o "N avisos novos" de antes.
//
// LIMITE DE RUIDO (06/10): UMA VEZ POR SESSAO POR CHAVE. Um item que sai e
// volta a lista (o "rec" quando a contagem zera e sobe, a versao nova depois de
// uma lista cheia) entrava de novo com `anunciar` e passava outra vez pela
// pilula. Ja dito nesta sessao (avisodisp_sessao_*) nao passa de novo; um
// evento novo e outra chave e passa.
static void anunciarNaIlha(void) {
  static Aviso copia[AV_MAX];
  int i, k = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (itens[i].anunciar) {
    char ch[96];
    itens[i].anunciar = 0;
    snprintf(ch, sizeof ch, "av:%s", itens[i].id);
    if (avisodisp_sessao_tem(ch)) continue;
    avisodisp_sessao_por(ch);
    copia[k++] = itens[i];
  }
  pthread_mutex_unlock(&trava);
  for (i = 0; i < k; i++) anunciarItem(&copia[i]);
  if (!k && toastN > 0) {
    char txt[120];
    snprintf(txt, sizeof txt, toastN == 1 ? i18n("%d aviso novo") : i18n("%d avisos novos"), toastN);
    { IlhaAvisoEx e;
      memset(&e, 0, sizeof e);
      e.chave = AV_ILHA_CHAVE; e.tipo = ILHA_ACENTO; e.icone = "sino"; e.texto = txt;
      e.ms = (unsigned)AV_TOAST_MS; e.tecla = 1; e.grupo = 1;
      ilha_avisar_ex(&e); }
  }
  toastN = 0;
}

void avisos_ilha_acao(const char *chave, int botao) {
  const char *id;
  if (!chave || strncmp(chave, "av:", 3)) return;
  id = chave + 3;
  if (!strncmp(id, "update:", 7)) {
    if (botao == 1) pediuCodigo = AVISOS_ABRIR_ATUALIZACAO;
  } else if (botao == 2) {
    // Local notice: Close | Dismiss.
    avisos_dispensar(id);
    return;
  }
  avisos_marcar_visto(id);
}

// A LISTA, desenhada dentro de qualquer caixa: o painel proprio usa, e a aba
// AVISOS do painel de Salvos tambem (pedido do dono: abrir quando quiser, sem
// depender do toast). `foco` e de quem chama; -1 = nenhuma linha em foco.
// GLASS UI (mockup "ilha" tela 3, 02/10): a linha do aviso e a LINHA DA ILHA
// do painel Social (salvospainel.c) — sem caixa em repouso, superficie um
// degrau mais clara so no foco, raio 22, recuo 18/22, o disco do icone de 52,
// titulo 24 semibold, texto 19 a 62 % (duas linhas de 25) e a acao em 15 a
// 38 %. 18 + 29 + 4 + 50 + 4 + 18 + 18 = 141. Era um cartao escuro por linha
// e o foco num bloco cheio de acento: na aba Avisos, ao lado das outras
// tres, lia como outro aplicativo.
#define AVL_ROW 142.0f
#define AVL_PADX 22.0f
// A LINHA EM FOCO DE UM AVISO LOCAL CRESCE para o texto inteiro (20/09/2026,
// visto na previa do aviso da 1.3.4-rc1: duas linhas cortavam justamente o
// "onde baixar"). As outras ficam em AVL_ROW. A altura expandida e medida no
// desenho (txt_bloco devolve o que ocupou) e vale a partir do quadro seguinte —
// a mola do hospedeiro engole o quadro de diferenca.
#define AVL_LINHAS_LOCAL 8
static float alturaLocalFoco = AVL_ROW + 4.0f * 27.0f;
static int ehLocalExpansivel(int i) { return i >= 0 && i < n && itens[i].tipo == AV_LOCAL; }
// A ALTURA DE CADA LINHA SAI DO QUE ELA TEM: texto de uma ou de duas linhas,
// e a linha da acao so quando o aviso tem acao. Com AVL_ROW fixo, um aviso de
// "modo seguro" (uma frase, sem acao) ganhava 40 px de superficie vazia
// embaixo no foco — a captura da aba Avisos mostrou. A largura do texto e a
// do ultimo desenho (todo hospedeiro desenha antes de rolar); antes do
// primeiro, a da aba Avisos do painel Social.
static float larguraTextoAviso = 610.0f;
static int temAcaoAviso(const Aviso *av) {
  return av->tipo == AV_UPDATE;
}
// A LINHA EM FOCO SEMPRE TEM A LINHA DE ACAO (06/10): e onde mora "Segure OK
// para dispensar". Fora do foco so quem tem acao a desenha.
static float alturaAvisoF(int i, int focado) {
  const Aviso *av = &itens[i];
  float h = 18.0f + 29.0f + 18.0f;
  if (av->texto[0])
    h += 4.0f + ((float)txt_largura(TXT_ILHA_SUB, av->texto) > larguraTextoAviso ? 50.0f : 25.0f);
  if (temAcaoAviso(av) || focado) h += 4.0f + 18.0f;
  return h < 92.0f ? 92.0f : h;
}
static float alturaAviso(int i) { return alturaAvisoF(i, 0); }
// "DISPENSAR TODOS": a ultima linha da lista, so quando ha o que dispensar.
#define AVL_TODOS_H 84.0f
int avisos_lista_linhas(void) { int k; pthread_mutex_lock(&trava); k = n ? n + 1 : 0; pthread_mutex_unlock(&trava); return k; }
float avisos_lista_altura_linha(int linha, int focoLinha) {
  float h = AVL_ROW;
  pthread_mutex_lock(&trava);
  if (linha == focoLinha && ehLocalExpansivel(linha)) h = alturaLocalFoco;
  else if (linha >= 0 && linha < n) h = alturaAvisoF(linha, linha == focoLinha);
  else if (linha == n && n > 0) h = AVL_TODOS_H;
  pthread_mutex_unlock(&trava);
  return h;
}
float avisos_lista_y(int linha, int focoLinha) {
  float y = 0.0f;
  int i;
  for (i = 0; i < linha; i++) y += avisos_lista_altura_linha(i, focoLinha);
  return y;
}
int avisos_lista_n(void) { int k; pthread_mutex_lock(&trava); k = n; pthread_mutex_unlock(&trava); return k; }

void avisos_lista_desenhar(float x, float y0, float w, float a, int focoLinha) {
  float ar, ag, ab;
  int i;
  const float fr = 243.0f / 255.0f, fg = 242.0f / 255.0f, fb = 239.0f / 255.0f;
  ajustes_acento(&ar, &ag, &ab);
  pthread_mutex_lock(&trava);
  if (n == 0) {
    TxtLinha t = txt_linha(TXT_ILHA_SUB, i18n("Nada por enquanto."), 243, 242, 239, 255);
    txt_desenhar_alpha(t, x + AVL_PADX, y0, a * 0.5f);
  }
  { float y = y0;
  for (i = 0; i < n; i++) {
    const Aviso *av = &itens[i];
    int f = (i == focoLinha);
    int expande = f && av->tipo == AV_LOCAL;
    float tx = x + AVL_PADX + 52.0f + 18.0f, tw = w - (tx - x) - AVL_PADX;
    float rowH;
    larguraTextoAviso = tw;
    rowH = expande ? alturaLocalFoco : alturaAvisoF(i, f);
    GfxRect row = { x, y, w, rowH };
    const char *acao = NULL;
    if (f) {
      if (ajustes_vidro()) gfx_cor(row, 22.0f / row.h, 1, 1, 1, .12f * a);
      else {
        gfx_rect((GfxRect){ row.x - 14.0f, row.y - 2.0f, row.w + 28.0f, row.h + 30.0f }, 0,
                 GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, .30f * a);
        gfx_cor(row, 22.0f / row.h, .169f, .176f, .204f, a);
      }
    }
    // O disco do icone: branco a 8 % (o ".dsc" do mockup), o icone a 85 %.
    if (ajustes_vidro()) gfx_cor((GfxRect){ x + AVL_PADX, y + 18.0f, 52.0f, 52.0f }, 0.5f, 1, 1, 1, .08f * a);
    else gfx_cor((GfxRect){ x + AVL_PADX, y + 18.0f, 52.0f, 52.0f }, 0.5f, .141f, .149f, .173f, a);
    gfx_icone((GfxRect){ x + AVL_PADX + 13.0f, y + 31.0f, 26.0f, 26.0f }, icone(av->tipo),
              fr * .85f, fg * .85f, fb * .85f, a);
    // NOVO = um ponto na cor de acento colado ao icone, e nao uma pilula com
    // palavra: a palavra competia com o titulo e o ponto e o vocabulario que
    // a aba Social ja usa para "qual delas e nova". Agora tambem no foco: a
    // superficie do foco nao e mais o acento, entao o ponto nao some nela.
    if (!av->visto) gfx_cor((GfxRect){ x + AVL_PADX + 40.0f, y + 16.0f, 14.0f, 14.0f }, 0.5f, ar, ag, ab, a);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, av->titulo, f ? 255 : 243, f ? 255 : 242,
                                   f ? 255 : 239, 255, tw);
      txt_desenhar_alpha(t, tx, y + 18.0f, a * (f ? 1.0f : 0.88f)); }
    if (expande) {
      float h = txt_bloco(TXT_ILHA_SUB, av->texto, 243, 242, 239, tx, y + 51.0f, tw, 25.0f, a * 0.62f,
                          AVL_LINHAS_LOCAL);
      float nova = 51.0f + h + 4.0f + 18.0f + 18.0f;   // em foco: sempre a linha de acao
      if (nova < alturaAviso(i)) nova = alturaAviso(i);
      alturaLocalFoco = nova;
    }
    else txt_bloco(TXT_ILHA_SUB, av->texto, 243, 242, 239, tx, y + 51.0f, tw, 25.0f, a * 0.62f, 2);
    switch (av->tipo) {
      case AV_UPDATE: acao = i18n("OK abre a atualização"); break;
      default: break;
    }
    // Em foco, a acao ganha o segundo gesto: segurar OK dispensa (o menu na
    // aba Avisos de Salvos, direto no painel proprio da central).
    { char linhaAcao[200];
      if (f) snprintf(linhaAcao, sizeof linhaAcao, acao ? "%s  ·  %s" : "%s%s",
                      acao ? acao : "", i18n("Segure OK para dispensar"));
      else snprintf(linhaAcao, sizeof linhaAcao, "%s", acao ? acao : "");
      if (linhaAcao[0]) {
        TxtLinha t = txt_linha_corta(TXT_ILHA_HORA, linhaAcao, 243, 242, 239, 255, tw);
        txt_desenhar_alpha(t, tx, y + rowH - 18.0f - 18.0f, a * (f ? 0.62f : 0.38f));
      } }
    y += rowH;
  }
  // DISPENSAR TODOS: a mesma linha da ilha, mais baixa, com o X no disco.
  if (n > 0) {
    int f = focoLinha == n;
    GfxRect row = { x, y, w, AVL_TODOS_H };
    if (f) {
      if (ajustes_vidro()) gfx_cor(row, 22.0f / row.h, 1, 1, 1, .12f * a);
      else gfx_cor(row, 22.0f / row.h, .169f, .176f, .204f, a);
    }
    if (ajustes_vidro()) gfx_cor((GfxRect){ x + AVL_PADX, y + 16.0f, 52.0f, 52.0f }, 0.5f, 1, 1, 1, .08f * a);
    else gfx_cor((GfxRect){ x + AVL_PADX, y + 16.0f, 52.0f, 52.0f }, 0.5f, .141f, .149f, .173f, a);
    gfx_icone((GfxRect){ x + AVL_PADX + 13.0f, y + 29.0f, 26.0f, 26.0f }, "aj_x",
              fr * .85f, fg * .85f, fb * .85f, a);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, i18n("Dispensar todos"), 243, 242, 239, 255,
                                   w - 2.0f * AVL_PADX - 70.0f);
      txt_desenhar_alpha(t, x + AVL_PADX + 70.0f, y + (AVL_TODOS_H - (float)t.h) * 0.5f, a * (f ? 1.0f : 0.72f)); }
  } }
  pthread_mutex_unlock(&trava);
}

// OK numa linha, para quem hospeda a lista (o painel proprio ou a aba de
// Salvos). Devolve 1 quando a linha pediu para o hospedeiro FECHAR (a acao
// abre outra coisa); 0 quando a lista continua (aviso local).
int avisos_lista_ok(int linha) {
  Aviso a;
  int ha = 0;
  pthread_mutex_lock(&trava);
  if (linha >= 0 && linha < n) { a = itens[linha]; ha = 1; }
  pthread_mutex_unlock(&trava);
  if (!ha) {
    // A ultima linha: "Dispensar todos". A lista fica (vazia) no hospedeiro.
    if (linha == avisos_lista_n() && linha > 0) avisos_dispensar_todos();
    return 0;
  }
  switch (a.tipo) {
    case AV_UPDATE: pediuCodigo = AVISOS_ABRIR_ATUALIZACAO; return 1;
    default: return 0;
  }
}

// A ESTREIA AINDA NAO LIDA mais recente (ilhacart.c poe na ilha). Os itens
// entram no fim da lista, entao o mais novo e o de indice maior.

void avisos_marcar_visto(const char *id) {
  int i;
  if (!id || !id[0]) return;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (!strcmp(itens[i].id, id)) itens[i].visto = 1;
  marcarVisto(id);
  pthread_mutex_unlock(&trava);
}

// Tudo lido: o hospedeiro chama ao fechar (o painel proprio e a aba).
void avisos_marcar_lidos(void) {
  int i;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) { itens[i].visto = 1; marcarVisto(itens[i].id); }
  pthread_mutex_unlock(&trava);
}

// --- dispensar (06/10) ---------------------------------------------------------------
// Dismissed notice IDs persist per profile; later events have new IDs.
void avisos_dispensar(const char *id) {
  char ch[96];
  if (!id || !id[0]) return;
  snprintf(ch, sizeof ch, "%s", id);
  avisodisp_por(ch);
  pthread_mutex_lock(&trava);
  marcarVisto(ch);
  tirar(ch);
  pthread_mutex_unlock(&trava);
  { char chIlha[100];
    snprintf(chIlha, sizeof chIlha, "av:%s", ch);
    ilha_retirar(chIlha); }
  printf("[avisos] dispensado: %s\n", ch);
}

void avisos_dispensar_todos(void) {
  char ids[AV_MAX][72];
  int i, k;
  pthread_mutex_lock(&trava);
  for (k = 0; k < n; k++) snprintf(ids[k], sizeof ids[k], "%s", itens[k].id);
  pthread_mutex_unlock(&trava);
  for (i = 0; i < k; i++) avisos_dispensar(ids[i]);
  ilha_retirar(AV_ILHA_CHAVE);
}

int avisos_lista_dispensar(int linha) {
  char id[72] = "";
  pthread_mutex_lock(&trava);
  if (linha >= 0 && linha < n) snprintf(id, sizeof id, "%s", itens[linha].id);
  pthread_mutex_unlock(&trava);
  if (!id[0]) return 0;
  avisos_dispensar(id);
  return 1;
}

int avisos_lista_item(int linha, char *id, size_t tamId, char *titulo, size_t tamTit) {
  int ok = 0;
  pthread_mutex_lock(&trava);
  if (linha >= 0 && linha < n) {
    const Aviso *a = &itens[linha];
    if (id && tamId) snprintf(id, tamId, "%s", a->id);
    if (titulo && tamTit) snprintf(titulo, tamTit, "%s", a->titulo);
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

static void avisos_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void avisos_desenhar(Uint32 agora) {
  ESCALA_INI();
  avisos_desenharCorpo_(agora);
  ESCALA_FIM();
}
static void avisos_desenharCorpo_(Uint32 agora) {
  (void)agora;
  float a = anim_clamp(entrada, 0.0f, 1.0f), dx;
  if (toastPendente && !aberto) {
    toastPendente = 0;
    anunciarNaIlha();
  }
  if (a < 0.01f) return;
  dx = (1.0f - a) * 80.0f;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.45f * a);
  // O painel continua com luz ambiente suave; os cartoes e controles adotam
  // a nova superficie tonal sem retirar a profundidade aprovada da camada.
  { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    GfxRect p = { AVP_X + dx, 24.0f, AVP_W, NV_TELA_H - 48.0f };
    gfx_cor(p, 28.0f / AVP_W, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / AVP_W, AVP_W * 0.9f, -AVP_W * 0.1f, AVP_W * 0.65f, ar, ag, ab, 0.22f * a); }
  { TxtLinha t = txt_linha(TXT_HEADLINE, i18n("Avisos"), 240, 242, 248, 255);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, 64.0f, a); }
  txt_bloco(TXT_CAPTION, i18n("Versões novas, recuperação de ajustes e o que aconteceu com o app neste aparelho."),
            150, 153, 162, AVP_X + dx + AVP_MARG, 118.0f, AVP_W - 2 * AVP_MARG, 28.0f, a, 2);
  gfx_recorte(AVP_X + dx, AVP_TOPO - 8.0f, AVP_W, NV_TELA_H - 80.0f - AVP_TOPO + 8.0f);
  { float areaH = NV_TELA_H - 80.0f - AVP_TOPO;
    float fim = avisos_lista_y(foco, foco) + avisos_lista_altura_linha(foco, foco);
    float alvo = fim > areaH ? fim - areaH : 0.0f;
    rol += (alvo - rol) * 0.25f; }
  // A linha da ilha tem 22 de recuo proprio: a caixa dela sai 22 para fora,
  // e o texto continua na prumada do titulo do painel.
  avisos_lista_desenhar(AVP_X + dx + AVP_MARG - 22.0f, AVP_TOPO - rol, AVP_W - 2 * AVP_MARG + 44.0f, a, foco);
  gfx_sem_recorte();
  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("↑ ↓ escolher · OK agir · Voltar fecha e marca tudo como lido"),
                                 140, 144, 154, 255, AVP_W - 2 * AVP_MARG);
    txt_desenhar_alpha(t, AVP_X + dx + AVP_MARG, NV_TELA_H - 62.0f, a * 0.85f); }
}

#ifdef AVISOS_TESTE
// A queda da sessao anterior na ilha, como o arranque a anunciaria.
void avisos_teste_queda(const char *texto) {
  Aviso a;
  memset(&a, 0, sizeof a);
  snprintf(a.id, sizeof a.id, "teste:crash");
  a.tipo = AV_CRASH;
  snprintf(a.titulo, sizeof a.titulo, "%s", i18n("O app fechou sozinho"));
  snprintf(a.texto, sizeof a.texto, "%s", texto);
  anunciarItem(&a);
}
#endif
