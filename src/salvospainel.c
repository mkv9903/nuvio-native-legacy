// Painel "Salvos" — a camada da direita que a tecla AZUL abre. Ver salvospainel.h
// para o que ele substituiu e por que.
//
// O QUE ELE MOSTRA, e a decisao nao e obvia: a UNIAO das tres fontes de "quero
// ver", nao so a lista local. As tres caem na mesma marca (`CatItem.naLista`):
// a watchlist do Trakt (descoberta.c), a biblioteca da conta (contalib.c) e a
// lista local (salvos.c). A aba "Salvos" da tela de Biblioteca ja mostra essa
// uniao, e duas telas chamadas "Salvos" mostrando conjuntos diferentes seria
// exatamente o defeito que o app irmao teve com quatro botoes "+".
//
// A lista local entra por fora do catalogo de proposito. Ela guarda titulo,
// poster e meta no proprio arquivo (ver salvos.h), entao o painel se desenha no
// primeiro quadro do arranque — antes de a descoberta responder. Sem isso o
// atalho mais rapido do controle abriria vazio por ~20 s toda vez que a TV
// liga, que e justamente quando alguem aperta.
#include "salvospainel.h"
#include "cwretido.h"
#include "salvos.h"
#include "salvosorg.h"
#include "teclado.h"
#include "simkl.h"
#include "avisos.h"
#include "agenda.h"
#include "agendaui.h"
#include "catalogo.h"
#include "ctxmenu.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "movimento.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "botoes.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "extras.h"
#include "perfis.h"
#include "dados.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

// Mesma pegada do painel "Sua atividade" que ele substitui (perfil.c desenhava
// em x=1120, 776x1032): quem ja tinha o gesto na memoria muscular encontra a
// camada no mesmo lugar, so com outro conteudo.
#define SP_X          (NV_TELA_W - 800.0f)   // 1120 em 1080; tela virtual (escala.h)
#define SP_W           776.0f
// A ILHA FLUTUA A NV_FOLHA_MARGEM das tres bordas e com o raio da folha de
// Fontes (streams.c): as duas camadas da direita sao o mesmo objeto, e uma
// colada no topo ao lado da outra solta leria como dois materiais.
#define SP_Y          NV_FOLHA_MARGEM
#define SP_H          (NV_TELA_H - 2.0f * NV_FOLHA_MARGEM)
#define SP_RAIO        NV_FOLHA_RAIO
// A GRAMATICA DO MOCKUP APROVADO ("Glass UI — ilha", tela 3; o CSS esta em
// design/glass-ilha/glass-ilha.html), em px de 1080p e RELATIVA AO TOPO E A
// ESQUERDA DO PAINEL — o mockup poe a ilha a 40 da borda e a folha de Fontes
// real a 24, entao o que se copia e a distancia de dentro, nao a absoluta:
//   cabecalho  40 de ar; o kicker (15 caixa alta) e "Social" (40 bold) a 48 da
//              borda. SEM o disco de fechar do mockup: o dono pediu para tirar
//              o X (03/10); VOLTAR e ESQUERDA na borda fecham.
//   abas       136 do topo, o seletor a 42 da borda: 5 de folga, segmentos de
//              45 (19 semibold, 20 de recuo, contagem 16 em cinza a 9 do rotulo)
//   lista      217 do topo; linhas de 26 a 26 das bordas, raio 22, 18/22 de
//              recuo; secao 22 bold com um fio de 1 px a 8 % embaixo
// Era o painel "parecido": abas no lugar do titulo, rotulos de secao em
// cinza 22, rosto e capa juntos a esquerda (foto do dono lado a lado com o
// mockup, 02/10: "o mockup ta bem mais polido que a build").
#define SP_PAD          48.0f   // do painel ao texto (26 da linha + 22 dela)
#define SP_INTERNO    (SP_W - SP_PAD * 2.0f)
#define SP_LINHA_X      26.0f   // do painel a superficie da linha
#define SP_LINHA_W    (SP_W - SP_LINHA_X * 2.0f)
#define SP_LINHA_PADX   22.0f
#define SP_LINHA_RAIO   22.0f
#define SP_KICK_Y      (SP_Y + 40.0f)
#define SP_TIT_Y       (SP_Y + 62.0f)
// ABAS. Elas so existem quando o servico de recomendacoes foi compilado
// (recomenda_ativo); sem ele o painel e exatamente o que era, sem uma linha a
// mais de cromo para uma funcao que nao existe naquele pacote.
// A FAIXA DE ABAS MORA NA LINHA DO TITULO (pedido do dono, 07/10): o titulo grande
// diz a aba aberta e as abas, compactas (icone + contagem), ficam a direita dele.
// Era uma segunda fileira de 55 px abaixo do titulo; a lista subiu esses 77 px.
#define SP_ABAS_Y      (SP_Y + 60.0f)
#define SP_ABAS_H        52.0f
#define SP_ABAS_X       42.0f
// AR DO FOCO entre o recorte da lista e a primeira linha. A superficie da linha
// focada comeca no `y` da linha, e a lista comecava exatamente no topo do
// recorte: a primeira linha em foco — e qualquer linha que a rolagem alinhava
// ao topo — saia com a borda de cima reta, sem os cantos (foto do dono, 01/10,
// aba Social com vidro; medido no tests/spainel_foco_shot). O conteudo comeca
// SP_FOCO_AR abaixo do recorte e a rolagem guarda o mesmo ar em cima e embaixo
// da linha focada. O recorte fica 12 px abaixo das abas, e o conteudo nasce
// em 217 do topo do painel, onde o mockup poe a lista.
#define SP_FOCO_AR      14.0f
#define SP_LISTA_Y     (SP_Y + 140.0f - SP_FOCO_AR)
#define SP_LISTA_BASE  (SP_Y + SP_H - 24.0f)
// A LINHA DE SALVOS: cartaz 2:3 de 64x96 com 18 px de recuo em cima e
// embaixo, a mesma altura de ritmo das linhas do feed (capa 52x76 e 114). O
// cartaz de 92x138 de antes fazia a aba Salvos parecer outra tela ao lado da
// Atividade.
#define SP_POSTER_W     76.0f   // 64 antes: a lista com capa +19 % (pedido do dono, 03/10)
#define SP_POSTER_H    114.0f   // 96 antes; 2:3 mantido
#define SP_LINHA_PADY   21.0f   // 18 antes
#define SP_LINHA_SALVO (SP_POSTER_H + SP_LINHA_PADY * 2.0f)
// As linhas se encostam, como no mockup: so uma tem superficie (a focada), e
// o recuo de 18 de cada uma ja e o ar entre os textos.
#define SP_PASSO       SP_LINHA_SALVO
// ROTULO DE SECAO ("Hoje", "Continuar"): 22 bold em branco, 10 px ate um fio
// de 1 px a 8 % de borda a borda da LINHA, e 6 px ate a primeira linha. Acima,
// 6 px na primeira secao da lista e 22 nas outras (.sec do mockup, margem 6 e
// "margin-top:22px" na segunda). Era CAPTION 22 cinza com 24 de ar: lia como
// nota de rodape, nao como divisao.
#define SP_SECAO_AR1     6.0f
#define SP_SECAO_AR     22.0f
#define SP_SECAO_TXT    27.0f   // a altura da linha de 22 px
#define SP_SECAO_H     (SP_SECAO_AR + SP_SECAO_TXT + 10.0f + 1.0f + 6.0f)
#define SP_SECAO_H1    (SP_SECAO_AR1 + SP_SECAO_TXT + 10.0f + 1.0f + 6.0f)
#define SP_TEXTO_X    (SP_PAD + SP_POSTER_W + 21.0f)
#define SP_TEXTO_W    (SP_INTERNO - SP_POSTER_W - 21.0f)
// Barra de progresso do card de retomada: o trilho fino do mockup, com o que
// FALTA ao lado ("T3E4 · 43 min restantes"), que e o dado que importa.
#define SP_BARRA_W     230.0f
#define SP_BARRA_LABEL_GAP 14.0f
#define SP_BARRA_H       5.0f
// A LINHA DA ILHA (ver linhaIlhaRet e andaresIlha, mais abaixo).
#define SPI_H         114.0f   // 18 + 29 + 4 + 23 + 4 + 18 + 18
#define SPI_H2         92.0f   // dois andares: 18 + 29 + 4 + 23 + 18
#define SPI_AV         52.0f
#define SPI_AV_GAP     18.0f
#define SPI_CAP_W      52.0f
#define SPI_CAP_H      76.0f
#define SPI_CAP_RAIO    9.0f
// A RECOMENDACAO E UM CARTAO DEITADO (dono, F08): arte 16:9 na altura da capa.
#define SPI_CAPD_W    135.0f
#define SPI_FG_R 243
#define SPI_FG_G 242
#define SPI_FG_B 239
// O VEU DA FOLHA DE FONTES (streams.c): 30 % sobre o vidro, 42 % no solido.
// Era 58 %: a arte atras do painel apagava, e o vidro nao tinha o que mostrar
// — no mockup o fundo continua vivo a esquerda da ilha.
#define SP_VEU   (ajustes_vidro() ? 0.30f : 0.42f)
// O RASTRO. A linha focada aqui e um bloco inteiro na cor de realce, e nao um
// anel: com a mesma mola nos dois sentidos (95 % em 120 ms) e a tecla presa
// descendo a ~10 linhas/s, duas ou tres linhas acima da focada ainda estavam
// acesas pela metade — uma cauda de pilulas atras do foco. E a explicacao
// mais provavel para o "deixando rastro" do dono (24/09/2026), somada ao
// ritmo irregular de 52 fps; nao foi conferida na TV. A que perde o foco
// apaga em ~50 ms (95 %);
// a que ganha continua na mola do app. A mola e exp(-k*dt): o tempo e o mesmo
// a 60 ou a 30 quadros por segundo.
#define SP_MOLA_DESFOCO 60.0f

// Teto de linhas do painel. ERA 200, com a lista local podendo ter 300 e a
// conta mais 200: o que passasse de 200 sumia do painel sem aviso, e como a
// lista local vem primeiro na ordem de insercao, sumia justamente o mais novo.
// Agora cabe tudo que pode existir (lista local + catalogo inteiro); o vetor de
// linhas mora no heap e cresce so ate o que a lista de verdade tem.
#define SP_MAX (SALVOS_MAX + CAT_MAX)

// Linha ja resolvida: o desenho nao volta ao catalogo nem a lista local por
// quadro.
//
// OS TEXTOS SAO COPIADOS, E NAO APONTADOS — e isto derrubou o app na TV.
//
// A primeira versao guardava `const char *titulo` apontando para dentro do
// CatItem, com um comentario afirmando que a memoria era estavel. Nao e: o
// vetor `itens` de catalogo.c e do heap e TROCA DE BLOCO a cada republicacao
// (cat_definir_tudo/cat_acrescentar_lote fazem malloc do bloco novo e liberam o
// antigo). O proprio catalogo.c documenta isso na linha da troca e segura UM
// bloco velho em `lixo` justamente porque alguem ja leu memoria liberada ali —
// mas uma folga de um bloco nao salva quem guarda o ponteiro por varios ciclos.
//
// Na TV o resultado foi core dump de 218 MB alguns segundos depois do arranque,
// quando o segundo ciclo de sync republicou o catalogo (o log parava logo apos
// "[contalib] biblioteca da conta aplicada"). No Mac, sem conta, o catalogo
// nunca era republicado e nada acontecia — o defeito so existia com dados reais.
//
// Copiar custa ~800 bytes por linha, no heap e do tamanho da lista real. E o
// preco de nao depender do tempo de vida de um bloco que outro modulo troca sem
// avisar.
typedef struct {
  char  titulo[160], poster[512], meta[96];
  char  id[24];
  int   serie;
  int   nota;
  int   progresso, temporada, episodio, restanteMin;
  long long quandoS;      // 0 = veio do Trakt/conta, nao sabemos quando entrou
  // As duas legendas da barra de retomada, montadas na reconstrucao e nao a
  // cada quadro: dependem so do progresso, que so muda com o catalogo (e ai
  // a lista e reconstruida). A rasterizacao ja era cacheada em text.c — o log
  // da C9 dizia "texto 0.0ms em 0 linhas" com o painel aberto —; isto poupa
  // os snprintf e as buscas de i18n de cada linha visivel.
  char  txtRestante[96], txtVisto[96];
  // ORGANIZACAO (salvosorg.h). `fundo` e a arte 16:9 do catalogo, para o
  // estilo paisagem; vazio cai no cartaz. `tipoG` e o grupo de tipo (SPT_*),
  // `cat` a categoria da pessoa (0 = nenhuma), `orig` a posicao na ordem de
  // sempre — o desempate de toda ordenacao, para a lista nunca dancar.
  char  fundo[512];
  int   ano, tipoG, cat, orig;
  // ONDE A CELULA MORA, relativo ao topo do conteudo e a esquerda da coluna.
  // Calculado na reconstrucao (montarLayout), nunca por quadro: o desenho, a
  // rolagem e o D-pad perguntam ao mesmo lugar. `fila` e a linha visual —
  // na grade varias celulas dividem a mesma.
  float lx, ly, lw, lh;
  int   fila;
} SPLinha;
// Tipos para "Agrupar por tipo", na ordem das secoes.
enum { SPT_FILME = 0, SPT_SERIE, SPT_COLECAO, SPT_CANAL, SPT_N };

static SPLinha *linhas;
static int nLinhas, capLinhas;

// Vaga para `n` linhas. Cresce dobrando ate SP_MAX; 0 quando nao ha memoria, e
// quem chama para de acrescentar (a lista fica curta, mas nao corrompe).
static int garantirLinhas(int n) {
  SPLinha *novo;
  int cap;
  if (n <= capLinhas) return 1;
  if (n > SP_MAX) return 0;
  cap = capLinhas ? capLinhas * 2 : 64;
  while (cap < n) cap *= 2;
  if (cap > SP_MAX) cap = SP_MAX;
  novo = (SPLinha *)realloc(linhas, sizeof *linhas * (size_t)cap);
  if (!novo) return 0;
  linhas = novo;
  capLinhas = cap;
  return 1;
}
static int nCont;            // quantas das primeiras linhas sao "Continuar"

// A ABA SOCIAL. `foco == SP_FOCO_ABAS` e a linha de cima, onde esquerda e
// direita trocam de aba; do zero para baixo o D-pad e o de sempre. Uma linha
// de foco "fora da lista" em vez de um modo separado porque o resto do painel
// (rolagem, animacao de foco, recorte) continua valendo sem mudanca nenhuma.
// A ABA AVISOS e a central de avisos (avisos.h) dentro deste painel, para
// abrir quando se quiser e nao so no toast. Existe SEMPRE; a Social so com o
// servico de recomendacoes.
// ATIVIDADE (02/10/2026, tela B do desenho aprovado): o feed dos amigos
// agrupado por dia, entre Salvos e a antiga Social — que passou a se chamar
// AMIGOS, porque e isso que ela lista (recomendacoes recebidas, sugestoes e a
// lista de amigos). O nome interno SP_ABA_SOCIAL ficou: e o mesmo conteudo.
// AGENDA (07/10): os proximos episodios das series seguidas, do mesmo modulo
// (agenda.h) da tela Agenda. O id e o numero que vai para o arquivo de abas do
// perfil: nunca renumerar, so acrescentar.
enum { SP_ABA_SALVOS = 0, SP_ABA_ATIVIDADE = 1, SP_ABA_SOCIAL = 2, SP_ABA_AVISOS = 3,
       SP_ABA_AGENDA = 4, SP_ABA_N = 5 };
#define SP_FOCO_ABAS (-1)
static int aba;

#define SPS_H_ACAO      72.0f

// AGENDA: indices em agenda_lista() dos proximos episodios, mais a linha final
// "Abrir a agenda completa". So entram series com data de estreia de hoje em
// diante; o resto (sem data, encerradas) e assunto da tela Agenda.
#define SPAG_MAX 24
#define SPAG_H   120.0f   // 116 da linha + 4: a mesma da tela Agenda (agendaui)
#define SPAG_VAZIO_H 150.0f   // o texto de "nada a caminho", antes do botao
static int agIdx[SPAG_MAX];
static int nAg;
static int agVer = -1;
static int temPedidoAgenda;
// Cabecalho de DATA antes da primeira linha de cada dia (como a tela Agenda):
// altura do cabecalho que precede a linha `i`, 0 quando o dia nao muda.
static float agAntes(int i);
// EDITAR ABAS: `editando` troca a lista por uma folha com uma linha por aba
// (ligar/desligar, subir, descer). `editLin` = linha, `editCol` = botao.
static int editando, editLin, editCol;
// O lapis da faixa tem foco proprio (foco == SP_FOCO_ABAS e editLapis).
static int editLapis;

static int aberto, foco, marcaCatN = -1;
static float entrada, scrollY;
static float entradaP, entradaV;
static float trocaT = 1.0f, trocaV;
static int trocaDir;
static char tituloAnt[96];
static float pilX, pilXv, pilW, pilWv, pilAlvoX, pilAlvoW;
static int pilIni;
#define SP_TROCA_DESLIZE 72.0f
#define SP_TITULO_DESLIZE 28.0f
static float velY;
static float animFoco[SP_MAX];

static char  pedido[24];
static int   temPedido;

// SEGURAR OK NUMA LINHA DE "SALVOS" abre o menu do cartaz (ctxmenu.c, modo
// painel): remover, mais informacoes, assistido. O toque curto continua
// abrindo o titulo — mas agora na SOLTURA, e nao no KEYDOWN: so no KEYUP se
// sabe quanto o dedo ficou. E a mesma medida da home e da Agenda (NV_HOLD_MS),
// e o menu abre NO LIMIAR, com o dedo ainda no botao, como na home: esperar a
// soltura deixaria a barra cheia na tela sem nada acontecer.
// 0 = nenhum OK afundado numa linha. So a aba Salvos arma; as outras abas
// continuam decidindo no KEYDOWN, porque nelas nao ha o que segurar.
static Uint32 okDesde;
// O FOCO SEGUE A REMOCAO. Ao abrir o menu, o painel guarda o titulo e o que vem
// logo depois dele; quando a lista remontar sem o titulo, o foco vai para o
// seguinte — e nao para "o mesmo indice", que depois de uma remocao pelo Trakt
// (a linha local sai antes, a copia do catalogo so no 2xx) apontaria para
// outra coisa no meio do caminho.
static char   menuId[24], menuProximo[24];
// O FOCO SEGUE O TITULO MOVIDO. Mover para uma categoria (com a lista agrupada
// por categoria) muda a posicao dele; o foco vai junto, na reconstrucao.
static char   seguirId[24];

int spainel_aberto(void)  { return aberto; }
static int abaExiste(int a);
static void trocarAba(int nova);
static int nVisiveis(void);
int spainel_aba_atual(void) { return aba; }
int spainel_foco_indice(void) { return foco; }
void spainel_ir_aba(int a) {
  if (!aberto) spainel_abrir();
  if (a < SP_ABA_SALVOS || a >= SP_ABA_N || !abaExiste(a)) return;
  if (a != aba) trocarAba(a);
  foco = nVisiveis() > 0 ? 0 : SP_FOCO_ABAS;
}
int spainel_pediu_agenda(void) {
  if (!temPedidoAgenda) return 0;
  temPedidoAgenda = 0;
  return 1;
}
int spainel_visivel(void) { return aberto || entrada > 0.002f; }

const char *spainel_pediu_abrir(void) {
  if (!temPedido) return NULL;
  temPedido = 0;
  return pedido;
}

static int ehSerie(const char *tipo, int nTemporadas) {
  return (tipo && !strcmp(tipo, "series")) || nTemporadas > 0;
}

// O QUE A LISTA MONTADA VIU: as revisoes do catalogo e da lista local no
// instante da reconstrucao. Reconstruir quando uma delas sobe, e so entao.
//
// ERA `cat_n() != marcaCatN || catTrocou()` — contagem e o id do item 0. O
// retrato tinha dois defeitos opostos: nao via mudanca de MARCA (um titulo
// salvo pela conta com o painel aberto nao aparecia) e, quando disparava, a
// reconstrucao andava pelo catalogo inteiro. Com as revisoes a pergunta por
// quadro e comparar dois inteiros, e a reconstrucao so acontece quando ha o
// que mostrar de diferente (tests/salvospainel.sh conta quantas vezes).
static unsigned marcaRevCat, marcaRevSalvos, marcaRevOrg, marcaRevRet;
static int reconstrucoes, fundosPintados;
int spainel_n_reconstrucoes(void) { return reconstrucoes; }
int spainel_n_fundos(void) { return fundosPintados; }
static int listaVelha(void) {
  sorg_carregar();   // troca de perfil sobe a revisao da organizacao
  return marcaCatN < 0 || sorg_revisao() != marcaRevOrg || cat_revisao_itens() != marcaRevCat ||
         salvos_revisao() != marcaRevSalvos || cat_n() != marcaCatN ||
         cw_retido_rev() != marcaRevRet;
}

// Monta a lista visivel. A uniao (lista local + catalogo) vem de salvos_uniao,
// e a reordenacao e daqui:
//   1. a lista LOCAL, na ordem de insercao (ela existe mesmo sem catalogo);
//   2. cada TITULO que o catalogo tem marcado como naLista e ainda nao entrou;
//   3. os itens COM progresso sobem para o topo, virando a secao "Continuar".
// A reordenacao e uma insercao estavel: dentro de cada secao a ordem das duas
// passadas e preservada, senao a lista dancaria a cada reconstrucao.
//
// A DEDUPLICACAO NAO E MAIS DAQUI. Ela era um strcmp dos ids ja postos
// (`jaTem`), e o catalogo guarda a serie com progresso como "tt123:1:2" ao lado
// do "tt123" da lista local e da conta: Widows Bay aparecia duas vezes, as duas
// no mesmo episodio, porque a linha local puxava o progresso daquela copia e a
// copia entrava de novo por conta propria. salvos_uniao compara por titulo, e e
// a mesma regra que tests/salvos.sh cobra.
static void legendasDaBarra(SPLinha *l);
static int tipoGrupo(const char *tipo, const char *id, int serie);
static int anoDe(const char *meta);
static void organizar(void);
// O QUE ACABOU DE SER VISTO ENTRA EM "CONTINUAR" (dono, 03/10: "as coisas que
// acabei de ver nao tao aparecendo no social, na parte saved; tem que aparecer
// ele com a barra e o tempo"). A uniao (salvos_uniao) so leva o que esta
// SALVO — a lista local e o que a conta marcou naLista —, entao um filme
// comecado sem "+" nunca virava linha, e um salvo da conta cujo progresso so
// existe na copia da fileira Continuar assistindo (sem a marca) ficava sem
// barra. Aqui a fileira "continue_watching" do catalogo, que e onde o progresso
// local, o da conta e o do Trakt ja chegam juntos (descoberta.c), completa a
// lista: titulo novo vira linha com a barra e o tempo; titulo ja listado sem
// progresso ganha o da fileira. So itens com progresso: o "proximo episodio"
// sem nada visto e assunto da home, nao de "Continuar".
static void preencherDoCat(SPLinha *l, const CatItem *c) {
  l->progresso = c->progresso;
  l->temporada = c->temporada;
  l->episodio  = c->episodio;
  l->restanteMin = c->restanteMin;
  if (c->nota > 0) l->nota = c->nota;
  if (c->poster[0]) snprintf(l->poster, sizeof l->poster, "%s", c->poster);
  if (c->meta[0])   snprintf(l->meta, sizeof l->meta, "%s", c->meta);
  if (ehSerie(c->tipo, c->nTemporadas)) l->serie = 1;
  if (c->backdrop[0]) snprintf(l->fundo, sizeof l->fundo, "%s", c->backdrop);
}
static void vistosAgora(void) {
  int r, nf = cat_n_fileiras(), base = nLinhas;
  for (r = 0; r < nf; r++) {
    const CatFileira *f = cat_fileira(r);
    int k;
    if (!f || strcmp(f->chave, "continue_watching")) continue;
    for (k = f->ini; k < f->ini + f->n; k++) {
      const CatItem *c = cat_item(k);
      int j, achou = -1;
      SPLinha *l;
      if (!c || !c->imdb[0] || c->progresso <= 0) continue;
      // UM LUGAR SO (cwretido.h): o que o cartao da ilha / a faixa "Retomar
      // agora" segura so vira linha daqui quando sair de la.
      if (cw_retido_exclui(c->imdb)) continue;
      for (j = 0; j < nLinhas && achou < 0; j++)
        if (salvos_mesmo_titulo(linhas[j].id, c->imdb)) achou = j;
      if (achou >= 0) {
        // Ja listado: so completa a barra de quem nao tinha (os acrescentados
        // aqui ja vieram com a copia mais recente da fileira).
        if (achou < base && linhas[achou].progresso <= 0) preencherDoCat(&linhas[achou], c);
        continue;
      }
      if (nLinhas >= SP_MAX || !garantirLinhas(nLinhas + 1)) return;
      l = &linhas[nLinhas++];
      memset(l, 0, sizeof *l);
      snprintf(l->id, sizeof l->id, "%s", c->imdb);
      snprintf(l->titulo, sizeof l->titulo, "%s", c->titulo);
      preencherDoCat(l, c);
      l->tipoG = tipoGrupo(c->tipo, l->id, l->serie);
      l->ano = anoDe(l->meta);
      l->cat = sorg_categoria_de(l->id);
    }
    break;
  }
}

static void reconstruir(void) {
  static SalvosEntrada *uniao;
  static int capUniao;
  int i, n, escrita = 0;
  // As revisoes sao lidas ANTES de ler a lista: se a descoberta mudar o
  // catalogo no meio desta reconstrucao, a revisao guardada fica velha e o
  // quadro seguinte reconstroi de novo, em vez de guardar a nova e perder a
  // mudanca.
  unsigned revCat = cat_revisao_itens(), revSalvos = salvos_revisao();
  int catN = cat_n();
  reconstrucoes++;
  nLinhas = 0;
  n = salvos_n() + cat_n();
  if (n > SP_MAX) n = SP_MAX;
  if (n > capUniao) {
    SalvosEntrada *novo = (SalvosEntrada *)realloc(uniao, sizeof *uniao * (size_t)n);
    if (novo) { uniao = novo; capUniao = n; }
  }
  n = salvos_uniao(uniao, capUniao);
  for (i = 0; i < n && nLinhas < SP_MAX; i++) {
    const SalvoItem *s = uniao[i].local >= 0 ? salvos_item(uniao[i].local) : NULL;
    const CatItem *c = uniao[i].cat >= 0 ? cat_item(uniao[i].cat) : NULL;
    SPLinha *l;
    if (!s && !c) continue;
    if (!garantirLinhas(nLinhas + 1)) break;
    l = &linhas[nLinhas++];
    memset(l, 0, sizeof *l);
    if (s) {
      snprintf(l->id, sizeof l->id, "%s", s->id);
      snprintf(l->titulo, sizeof l->titulo, "%s", s->titulo);
      snprintf(l->poster, sizeof l->poster, "%s", s->poster);
      snprintf(l->meta, sizeof l->meta, "%s", s->meta);
      l->nota   = s->nota;
      l->quandoS = s->quandoS;
      l->serie  = ehSerie(s->tipo, 0);
    } else {
      snprintf(l->id, sizeof l->id, "%s", c->imdb);
      snprintf(l->titulo, sizeof l->titulo, "%s", c->titulo);
      snprintf(l->poster, sizeof l->poster, "%s", c->poster);
      snprintf(l->meta, sizeof l->meta, "%s", c->meta);
      l->nota   = c->nota;
    }
    // O PROGRESSO SO EXISTE NO CATALOGO. A lista local guarda o que e dela
    // (titulo, poster, quando entrou); posicao de retomada e de progresso.c e
    // muda sem passar por aqui. Guardar uma copia envelheceria em minutos.
    if (c) preencherDoCat(l, c);
    l->tipoG = tipoGrupo(s ? s->tipo : c->tipo, l->id, l->serie);
    l->ano = anoDe(l->meta);
    l->cat = sorg_categoria_de(l->id);
  }
  vistosAgora();
  for (i = 0; i < nLinhas; i++) { legendasDaBarra(&linhas[i]); linhas[i].orig = i; }
  // A ORDEM DE SEMPRE era: a uniao na ordem acima e, por cima, quem tem
  // progresso subindo para "Continuar". Agora a ordem e o agrupamento sao da
  // pessoa (salvosorg.h) — e o padrao dos dois reproduz exatamente aquilo.
  for (i = 0; i < nLinhas; i++) if (linhas[i].progresso > 0) escrita++;
  nCont = escrita;
  organizar();
  marcaRevOrg = sorg_revisao();
  marcaCatN = catN;
  marcaRevCat = revCat;
  marcaRevSalvos = revSalvos;
  marcaRevRet = cw_retido_rev();
  // O FOCO DAS ABAS (-1) NAO E UM FOCO FORA DA FAIXA. Sem esta guarda, uma
  // reconstrucao com a lista vazia jogaria o foco de volta para a linha 0, que
  // nao existe, e a linha de abas perderia o anel debaixo do dedo.
  if (menuId[0] && foco >= 0) {
    // Com o menu do painel no ar (ou acabando de sair), o foco vai por
    // IDENTIDADE: fica no titulo enquanto ele existir, e cai no seguinte quando
    // ele sair. A animacao de foco nao e zerada — a linha que chega ao lugar
    // acende pela mola, sem piscar.
    int achou = -1, prox = -1;
    for (i = 0; i < nLinhas; i++) {
      if (achou < 0 && salvos_mesmo_titulo(linhas[i].id, menuId)) achou = i;
      if (prox < 0 && menuProximo[0] && salvos_mesmo_titulo(linhas[i].id, menuProximo)) prox = i;
    }
    if (achou >= 0) foco = achou;
    else if (prox >= 0) foco = prox;
  }
  if (seguirId[0] && foco >= 0) {
    for (i = 0; i < nLinhas; i++)
      if (salvos_mesmo_titulo(linhas[i].id, seguirId)) { foco = i; break; }
    seguirId[0] = 0;
  }
  if (foco >= 0 && foco >= nLinhas) foco = nLinhas > 0 ? nLinhas - 1 : 0;
}

// --- ORGANIZAR: grupos, ordem e o lugar de cada celula ------------------------
//
// Tudo isto roda NA RECONSTRUCAO, e so nela: a lista muda quando o catalogo, a
// lista local ou a organizacao mudam (as tres revisoes de listaVelha), e entre
// uma mudanca e outra o quadro so le `ly`/`lh` prontos. Com 300 salvos a conta
// inteira e uma ordenacao de 300 indices — nada que apareca num quadro.

// Secoes da lista montada: o rotulo e onde ele mora. `vazia` = categoria sem
// titulo nenhum, que ainda assim aparece (com uma dica no lugar das celulas):
// quem acabou de criar "Kids" tem de ver "Kids" na tela.
#define SP_SECOES_MAX (SORG_CAT_MAX + 8)
typedef struct { float y; char rot[SORG_NOME_MAX + 8]; int vazia; } SPSecao;
static SPSecao secoes[SP_SECOES_MAX];
static int nSecoes;
static float alturaConteudo;

// As medidas dos tres estilos. A coluna util e SP_INTERNO (696).
//   LISTA     a de sempre: cartaz 92x138 e texto ao lado, passo SP_PASSO.
//   GRADE     4 cartazes por fila, 159x238, titulo embaixo. 4 e nao 5: a 3 m,
//             um cartaz de 130 px ja nao deixa ler o titulo de baixo.
//   PAISAGEM  2 cartoes 16:9 por fila, 338x190, titulo e linha de apoio.
#define SPG_COLS        4
#define SPG_VAO        20.0f
#define SPG_W         ((SP_INTERNO - SPG_VAO * (SPG_COLS - 1)) / SPG_COLS)
#define SPG_POSTER_H  (SPG_W * 1.5f)
#define SPG_H         (SPG_POSTER_H + 48.0f)
#define SPG_PASSO     (SPG_H + 24.0f)
#define SPP_COLS        2
#define SPP_VAO        20.0f
#define SPP_W         ((SP_INTERNO - SPP_VAO * (SPP_COLS - 1)) / SPP_COLS)
#define SPP_IMG_H     (SPP_W * 9.0f / 16.0f)
#define SPP_H         (SPP_IMG_H + 82.0f)
#define SPP_PASSO     (SPP_H + 24.0f)
// Altura da dica de uma categoria vazia, abaixo do rotulo.
#define SP_VAZIA_H     64.0f

// O que o tipo do catalogo diz, reduzido aos quatro grupos que a pessoa ve.
// Canal: os addons de TV declaram "tv" ou "channel", e o id de canal do app e
// "cs:channel:..." (ver idbase.h). Colecao: o tipo pode chegar cortado em 8
// bytes ("collect"), entao basta o prefixo.
static int tipoGrupo(const char *tipo, const char *id, int serie) {
  if (tipo && (!strcmp(tipo, "tv") || !strcmp(tipo, "channel"))) return SPT_CANAL;
  if (id && !strncmp(id, "cs:", 3)) return SPT_CANAL;
  if (tipo && !strncmp(tipo, "coll", 4)) return SPT_COLECAO;
  if (serie) return SPT_SERIE;
  return SPT_FILME;
}

// O ano da meta ("2002", "2022 · 3 temporadas"): o primeiro numero de quatro
// digitos entre 1900 e 2099. 0 quando a meta nao traz ano.
static int anoDe(const char *meta) {
  const char *p;
  if (!meta) return 0;
  for (p = meta; *p; p++) {
    if (p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9' &&
        !(p[4] >= '0' && p[4] <= '9') && (p == meta || !(p[-1] >= '0' && p[-1] <= '9'))) {
      int a = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
      if (a >= 1900 && a <= 2099) return a;
    }
  }
  return 0;
}

// Em que secao a linha cai, no agrupamento atual. Numero menor = mais acima.
static int grupoDe(const SPLinha *l, int g) {
  if (g == SORG_GRUPO_PROGRESSO) return l->progresso > 0 ? 0 : 1;
  if (g == SORG_GRUPO_TIPO) return l->tipoG;
  if (g == SORG_GRUPO_CATEGORIA) {
    int k = l->cat ? sorg_categoria_indice(l->cat) : -1;
    return k >= 0 ? k : SORG_CAT_MAX;   // "Sem categoria" fecha a lista
  }
  return 0;
}

// Quanto falta, para "Menos tempo restante": o minuto quando ha, senao o
// percentual que falta (escala alta, para cair depois de quem tem minuto).
static int restanteDe(const SPLinha *l) {
  if (l->progresso <= 0) return 1 << 30;
  if (l->restanteMin > 0) return l->restanteMin;
  return 100000 + (100 - l->progresso);
}

static int ordemAtual, grupoAtual;
static int compara(const void *pa, const void *pb) {
  const SPLinha *a = &linhas[*(const int *)pa], *b = &linhas[*(const int *)pb];
  int ga = grupoDe(a, grupoAtual), gb = grupoDe(b, grupoAtual), d = 0;
  if (ga != gb) return ga - gb;
  switch (ordemAtual) {
    case SORG_ORDEM_RECENTES:
      // Do Trakt ou da conta nao sabemos quando entrou (quandoS 0): depois.
      d = (b->quandoS > a->quandoS) - (b->quandoS < a->quandoS);
      break;
    case SORG_ORDEM_NOME: d = strcasecmp(a->titulo, b->titulo); break;
    case SORG_ORDEM_ANO:  d = b->ano - a->ano; break;
    case SORG_ORDEM_NOTA: d = b->nota - a->nota; break;
    case SORG_ORDEM_RESTANTE: {
      int ra = restanteDe(a), rb = restanteDe(b);
      d = (ra > rb) - (ra < rb);
      break; }
    default: break;
  }
  // O DESEMPATE E A ORDEM DE SEMPRE: qsort nao e estavel, e sem isto dois
  // titulos sem ano trocariam de lugar a cada reconstrucao.
  return d ? d : a->orig - b->orig;
}

// Rotulo da secao `g` no agrupamento atual. Passa por i18n no desenho
// (txt_linha), exceto o nome de categoria, que e da pessoa.
static void rotuloGrupo(int g, char *dst, size_t tam) {
  static const char *tipos[SPT_N] = { "Filmes", "Séries", "Coleções", "Canais" };
  if (grupoAtual == SORG_GRUPO_PROGRESSO)
    snprintf(dst, tam, "%s", g == 0 ? "Continuar" : (nCont > 0 ? "Não começados" : "Sua lista"));
  else if (grupoAtual == SORG_GRUPO_TIPO)
    snprintf(dst, tam, "%s", g >= 0 && g < SPT_N ? tipos[g] : "Sua lista");
  else if (grupoAtual == SORG_GRUPO_CATEGORIA) {
    const char *n = g < SORG_CAT_MAX ? sorg_categoria_nome_id(sorg_categoria_id(g)) : NULL;
    snprintf(dst, tam, "%s", n ? n : (sorg_n_categorias() > 0 ? "Sem categoria" : "Sua lista"));
  } else snprintf(dst, tam, "%s", "Sua lista");
}

static int novaSecao(float y, int g, int vazia) {
  SPSecao *sc;
  if (nSecoes >= SP_SECOES_MAX) return 0;
  sc = &secoes[nSecoes++];
  sc->y = y;
  sc->vazia = vazia;
  rotuloGrupo(g, sc->rot, sizeof sc->rot);
  return 1;
}

static int expIdx(void);
static float expExtra(void);
static float expExtraBloco(void);
// Poe cada linha no lugar: secoes, filas, colunas.
static void montarLayout(void) {
  int estilo = sorg_estilo(), cols = 1, i, col = 0, fila = -1, gAnt = -999;
  int catVazias = grupoAtual == SORG_GRUPO_CATEGORIA;
  int proximaCat = 0;   // categorias vazias entram na ordem, entre as cheias
  float w = SP_INTERNO, h = SP_LINHA_SALVO, passo = SP_PASSO, vao = 0.0f, y = 0.0f;
  if (estilo == SORG_ESTILO_GRADE) { cols = SPG_COLS; w = SPG_W; h = SPG_H; passo = SPG_PASSO; vao = SPG_VAO; }
  else if (estilo == SORG_ESTILO_PAISAGEM) { cols = SPP_COLS; w = SPP_W; h = SPP_H; passo = SPP_PASSO; vao = SPP_VAO; }
  nSecoes = 0;
  for (i = 0; i < nLinhas; i++) {
    SPLinha *l = &linhas[i];
    int g = grupoDe(l, grupoAtual);
    if (g != gAnt) {
      if (col > 0) { y += passo; col = 0; }
      // As categorias criadas e ainda vazias que vem antes desta secao.
      if (catVazias)
        for (; proximaCat < sorg_n_categorias() && proximaCat < g; proximaCat++) {
          novaSecao(y, proximaCat, 1);
          y += (y <= 0.0f ? SP_SECAO_H1 : SP_SECAO_H) + SP_VAZIA_H;
        }
      if (catVazias && g < SORG_CAT_MAX) proximaCat = g + 1;
      novaSecao(y, g, 0);
      y += y <= 0.0f ? SP_SECAO_H1 : SP_SECAO_H;
      gAnt = g;
    }
    if (col == 0) fila++;
    l->lx = (float)col * (w + vao);
    l->ly = y;
    l->lw = w;
    l->lh = h;
    l->fila = fila;
    if (++col >= cols) { col = 0; y += passo; }
  }
  if (col > 0) y += passo;
  if (catVazias)
    for (; proximaCat < sorg_n_categorias(); proximaCat++) {
      novaSecao(y, proximaCat, 1);
      y += (y <= 0.0f ? SP_SECAO_H1 : SP_SECAO_H) + SP_VAZIA_H;
    }
  alturaConteudo = y;
}

static void organizar(void) {
  static int *idx;
  static int capIdx;
  SPLinha *tmp;
  int i;
  ordemAtual = sorg_ordem();
  grupoAtual = sorg_grupo();
  // "Por categoria" sem nenhuma categoria criada e o mesmo que nenhum grupo:
  // uma secao "Sem categoria" sozinha nao diz nada.
  if (grupoAtual == SORG_GRUPO_CATEGORIA && sorg_n_categorias() < 1) grupoAtual = SORG_GRUPO_NENHUM;
  if (nLinhas > 1 && (ordemAtual != SORG_ORDEM_SALVOU || grupoAtual != SORG_GRUPO_NENHUM)) {
    if (nLinhas > capIdx) {
      int *n = (int *)realloc(idx, sizeof *idx * (size_t)nLinhas);
      if (n) { idx = n; capIdx = nLinhas; }
    }
    tmp = nLinhas <= capIdx ? (SPLinha *)malloc(sizeof *tmp * (size_t)nLinhas) : NULL;
    if (tmp) {
      for (i = 0; i < nLinhas; i++) idx[i] = i;
      qsort(idx, (size_t)nLinhas, sizeof *idx, compara);
      for (i = 0; i < nLinhas; i++) tmp[i] = linhas[idx[i]];
      memcpy(linhas, tmp, sizeof *tmp * (size_t)nLinhas);
      free(tmp);
    }
  }
  montarLayout();
}

// 1 quando o pacote tem o servico de recomendacoes. Com 0 nao ha aba, nao ha
// selo e nao ha uma linha de rede: o dono publica builds sem NUVIO_REC_URL.
static int temAbas(void) { return 1; }
static int temSocial(void) { return 0; }
static int temAtividade(void) { return 0; }
// A aba PODE existir neste pacote/estado (nao e o que a pessoa escolheu ver).
static int abaExiste(int a) {
  if (a == SP_ABA_SOCIAL) return temSocial();
  if (a == SP_ABA_ATIVIDADE) return temAtividade();
  return a >= SP_ABA_SALVOS && a < SP_ABA_N;
}

// AS ABAS DE CADA PERFIL (07/10, "Editar"): quais aparecem e em que ordem,
// guardado por perfil em salvos-abas-p<N>.txt, uma linha "aba<TAB>id<TAB>0|1"
// por aba, na ordem da faixa. Id de aba que o arquivo nao conhece (uma versao
// futura acrescentou uma) entra no fim, ligada; id repetido ou invalido e
// ignorado. O arquivo e por TV de proposito: perfil e quem decide, nao a conta.
static int abaOrdem[SP_ABA_N] = { 0, 1, 2, 3, 4 };
static unsigned char abaLiga[SP_ABA_N] = { 1, 1, 1, 1, 1 };
static int abaPerfil = -1;
static const char *abasArquivo(void) {
  static char nome[40];
  int p = perfis_ativo();
  snprintf(nome, sizeof nome, "salvos-abas-p%d.txt", p > 0 ? p : 1);
  return nome;
}
static void abasPadrao(void) {
  int i;
  for (i = 0; i < SP_ABA_N; i++) { abaOrdem[i] = i; abaLiga[i] = 1; }
}
static void abasCarregar(void) {
  int p = perfis_ativo(), n = 0, i;
  int visto[SP_ABA_N] = { 0 };
  char *b, *linha, *prox;
  if (p == abaPerfil) return;
  abaPerfil = p;
  abasPadrao();
  b = dados_ler(abasArquivo());
  if (!b) return;
  for (linha = b; linha && *linha; linha = prox) {
    char *fim = strchr(linha, '\n');
    int id, on;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (sscanf(linha, "aba\t%d\t%d", &id, &on) != 2) continue;
    if (id < 0 || id >= SP_ABA_N || visto[id]) continue;
    visto[id] = 1;
    abaOrdem[n++] = id;
    abaLiga[id] = on != 0;
  }
  for (i = 0; i < SP_ABA_N; i++)
    if (!visto[i]) { abaOrdem[n++] = i; abaLiga[i] = 1; }
  free(b);
}
static void abasGravar(void) {
  char b[256];
  size_t k = (size_t)snprintf(b, sizeof b, "# nuvio salvos-abas v1\n");
  int i;
  for (i = 0; i < SP_ABA_N; i++)
    k += (size_t)snprintf(b + k, sizeof b - k, "aba\t%d\t%d\n", abaOrdem[i], abaLiga[abaOrdem[i]] ? 1 : 0);
  dados_gravar(abasArquivo(), b);
}
// Ligada: existe e a pessoa quer ver.
static int abaLigada(int a) { return a >= 0 && a < SP_ABA_N && abaExiste(a) && abaLiga[a]; }
static int algumaLigada(void) {
  int i;
  for (i = 0; i < SP_ABA_N; i++) if (abaLigada(i)) return 1;
  return 0;
}
// Na faixa: ligada, a aba aberta (uma notificacao pode abrir uma que a pessoa
// escondeu, e a faixa nao pode perder a aba em que ela esta) ou, se nada esta
// ligado (a social caiu e era tudo o que havia), Salvos.
static int abaNaFaixa(int a) {
  return abaLigada(a) || (a == aba && abaExiste(a)) || (!algumaLigada() && a == SP_ABA_SALVOS);
}
static int abaPos(int a) {
  int i;
  for (i = 0; i < SP_ABA_N; i++) if (abaOrdem[i] == a) return i;
  return 0;
}
static int proximaAba(int de, int dir) {
  int i = abaPos(de) + dir;
  for (; i >= 0 && i < SP_ABA_N; i += dir)
    if (abaNaFaixa(abaOrdem[i])) return abaOrdem[i];
  return de;
}
static int primeiraAba(void) {
  int i;
  for (i = 0; i < SP_ABA_N; i++) if (abaLigada(abaOrdem[i])) return abaOrdem[i];
  return SP_ABA_SALVOS;
}

static int nVisiveis(void) {
  if (aba == SP_ABA_AVISOS) return avisos_lista_linhas();   // + "Dispensar todos"
  if (aba == SP_ABA_AGENDA) return nAg + 1;   // + "Abrir a agenda completa"
  return nLinhas;
}

// A BARRA DE OPCOES (Ordenar, Agrupar, Estilo, categorias) mora entre as abas
// e a lista, e empurra a lista SP_OPC_EXTRA para baixo so quando existe.
#define SP_FOCO_BARRA  (-2)
#define SP_OPC_Y     (SP_ABAS_Y + SP_ABAS_H + 14.0f)
#define SP_OPC_H      NV_CTRL_H
#define SP_OPC_EXTRA (SP_OPC_Y + SP_OPC_H + 12.0f - SP_LISTA_Y)
static int temBarra(void);
static float listaTopo(void) { return SP_LISTA_Y + (temBarra() ? SP_OPC_EXTRA : 0.0f); }



static float agAntes(int i) {
  if (i < 0 || i >= nAg) return 0.0f;
  if (i == 0 || agendaui_painel_grupo_de(agenda_lista(agIdx[i])) !=
                agendaui_painel_grupo_de(agenda_lista(agIdx[i - 1])))
    return 55.0f;   // 51 do cabecalho de grupo + 4, como na tela Agenda
  return 0.0f;
}

static void reconstruirAgenda(void) {
  int i, n;
  agenda_montar();
  n = agenda_n();
  nAg = 0;
  for (i = 0; i < n && nAg < SPAG_MAX; i++) {
    const AgItem *it = agenda_lista(i);
    int d;
    if (!it || !it->dataProx[0]) continue;
    d = agenda_dias(it->dataProx);
    if (d == AG_SEM_DATA || d < 0) continue;
    agIdx[nAg++] = i;
  }
  agVer = agenda_versao();
}

static const char *rotuloAba(int i);
// O titulo que o cabecalho mostra agora (a aba aberta, ou a folha Editar).
static const char *tituloAgora(void) { return editando ? i18n("Editar abas") : i18n(rotuloAba(aba)); }
static int posNaFaixa(int id) {
  int i;
  for (i = 0; i < SP_ABA_N; i++) if (abaOrdem[i] == id) return i;
  return 0;
}
// Arma a transicao: guarda o titulo de agora (sai) e recomeca a mola do corpo.
// So com o painel de pe; aberto de vez nao ha o que animar.
static void trocaIniciar(int dir) {
  if (!aberto || entrada < 0.9f) { trocaT = 1.0f; trocaV = 0.0f; return; }
  if (trocaT < 0.02f) return;   // ja armada neste quadro (Editar -> outra aba)
  snprintf(tituloAnt, sizeof tituloAnt, "%s", tituloAgora());
  trocaDir = dir < 0 ? -1 : 1; trocaT = 0.0f; trocaV = 0.0f;
}
static void trocarAba(int nova) {
  if (!temAbas() || nova == aba) return;
  trocaIniciar(posNaFaixa(nova) >= posNaFaixa(aba) ? 1 : -1);
  if (aba == SP_ABA_AVISOS) avisos_marcar_lidos();
  aba = nova;
  foco = SP_FOCO_ABAS;
  scrollY = 0.0f; velY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
  editLapis = 0;
  if (aba == SP_ABA_AGENDA) { agenda_atualizar_seguidas(); reconstruirAgenda(); }
}

// --- BARRA DE OPCOES, ESCOLHAS E CATEGORIAS -------------------------------
//
// O CONTROLE REMOTO DECIDE O DESENHO. Quatro pilulas numa fila so, cada uma
// dizendo o que vale agora ("Ordenar / Recentes"): o D-pad chega nelas subindo
// da lista, e o OK abre uma escolha curta por cima do painel — a lista de
// opcoes com a atual marcada. Nada de menu dentro de menu para ordenar: duas
// teclas e o efeito ja esta na lista, embaixo.
//
// A ESCOLHA ("pop") E UMA SO, para tudo: ordenar, agrupar, estilo, a lista de
// categorias, o que fazer com uma, a confirmacao de excluir e o "Mover para"
// que vem do menu do cartaz. Uma maquina so para sete usos, pela mesma razao
// do teclado.h: duas que fazem a mesma coisa divergem na primeira correcao.
enum { SPB_ORDEM = 0, SPB_GRUPO, SPB_ESTILO, SPB_CATEG, SPB_N };
static int   barraFoco;
static float animBarra[SPB_N];

static const char *ORDEM_CURTO[SORG_ORDEM_N] = {
  "Mais antigos", "Recentes", "Nome", "Ano", "Nota", "Restante" };
static const char *ORDEM_LONGO[SORG_ORDEM_N] = {
  "Salvos mais antigos primeiro", "Salvos mais recentes primeiro", "Nome (A–Z)",
  "Ano (mais novo primeiro)", "Nota do IMDb", "Menos tempo restante" };
static const char *GRUPO_CURTO[SORG_GRUPO_N] = {
  "Progresso", "Tipo", "Categoria", "Nenhum" };
static const char *GRUPO_LONGO[SORG_GRUPO_N] = {
  "Continuar e não começados", "Tipo: filmes, séries, coleções, canais",
  "Minhas categorias", "Sem agrupar" };
static const char *ESTILO_CURTO[SORG_ESTILO_N] = { "Lista", "Grade", "Paisagem" };
static const char *ESTILO_LONGO[SORG_ESTILO_N] = {
  "Lista com capa", "Grade de pôsteres", "Cartões paisagem" };
static const char *ESTILO_ICONE[SORG_ESTILO_N] = {
  "aj_rows-3", "aj_layout-dashboard", "aj_images" };
static int temBarra(void) {
  if (aba == SP_ABA_SALVOS) return nLinhas > 0;
  return 0;
}
static int nChips(void) { return SPB_N; }

enum { POP_NADA = 0, POP_ORDEM, POP_GRUPO, POP_ESTILO, POP_CATS,
       POP_CAT_ACOES, POP_EXCLUIR, POP_MOVER };
#define POP_MAX (SORG_CAT_MAX + 4)
#define POP_LINHA_H   64.0f
#define POP_LINHA_VAO  8.0f
#define POP_VISIVEIS   8
typedef struct {
  char rot[SORG_NOME_MAX + 40];
  const char *icone;
  int  valor, marcado;
} PopLinha;
static PopLinha popL[POP_MAX];
static int   pop, popN, popFoco, popCat;
static float popEntrada, popRol, popAnim[POP_MAX];
static char  popTitulo[200], popSub[200], popItem[24];

// O TECLADO DO APP (teclado.h) para nome de categoria: letras, numeros,
// espaco e hifen. A primeira letra sai maiuscula (sorg_nome_limpo).
enum { TK_NADA = 0, TK_CRIAR, TK_MOVER, TK_RENOMEAR };
static int tecladoPara;
static const char *ALFA_NOME = "abcdefghijklmnopqrstuvwxyz0123456789 -";
#define SP_NOME_LETRAS 24

static void popLinha(const char *rot, const char *icone, int valor, int marcado) {
  PopLinha *l;
  if (popN >= POP_MAX) return;
  l = &popL[popN++];
  snprintf(l->rot, sizeof l->rot, "%s", rot);
  l->icone = icone;
  l->valor = valor;
  l->marcado = marcado;
}

// Quantos titulos da lista montada estao na categoria `id`.
static int contaNaCategoria(int id) {
  int i, n = 0;
  for (i = 0; i < nLinhas; i++) if (linhas[i].cat == id) n++;
  return n;
}

static void popAbrir(int tipo) {
  int i, k;
  char b[SORG_NOME_MAX + 40];
  pop = tipo;
  popN = 0;
  popTitulo[0] = popSub[0] = 0;
  switch (tipo) {
    case POP_ORDEM:
      snprintf(popTitulo, sizeof popTitulo, "%s", "Ordenar por");
      for (i = 0; i < SORG_ORDEM_N; i++) popLinha(ORDEM_LONGO[i], NULL, i, i == sorg_ordem());
      break;
    case POP_GRUPO:
      snprintf(popTitulo, sizeof popTitulo, "%s", "Agrupar por");
      for (i = 0; i < SORG_GRUPO_N; i++) popLinha(GRUPO_LONGO[i], NULL, i, i == sorg_grupo());
      break;
    case POP_ESTILO:
      snprintf(popTitulo, sizeof popTitulo, "%s", "Estilo de exibição");
      for (i = 0; i < SORG_ESTILO_N; i++)
        popLinha(ESTILO_LONGO[i], ESTILO_ICONE[i], i, i == sorg_estilo());
      break;
    case POP_CATS:
      snprintf(popTitulo, sizeof popTitulo, "%s", "Categorias");
      snprintf(popSub, sizeof popSub, "%s", "Segure OK num título para mover para uma categoria.");
      popLinha("Nova categoria", "mais", -1, 0);
      for (i = 0; i < sorg_n_categorias(); i++) {
        int id = sorg_categoria_id(i), n = contaNaCategoria(id);
        const char *nome = sorg_categoria_nome_id(id);
        snprintf(b, sizeof b, "%s  ·  %d", nome ? nome : "", n);
        popLinha(b, "aj_folders", id, 0);
      }
      break;
    case POP_CAT_ACOES:
      { const char *nome = sorg_categoria_nome_id(popCat);
        snprintf(popTitulo, sizeof popTitulo, "%s", nome ? nome : ""); }
      popLinha("Renomear", "aj_keyboard", 1, 0);
      popLinha("Excluir categoria", NULL, 2, 0);
      break;
    case POP_EXCLUIR:
      { const char *nome = sorg_categoria_nome_id(popCat);
        snprintf(popTitulo, sizeof popTitulo, i18n("Excluir \xe2\x80\x9c%s\xe2\x80\x9d?"), nome ? nome : ""); }
      snprintf(popSub, sizeof popSub, "%s", "Os títulos continuam nos Salvos, só saem da categoria.");
      popLinha("Cancelar", NULL, 0, 0);
      popLinha("Excluir", NULL, 1, 0);
      break;
    case POP_MOVER: {
      int atual = sorg_categoria_de(popItem);
      const char *tit = "";
      for (i = 0; i < nLinhas; i++)
        if (salvos_mesmo_titulo(linhas[i].id, popItem)) { tit = linhas[i].titulo; break; }
      snprintf(popTitulo, sizeof popTitulo, "%s", "Mover para categoria");
      snprintf(popSub, sizeof popSub, "%s", tit);
      for (i = 0; i < sorg_n_categorias(); i++) {
        int id = sorg_categoria_id(i);
        const char *nome = sorg_categoria_nome_id(id);
        popLinha(nome ? nome : "", "aj_folders", id, id == atual);
      }
      popLinha("Sem categoria", NULL, 0, atual == 0);
      popLinha("Nova categoria", "mais", -1, 0);
      break; }
    default: pop = POP_NADA; return;
  }
  // O foco nasce na opcao que vale agora; sem nenhuma marcada, na primeira.
  popFoco = 0;
  for (k = 0; k < popN; k++) if (popL[k].marcado) { popFoco = k; break; }
  // "Excluir" nunca nasce com o foco: OK duas vezes sem ler cancela.
  if (tipo == POP_EXCLUIR) popFoco = 0;
  popRol = 0.0f;
  popEntrada = 0.0f;
  memset(popAnim, 0, sizeof popAnim);
}

static void tecladoNome(int para, const char *inicial) {
  tecladoPara = para;
  pop = POP_NADA;
  teclado_abrir_com(para == TK_RENOMEAR ? "Renomear categoria" : "Nova categoria",
                    "Ex.: Fim de semana, Kids. Até 24 letras.",
                    SP_NOME_LETRAS, ALFA_NOME, inicial);
}

// OK numa opcao da escolha aberta.
static void popOk(void) {
  int v;
  if (popFoco < 0 || popFoco >= popN) return;
  v = popL[popFoco].valor;
  switch (pop) {
    case POP_ORDEM:  sorg_definir_ordem(v);  pop = POP_NADA; break;
    case POP_GRUPO:  sorg_definir_grupo(v);  pop = POP_NADA; break;
    case POP_ESTILO:
      sorg_definir_estilo(v);
      pop = POP_NADA;
      // A lista trocou de forma: a rolagem recomeca de onde o foco esta.
      memset(animFoco, 0, sizeof animFoco);
      break;
    case POP_CATS:
      if (v < 0) tecladoNome(TK_CRIAR, NULL);
      else { popCat = v; popAbrir(POP_CAT_ACOES); }
      break;
    case POP_CAT_ACOES:
      if (v == 1) tecladoNome(TK_RENOMEAR, sorg_categoria_nome_id(popCat));
      else popAbrir(POP_EXCLUIR);
      break;
    case POP_EXCLUIR:
      if (v == 1) { sorg_excluir_categoria(popCat); pop = POP_NADA; }
      else popAbrir(POP_CATS);
      break;
    case POP_MOVER:
      if (v < 0) { tecladoNome(TK_MOVER, NULL); break; }
      sorg_mover(popItem, v);
      snprintf(seguirId, sizeof seguirId, "%s", popItem);
      pop = POP_NADA;
      break;
    default: pop = POP_NADA; break;
  }
}

static void popVoltar(void) {
  if (pop == POP_CAT_ACOES || pop == POP_EXCLUIR) popAbrir(POP_CATS);
  else pop = POP_NADA;
}

static void popEvento(const SDL_Event *e) {
  SDL_Keycode k;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || k == SDLK_LEFT || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    popVoltar(); return;
  }
  if (k == SDLK_DOWN) { if (popFoco + 1 < popN) popFoco++; return; }
  if (k == SDLK_UP)   { if (popFoco > 0) popFoco--; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (!e->key.repeat) popOk();
    return;
  }
}

// OK numa pilula da barra.
static void chipOk(void) {
  switch (barraFoco) {
    case SPB_ORDEM:  popAbrir(POP_ORDEM);  break;
    case SPB_GRUPO:  popAbrir(POP_GRUPO);  break;
    case SPB_ESTILO: popAbrir(POP_ESTILO); break;
    default:
      // Sem nenhuma categoria a lista de categorias so teria "Nova": vai
      // direto ao teclado.
      if (sorg_n_categorias() < 1) tecladoNome(TK_CRIAR, NULL);
      else popAbrir(POP_CATS);
      break;
  }
}

// O que o teclado devolveu. Chamado por quadro enquanto ha pedido pendente.
static void tecladoResultado(void) {
  int r, id;
  const char *t;
  if (!tecladoPara || teclado_aberto()) return;
  r = teclado_resultado();
  if (r == TECLADO_NADA) return;
  t = teclado_texto();
  if (r == TECLADO_PRONTO && t && t[0]) {
    if (tecladoPara == TK_RENOMEAR) sorg_renomear_categoria(popCat, t);
    else if ((id = sorg_criar_categoria(t)) != 0) {
      if (tecladoPara == TK_MOVER) {
        sorg_mover(popItem, id);
        snprintf(seguirId, sizeof seguirId, "%s", popItem);
      }
      // QUEM CRIA A CATEGORIA PELA BARRA QUER VE-LA: a lista passa a ser
      // agrupada por categoria, e "Kids" aparece vazia, com a dica de como
      // por algo nela. Mover pelo menu do cartaz nao troca o agrupamento —
      // a linha ganha o selo da categoria, que basta como resposta.
      else sorg_definir_grupo(SORG_GRUPO_CATEGORIA);
    }
  }
  tecladoPara = TK_NADA;
}

// A celula da fila de cima ou de baixo mais perto, na horizontal, da focada.
// Na lista e a vizinha; na grade e a da mesma coluna (ou a ultima da fila,
// quando a fila de baixo e mais curta).
static int celulaVizinha(int de, int dir) {
  int i, melhor = -1, alvo;
  float cx, md = 1e9f;
  if (de < 0 || de >= nLinhas) return -1;
  alvo = linhas[de].fila + dir;
  cx = linhas[de].lx + linhas[de].lw * 0.5f;
  for (i = de + dir; i >= 0 && i < nLinhas; i += dir) {
    float d;
    if (dir > 0 ? linhas[i].fila > alvo : linhas[i].fila < alvo) break;
    if (linhas[i].fila != alvo) continue;
    d = linhas[i].lx + linhas[i].lw * 0.5f - cx;
    if (d < 0) d = -d;
    if (d < md) { md = d; melhor = i; }
  }
  return melhor;
}

// Pedido de "Mover para categoria" vindo do menu do cartaz.
static void popAbrirMover(const char *imdb) {
  snprintf(popItem, sizeof popItem, "%s", imdb);
  popAbrir(POP_MOVER);
}

// Nascer da ilha (salvospainel.h). `origem` e fixa durante a abertura; o
// destino da volta e renovado por quadro, porque a pilula pode ter mudado de
// largura (o cartao alternou) enquanto o painel estava aberto.
static int deIlha, destinoOk;
static GfxRect origem, destino;

void spainel_abrir_de(float x, float y, float w, float h) {
  if (aberto) return;
  spainel_abrir();
  deIlha = 1;
  origem = (GfxRect){ x, y, w, h };
}

void spainel_recolher_para(int ok, float x, float y, float w, float h) {
  destinoOk = ok;
  if (ok) destino = (GfxRect){ x, y, w, h };
}

int spainel_da_ilha(void) { return deIlha && spainel_visivel(); }

void spainel_abrir(void) {
  if (aberto) return;
  deIlha = 0;
  aberto = 1;
  foco = 0;
  okDesde = 0;
  menuId[0] = menuProximo[0] = 0;
  seguirId[0] = 0;
  pop = POP_NADA;
  barraFoco = 0;
  memset(animBarra, 0, sizeof animBarra);
  abasCarregar();
  editando = 0; editLapis = 0;
  aba = SP_ABA_SALVOS;
  scrollY = 0.0f; velY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
  reconstruir();
  reconstruirAgenda();
  // A PRIMEIRA ABA DA FAIXA DO PERFIL, nao sempre Salvos.
  { int primeira = primeiraAba();
    if (primeira != SP_ABA_SALVOS) {
      trocarAba(primeira);
      foco = nVisiveis() > 0 ? 0 : SP_FOCO_ABAS;
    } }
}

void spainel_fechar(void) {
  if (aberto && aba == SP_ABA_AVISOS) avisos_marcar_lidos();
  pop = POP_NADA;
  aberto = 0;
}

// Altura ate o TOPO da linha `i`, contando o cabecalho de cada secao. Nao e
// `i * SP_PASSO`: o rotulo "Não começados" empurra tudo que vem depois dele, e
// sem contar esse empurrao a rolagem para a linha focada erra por 54px — o
// suficiente para o card focado ficar meio escondido atras do cabecalho.
static float topoDe(int i) {
  float y;
  // A ABA SOCIAL SOMA LINHA A LINHA, e nao multiplica por um passo fixo: as
  // linhas dela tem quatro alturas diferentes. Era `i * SP_PASSO` enquanto
  // todas eram recomendacoes; com uma sugestao de 112px no meio, a multiplicacao
  // erraria a partir dali e a rolagem pararia o foco meio fora da janela.
  if (aba == SP_ABA_AVISOS) return avisos_lista_y(i, foco);
  if (aba == SP_ABA_AGENDA) {
    int k;
    float y = 0.0f;
    for (k = 0; k < i && k < nAg; k++) y += agAntes(k) + SPAG_H;
    if (i < nAg) return y + agAntes(i);
    return nAg > 0 ? y + 10.0f : SPAG_VAZIO_H;
  }
  // Rotulo da primeira secao, sempre; mais o de "Não começados" para quem vem
  // depois dele. Com nCont == 0 nao existe segunda secao — a unica que aparece
  // e "Sua lista", e o segundo termo tem de ser zero para todo mundo.
  // Salvos: o lugar que montarLayout ja calculou (secoes, filas da grade).
  return (i >= 0 && i < nLinhas) ? linhas[i].ly : 0.0f;
}

// Uma linha de Salvos em foco, ou seja, algo que o OK longo pode segurar.
static int linhaSeguravel(void) {
  return aba == SP_ABA_SALVOS && foco >= 0 && foco < nLinhas;
}

static int linhaSocialSeguravel(void) { return 0; }

// O QUE AS EXTRAS DO MENU SOCIAL FAZEM. O menu (ctxmenu.c) so devolve o
// indice; a acao e a linha ficam guardadas aqui ate ele responder.
enum { SPX_AV_DISPENSAR = 1, SPX_AV_LEMBRETE, SPX_AV_TODOS };
static int      menuSxAcao[CTX_EXTRAS_MAX];
static char     menuAvId[72], menuAvImdb[24];

// SEGURAR OK NUMA LINHA DA ABA AVISOS (06/10, "muito alerta sem dispensar"):
// o mesmo menu das outras abas, so com as acoes do aviso — Dispensar, Remover
// lembrete (estreia com o lembrete ligado) e Dispensar todos. O toque curto
// continua abrindo o alvo, agora decidido na soltura. Dispensar tira a linha e
// grava a chave por perfil (avisodisp.h); o lembrete da serie so sai com
// "Remover lembrete", que tambem dispensa o aviso daquele episodio.
// Depois de dispensar: o foco fica na linha que tomou o lugar, na ultima, ou
// sobe para as abas quando a lista esvaziou.
static void focoAvisosValido(void) {
  int k = avisos_lista_linhas();
  if (foco < 0) return;
  if (k <= 0) foco = temAbas() ? SP_FOCO_ABAS : 0;
  else if (foco >= k) foco = k - 1;
}
static int linhaAvisoSeguravel(void) {
  return aba == SP_ABA_AVISOS && foco >= 0 && foco < avisos_lista_n();
}
static void abrirMenuAvisos(void) {
  CatItem c;
  CtxExtra ex[CTX_EXTRAS_MAX];
  int n = 0;
  static const CtxExtra DISP = { "Dispensar", "aj_x", 0, NULL, NULL, NULL };
  static const CtxExtra LEMB = { "Remover lembrete", "aj_bell", 0, NULL, NULL, NULL };
  static const CtxExtra TODOS = { "Dispensar todos", "aj_x", 0, NULL, NULL, NULL };
  if (!linhaAvisoSeguravel()) return;
  memset(&c, 0, sizeof c);
  if (!avisos_lista_item(foco, menuAvId, sizeof menuAvId, c.titulo, sizeof c.titulo,
                         menuAvImdb, sizeof menuAvImdb)) return;
  menuSxAcao[n] = SPX_AV_DISPENSAR; ex[n++] = DISP;
  if (menuAvImdb[0]) { menuSxAcao[n] = SPX_AV_LEMBRETE; ex[n++] = LEMB; }
  menuSxAcao[n] = SPX_AV_TODOS; ex[n++] = TODOS;
  ctx_abrir_social(&c, ex, n);
}

static void abrirMenuSocial(void) {}

// A extra escolhida no menu social (por quadro, em spainel_atualizar).
static void extraSocial(int k) {
  if (k < 0 || k >= CTX_EXTRAS_MAX) return;
  switch (menuSxAcao[k]) {
    case SPX_AV_LEMBRETE:
      if (menuAvImdb[0] && agenda_lembrete(menuAvImdb) == 1) agenda_alternar_lembrete(menuAvImdb);
      /* fall through */
    case SPX_AV_DISPENSAR:
      if (menuAvId[0]) avisos_dispensar(menuAvId);
      focoAvisosValido();
      break;
    case SPX_AV_TODOS:
      avisos_dispensar_todos();
      focoAvisosValido();
      break;
    default: break;
  }
}

// Abre o menu do cartaz sobre a linha focada. A linha vira um CatItem com o
// que o painel sabe dela; o menu troca pela copia do catalogo quando ela
// existe (ver o modo painel em ctxmenu.c).
static void abrirMenu(void) {
  CatItem c;
  const SPLinha *l;
  if (aba == SP_ABA_AVISOS) { abrirMenuAvisos(); return; }
  if (!linhaSeguravel()) return;
  l = &linhas[foco];
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "%s", l->id);
  snprintf(c.tipo, sizeof c.tipo, "%s", l->serie ? "series" : "movie");
  snprintf(c.titulo, sizeof c.titulo, "%s", l->titulo);
  snprintf(c.poster, sizeof c.poster, "%s", l->poster);
  snprintf(c.meta, sizeof c.meta, "%s", l->meta);
  c.nota = l->nota;
  c.progresso = l->progresso;
  c.temporada = l->temporada;
  c.episodio = l->episodio;
  c.restanteMin = l->restanteMin;
  snprintf(menuId, sizeof menuId, "%s", l->id);
  // O seguinte, ou o anterior quando a linha e a ultima: e para onde o foco
  // vai se o titulo sair da lista.
  menuProximo[0] = 0;
  if (foco + 1 < nLinhas) snprintf(menuProximo, sizeof menuProximo, "%s", linhas[foco + 1].id);
  else if (foco > 0) snprintf(menuProximo, sizeof menuProximo, "%s", linhas[foco - 1].id);
  ctx_inline_pedir(1);
  ctx_abrir_salvo(&c);
}

// O toque curto de sempre: entrega o IMDb e fecha, app.c abre o titulo.
static void okSocial(void) {}
static void abrirLinha(void) {
  if (aba == SP_ABA_AVISOS) { if (avisos_lista_ok(foco) == 1) spainel_fechar(); return; }
  if (foco >= 0 && foco < nLinhas) {
    snprintf(pedido, sizeof pedido, "%s", linhas[foco].id);
    temPedido = 1;
    aberto = 0;
  }
}

static int teclaOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

// --- EDITAR ABAS -----------------------------------------------------------
//
// Uma linha por aba que existe agora (a social fora do ar nem aparece), na
// ordem da faixa. Tres botoes por linha: ligar/desligar (o olho), subir e
// descer; ← → andam entre eles e OK age. Uma aba tem de ficar ligada, sempre:
// o olho da ultima nao desliga. Cada mudanca ja grava (por perfil).
static int editIds(int *ids) {
  int i, n = 0;
  for (i = 0; i < SP_ABA_N; i++) if (abaExiste(abaOrdem[i])) ids[n++] = abaOrdem[i];
  return n;
}
static int nLigadas(void) {
  int i, n = 0;
  for (i = 0; i < SP_ABA_N; i++) if (abaLigada(i)) n++;
  return n;
}
static void abrirEditar(void) {
  trocaIniciar(1);
  editando = 1; editLin = 0; editCol = 0; editLapis = 0;
}
static void fecharEditar(void) {
  trocaIniciar(-1);
  editando = 0;
  foco = SP_FOCO_ABAS; editLapis = 1;
  // Se a aba aberta foi desligada, a faixa vai para uma que sobrou.
  if (!abaLigada(aba) && algumaLigada()) trocarAba(primeiraAba());
  editLapis = 1;
  scrollY = 0.0f; velY = 0.0f;
}
static void editarTecla(SDL_Keycode k) {
  int ids[SP_ABA_N], n = editIds(ids);
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE) {
    fecharEditar(); return;
  }
  if (k == SDLK_UP) { if (editLin > 0) editLin--; if (editLin >= n) editCol = 0; return; }
  if (k == SDLK_DOWN) { if (editLin < n) editLin++; if (editLin >= n) editCol = 0; return; }
  if (k == SDLK_LEFT) { if (editCol > 0) editCol--; return; }
  if (k == SDLK_RIGHT) { if (editLin < n && editCol < 2) editCol++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    int id;
    if (editLin >= n) { fecharEditar(); return; }
    id = ids[editLin];
    if (editCol == 0) {
      if (abaLiga[id] && nLigadas() <= 1) return;   // a ultima fica
      abaLiga[id] = !abaLiga[id];
      abasGravar();
    } else {
      int viz = editLin + (editCol == 1 ? -1 : 1);
      if (viz >= 0 && viz < n) {
        int pa = abaPos(id), pb = abaPos(ids[viz]), t = abaOrdem[pa];
        abaOrdem[pa] = abaOrdem[pb]; abaOrdem[pb] = t;
        editLin = viz;
        abasGravar();
      }
    }
  }
}

// Troca de aba pelas setas com o foco NA LISTA: o foco cai no mesmo indice da
// aba nova se ele existe, senao no primeiro item (trocarAba o poe nas abas).
static void trocarAbaDaLista(int d) {
  int antes = foco, nova = proximaAba(aba, d);
  trocarAba(nova);
  if (aba == nova && nVisiveis() > 0) foco = antes < nVisiveis() ? antes : 0;
}
void spainel_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  // O teclado e a escolha aberta ficam POR CIMA da lista: a tecla e deles.
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (pop) { popEvento(e); return; }
  // A SOLTURA DO OK numa linha de Salvos: curto abre o titulo, longo abre o
  // menu (se spainel_atualizar ainda nao o abriu no limiar — um quadro lento
  // ou um teste sem quadro). Soltura sem o KEYDOWN daqui nao e clique: e o OK
  // que fechou o menu do cartaz, ou o que abriu o painel por outra porta.
  if (e->type == SDL_KEYUP) {
    if (okDesde && teclaOk(e->key.keysym.sym)) {
      Uint32 dur = SDL_GetTicks() - okDesde;
      okDesde = 0;
      if (dur >= NV_HOLD_MS) abrirMenu();
      else abrirLinha();
    }
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (editando) { editarTecla(k); return; }
  // Qualquer outra tecla no meio desfaz o gesto, como na home (observarHold).
  if (!teclaOk(k)) okDesde = 0;
  // Mesmo conjunto de "voltar" que o menu lateral aceita, mais a ESQUERDA: o
  // painel encosta na borda direita da tela, entao sair por ele e ir para a
  // esquerda. E o gesto que perfil.c ja tinha nesta mesma posicao.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    spainel_fechar(); return;
  }
  // ESQUERDA NA LINHA DE ABAS NAO FECHA SE HA PARA ONDE IR. Fora dela, e fora
  // da primeira aba, ela continua sendo "sair pela borda" — o gesto que
  // perfil.c ja tinha nesta posicao.
  if (k == SDLK_LEFT) {
    if (temAbas() && foco == SP_FOCO_ABAS && editLapis) { editLapis = 0; return; }
    if (temAbas() && foco == SP_FOCO_ABAS && proximaAba(aba, -1) != aba) {
      trocarAba(proximaAba(aba, -1)); return;
    }
    // Na barra e na grade a esquerda anda; so na primeira coluna ela sai.
    if (foco == SP_FOCO_BARRA && barraFoco > 0) { barraFoco--; return; }
    if (aba == SP_ABA_SALVOS && foco > 0 && foco < nLinhas &&
        linhas[foco - 1].fila == linhas[foco].fila) { foco--; return; }
    // COM O FOCO NA LISTA a esquerda (na primeira coluna) volta uma aba.
    if (foco >= 0 && temAbas() && !ctx_aberto() && proximaAba(aba, -1) != aba) { trocarAbaDaLista(-1); return; }
    spainel_fechar(); return;
  }
  if (k == SDLK_RIGHT) {
    if (temAbas() && foco == SP_FOCO_ABAS && editLapis) return;
    if (temAbas() && foco == SP_FOCO_ABAS) {
      int p = proximaAba(aba, 1);
      if (p != aba) trocarAba(p);
      else editLapis = 1;           // da ultima aba o D-pad vai para o lapis (Editar)
    }
    else if (foco == SP_FOCO_BARRA) { if (barraFoco + 1 < nChips()) barraFoco++; }
    else if (aba == SP_ABA_SALVOS && foco >= 0 && foco + 1 < nLinhas &&
             linhas[foco + 1].fila == linhas[foco].fila) foco++;
    // COM O FOCO NA LISTA a direita (na ultima coluna) avanca uma aba.
    else if (foco >= 0 && temAbas() && !ctx_aberto() && proximaAba(aba, 1) != aba) trocarAbaDaLista(1);
    return;
  }
  if (k == SDLK_DOWN) {
    if (foco == SP_FOCO_ABAS) {
      editLapis = 0;
      if (temBarra()) { foco = SP_FOCO_BARRA; if (barraFoco >= nChips()) barraFoco = 0; }
      else if (nVisiveis() > 0) foco = 0;
      return;
    }
    if (foco == SP_FOCO_BARRA) { if (nVisiveis() > 0) foco = 0; return; }
    if (aba == SP_ABA_SALVOS) {
      int v = celulaVizinha(foco, 1);
      if (v >= 0) foco = v;
      return;
    }
    if (foco + 1 < nVisiveis()) foco++;
    return;
  }
  if (k == SDLK_UP) {
    // DE CIMA DA LISTA SOBE PARA A BARRA (se ha) E DAI PARA AS ABAS, e nao
    // para lugar nenhum. Sem isto a unica forma de trocar de aba seria fechar
    // e reabrir o painel.
    // Sem abas a barra e o topo: CIMA para nela.
    if (foco == SP_FOCO_BARRA) { if (temAbas()) foco = SP_FOCO_ABAS; return; }
    if (foco == SP_FOCO_ABAS) return;
    if (aba == SP_ABA_SALVOS && foco < nLinhas) {
      int v = celulaVizinha(foco, -1);
      if (v >= 0) { foco = v; return; }
      if (temBarra()) foco = SP_FOCO_BARRA;
      else if (temAbas()) foco = SP_FOCO_ABAS;
      return;
    }
    if (foco == 0 && temAbas()) { foco = temBarra() ? SP_FOCO_BARRA : SP_FOCO_ABAS; return; }
    if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (foco == SP_FOCO_BARRA) { if (!e->key.repeat) chipOk(); return; }
    if (foco == SP_FOCO_ABAS) {
      // OK na linha de abas alterna, para quem nao descobriu a seta.
      if (editLapis) { abrirEditar(); return; }
      { int p = proximaAba(aba, 1); trocarAba(p == aba ? primeiraAba() : p); }
      return;
    }
    if (aba == SP_ABA_AVISOS) {
      // Linha de aviso: curto abre, longo abre o menu (soltura/limiar). A
      // ultima, "Dispensar todos", age ja: nao ha o que segurar.
      if (linhaAvisoSeguravel()) {
        if (!e->key.repeat && !okDesde) { okDesde = SDL_GetTicks(); if (!okDesde) okDesde = 1; }
        return;
      }
      if (!e->key.repeat && avisos_lista_ok(foco) == 1) spainel_fechar();
      focoAvisosValido();
      return;
    }
    if (aba == SP_ABA_AGENDA) {
      if (e->key.repeat) return;
      if (foco >= 0 && foco < nAg) {
        const AgItem *it = agenda_lista(agIdx[foco]);
        if (it && it->imdb[0]) {
          snprintf(pedido, sizeof pedido, "%s", it->imdb);
          temPedido = 1;
          aberto = 0;
        }
      } else if (foco == nAg) { temPedidoAgenda = 1; aberto = 0; }
      return;
    }
    // A decisao fica para a soltura (ou para o limiar, em spainel_atualizar).
    // A repeticao automatica do controle nao rearma: o relogio e do primeiro
    // KEYDOWN.
    if (linhaSeguravel() && !e->key.repeat && !okDesde) {
      okDesde = SDL_GetTicks();
      if (!okDesde) okDesde = 1;
    }
    return;
  }
}

void spainel_atualizar(float dt, Uint32 agora) {
  int i;
  float alvo, topo, base;
  // A barra de "Segure OK" do menu do cartaz, centrada no painel enquanto ele e
  // dono do D-pad; fora dele, no centro da tela como sempre.
  ctx_centro_dica(aberto && (aba == SP_ABA_SALVOS ||
                             aba == SP_ABA_AVISOS ||
                             aba == SP_ABA_AGENDA)
                  ? SP_X + SP_W * 0.5f : -1.0f);
  if (!aberto) okDesde = 0;
  if (!aberto && entrada < 0.002f) {
    if (entrada != 0.0f) entrada = 0.0f;
    entradaP = entradaV = 0.0f; pilIni = 0; trocaT = 1.0f; trocaV = 0.0f;
    deIlha = 0;
    return;
  }
  // O catalogo pode ter sido republicado com o painel aberto (a descoberta faz
  // isso varias vezes por ciclo), e a conta pode ter marcado um titulo. Sem
  // reconstruir, a lista continuaria a do instante da abertura. Ver listaVelha:
  // a pergunta por quadro sao duas revisoes, e nao um retrato do catalogo.
  if (aberto && listaVelha()) reconstruir();
  // O teclado de nome de categoria e o que ele devolveu.
  if (tecladoPara) { teclado_atualizar(dt, agora); tecladoResultado(); }
  // "Mover para categoria" no menu do cartaz: a escolha abre aqui, por cima
  // do painel, com as categorias da pessoa.
  { const char *id = ctx_pediu_categoria();
    if (id && aberto) popAbrirMover(id); }
  // A barra some quando deixa de fazer sentido (a lista esvaziou, a Social
  // ficou com uma recomendacao so): o foco nao pode ficar num lugar que nao
  // e mais desenhado.
  if (foco == SP_FOCO_BARRA && !temBarra()) foco = SP_FOCO_ABAS;
  if (barraFoco >= nChips()) barraFoco = nChips() - 1;
  // O LIMIAR DO OK LONGO, com o dedo ainda no botao (ver okDesde).
  if (aberto && okDesde && SDL_GetTicks() - okDesde >= NV_HOLD_MS) {
    okDesde = 0;
    abrirMenu();
  }
  // A acao da linha social escolhida no menu ("Ja assisti", "Ver perfil"...).
  { int k = ctx_pediu_extra();
    if (k >= 0 && aberto) extraSocial(k); }
  // O menu saiu e a lista ja remontou o que tinha de remontar: o foco volta a
  // ser por indice, como sempre.
  if (menuId[0] && !ctx_aberto() && !listaVelha()) menuId[0] = menuProximo[0] = 0;
  if (aberto && agVer != agenda_versao()) {
    reconstruirAgenda();
    if (aba == SP_ABA_AGENDA && foco >= nVisiveis()) foco = nVisiveis() > 0 ? nVisiveis() - 1 : SP_FOCO_ABAS;
  }

  // A abertura e o fechamento na mola do modal da ilha (~0,7 s, repique curto).
  entradaP = mov_mola_assenta(&entradaV, entradaP, aberto ? 1.0f : 0.0f, dt, MOV_MODAL_W, MOV_MODAL_Z);
  entrada = anim_clamp(entradaP, 0.0f, 1.0f);
  trocaT = mov_mola_assenta(&trocaV, trocaT, 1.0f, dt, MOV_CORPO_W, MOV_CORPO_Z);
  if (pilIni) {
    pilX = mov_mola_assenta(&pilXv, pilX, pilAlvoX, dt, MOV_PILULA_W, MOV_PILULA_Z);
    pilW = mov_mola_assenta(&pilWv, pilW, pilAlvoW, dt, MOV_PILULA_W, MOV_PILULA_Z);
  }
  for (i = 0; i < nVisiveis() && i < SP_MAX; i++) {
    float a = (aberto && i == foco) ? 1.0f : 0.0f;
    animFoco[i] = ajustes_animacoes_reduzidas()
      ? a
      : anim_mola(animFoco[i], a, dt,
                  a > animFoco[i] ? NV_MOLA_FOCO : SP_MOLA_DESFOCO);
  }
  for (i = 0; i < SPB_N; i++) {
    float a = (aberto && foco == SP_FOCO_BARRA && i == barraFoco && !pop) ? 1.0f : 0.0f;
    animBarra[i] = ajustes_animacoes_reduzidas() ? a
      : anim_mola(animBarra[i], a, dt, a > animBarra[i] ? NV_MOLA_FOCO : SP_MOLA_DESFOCO);
  }
  // A ESCOLHA: entra em 140 ms, o foco na mola de sempre, e a lista dela rola
  // o minimo para a opcao focada caber (sete categorias ja passam da janela).
  if (pop) {
    float alvoR = popRol, topoR = (float)popFoco * (POP_LINHA_H + POP_LINHA_VAO);
    float janela = (float)POP_VISIVEIS * (POP_LINHA_H + POP_LINHA_VAO);
    popEntrada = anim_rampa(popEntrada, 1.0f, dt, 140.0f);
    for (i = 0; i < popN && i < POP_MAX; i++) {
      float a = i == popFoco ? 1.0f : 0.0f;
      popAnim[i] = ajustes_animacoes_reduzidas() ? a
        : anim_mola(popAnim[i], a, dt, a > popAnim[i] ? NV_MOLA_FOCO : SP_MOLA_DESFOCO);
    }
    if (topoR + POP_LINHA_H - alvoR > janela) alvoR = topoR + POP_LINHA_H - janela;
    if (topoR < alvoR) alvoR = topoR;
    popRol = ajustes_animacoes_reduzidas() ? alvoR : anim_mola(popRol, alvoR, dt, NV_MOLA_FOCO);
  }
  // Rola o MINIMO para a linha focada caber inteira, como a grade da
  // Biblioteca. Alinhar a focada ao topo joga o cabecalho para fora na primeira
  // descida e a pessoa perde de vista em que painel esta.
  alvo = scrollY;
  if (foco == SP_FOCO_ABAS || foco == SP_FOCO_BARRA) alvo = 0.0f;
  else if (nVisiveis() > 0 && foco >= 0 && foco < nVisiveis()) {
    float janela = SP_LISTA_BASE - listaTopo();
    topo = topoDe(foco);
    base = topo + (aba == SP_ABA_AGENDA ? (foco < nAg ? SPAG_H : SPS_H_ACAO)
                 : aba == SP_ABA_AVISOS ? avisos_lista_altura_linha(foco, foco)
                 : linhas[foco].lh);
    // A linha aberta (acordeao) cresce para baixo: ela inteira tem de caber.
    if (aba == SP_ABA_SALVOS && expIdx() == foco) {
      if (sorg_estilo() == SORG_ESTILO_LISTA) base += expExtra();
      else base = topo + (sorg_estilo() == SORG_ESTILO_GRADE ? SPG_PASSO : SPP_PASSO) - 12.0f + expExtraBloco();
    }
    // O ar do foco nas duas pontas: o conteudo ja nasce SP_FOCO_AR abaixo do
    // recorte (ver SP_FOCO_AR), entao em cima basta `topo` e embaixo sao dois.
    if (base + 2.0f * SP_FOCO_AR - alvo > janela) alvo = base + 2.0f * SP_FOCO_AR - janela;
    if (topo - alvo < 0.0f) alvo = topo;
  }
  if (alvo < 0.0f) alvo = 0.0f;
  scrollY = anim_mola2_reduzida(&velY, scrollY, alvo, dt, NV_MOLA2_SCROLL,
                                ajustes_animacoes_reduzidas());
}

// "Salvo há 2 horas". A FRASE INTEIRA passa por i18n como FORMATO, nao montada
// de pedacos: "há" e "atrás" trocam de lugar na traducao e uma frase remendada
// aqui sairia "2 horas ago" em ingles.
static void quandoTexto(char *dst, size_t tam, long long quandoS) {
  long long agora = (long long)time(NULL);
  long long d = agora - quandoS;
  if (quandoS <= 0) { dst[0] = 0; return; }
  if (d < 0) d = 0;
  if (d < 90)            snprintf(dst, tam, "%s", i18n("Salvo agora"));
  else if (d < 5400)     snprintf(dst, tam, i18n("Salvo há %d min"), (int)(d / 60));
  else if (d < 172800)   snprintf(dst, tam, i18n("Salvo há %d h"),   (int)(d / 3600));
  else                   snprintf(dst, tam, i18n("Salvo há %d dias"),(int)(d / 86400));
}

// O catalogo guarda a posicao como percentual e o restante em minutos. Com
// os dois valores presentes, esta e uma aproximacao honesta da parte ja vista:
// restante * p / (100 - p). Nao usamos p=100 (divisao por zero) nem inventamos
// minutos quando a fonte so trouxe o percentual.
static int minutosAssistidosAprox(const SPLinha *l) {
  int p = l ? l->progresso : 0;
  if (p <= 0 || p >= 100 || l->restanteMin <= 0) return 0;
  return (int)(((float)l->restanteMin * (float)p /
                (100.0f - (float)p)) + 0.5f);
}

static void restanteTexto(char *dst, size_t tam, const SPLinha *l) {
  if (l->temporada > 0 && l->episodio > 0 && l->restanteMin > 0)
    snprintf(dst, tam, i18n("T%dE%d · %d min restantes"),
             l->temporada, l->episodio, l->restanteMin);
  else if (l->temporada > 0 && l->episodio > 0)
    snprintf(dst, tam, i18n("T%dE%d · retomar"), l->temporada, l->episodio);
  else if (l->restanteMin > 0)
    snprintf(dst, tam, i18n("%d min restantes"), l->restanteMin);
  else
    snprintf(dst, tam, "%s", i18n("Retomar"));
}

static void legendasDaBarra(SPLinha *l) {
  int vistoMin;
  l->txtRestante[0] = l->txtVisto[0] = 0;
  if (l->progresso <= 0) return;
  restanteTexto(l->txtRestante, sizeof l->txtRestante, l);
  vistoMin = minutosAssistidosAprox(l);
  if (vistoMin > 0) {
    char mins[80];
    snprintf(mins, sizeof mins, i18n("%d min assistidos"), vistoMin);
    snprintf(l->txtVisto, sizeof l->txtVisto, "≈%s", mins);
  } else {
    snprintf(l->txtVisto, sizeof l->txtVisto, i18n("%d%% assistido"), l->progresso);
  }
}


// O foco chega como uma mola, mas a superficie nao precisa obedecer a uma
// reta. A smoothstep deixa os primeiros pixels assentarem no fundo escuro e
// segura o ultimo brilho do accent — uma entrada mais "material" sem criar
// overshoot ou uma animacao mais longa.
static float focoVisual(float f) {
  f = anim_clamp(f, 0.0f, 1.0f);
  return f * f * (3.0f - 2.0f * f);
}

// GLASS UI (dono, 02/10, mockup "ilha" tela 3): FOCO DE LINHA = SUPERFICIE UM
// DEGRAU MAIS CLARA, nos dois materiais — branco a 12 % sobre o vidro, cinza
// opaco sobre o solido. Sem aro, sem bloco cheio no acento e por isso SEM
// inverter o texto: a linha focada segue com as cores de repouso, e o acento
// fica para o estado (a barra de "nao lida", o ponto ao vivo, o selo da aba).
// Em repouso a linha nao tem superficie nenhuma: ela mora direto na ilha, como
// as linhas da folha de Fontes. focoTexto continua existindo para os chips e
// botoes (botaoSup), que sao FOCO DE BOTAO e esses sim invertem.
static float focoTexto(float f) { (void)f; return 0.0f; }

static void superficieItem(GfxRect r, float raio, float f, float a) {
  float v = focoVisual(f);
  if (v <= .001f || a <= .001f) return;
  if (ajustes_vidro()) { gfx_cor(r, raio, 1, 1, 1, .12f * v * a); return; }
  // NO SOLIDO A LINHA FOCADA SOBE: #2b2d34 com a sombra curta do mockup
  // (".row.foco" solido: 0 10px 30px a 40 %) e o fio claro de 1 px no topo
  // ("inset 0 1px 0" a 6 %). Sem a sombra o degrau cinza sobre o cinza do
  // painel e o unico sinal, e de longe ele some.
  gfx_rect((GfxRect){ r.x - 14.0f, r.y - 2.0f, r.w + 28.0f, r.h + 30.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, .30f * v * a);
  gfx_cor(r, raio, .169f, .176f, .204f, v * a);
  { float rp = raio * (r.h < r.w ? r.h : r.w);
    gfx_cor((GfxRect){ r.x + rp, r.y, r.w - 2.0f * rp, 1.0f }, 0.0f, 1, 1, 1, .06f * v * a); }
}

// CAIXA DE AJUSTE (o interruptor de aparecer, nivel e nome): precisa de um
// fundo em repouso, senao o interruptor boia sem dizer onde a linha comeca.
// Um degrau abaixo do foco: branco a 5 % no vidro, cinza opaco no solido.
static void superficieCaixa(GfxRect r, float raio, float f, float a) {
  if (ajustes_vidro()) gfx_cor(r, raio, 1, 1, 1, .05f * a);
  else gfx_cor(r, raio, .098f, .102f, .118f, a);
  superficieItem(r, raio, f, a);
}

// FOCO DE BOTAO (chips da barra, "Adicionar um amigo", as respostas do
// consentimento): o mesmo chipFolha da folha de Fontes. Repouso em branco a
// 8 % (vidro) ou cinza opaco (solido); foco = pilula CHEIA no acento — no
// vidro gfx_vidro_pilula_cheia, no solido com a luz de botao atras — e o texto
// vai para ajustes_tinta_foco (quem chama usa focoVisual, nao focoTexto).
static void botaoSup(GfxRect r, float raio, float f, float a) {
  float v = focoVisual(f), ar, ag, ab;
  if (ajustes_vidro()) gfx_cor(r, raio, 1, 1, 1, .08f * (1.0f - v) * a);
  else gfx_cor(r, raio, .14f, .148f, .17f, (1.0f - v) * a);
  if (v <= .001f) return;
  if (ajustes_vidro()) { gfx_vidro_pilula_cheia(r, raio, v, a); return; }
  ajustes_acento(&ar, &ag, &ab);
  botao_luz(r, v * .55f, a);
  gfx_cor(r, raio, ar, ag, ab, v * a);
}

// CAIXA ALTA ESPACADA (".kick" do mockup: 15 bold, 0,14 em): o kicker do
// cabecalho e o rotulo dos chips. A mesma conta de caixaAlta em streams.c —
// i18n antes da caixa alta, porque a tabela de idioma guarda a frase normal.
// x = -1 so mede.
static float caixaAltaIlha(const char *s, int r, int g, int b, float x, float y, float a) {
  char up[200];
  size_t k;
  snprintf(up, sizeof up, "%s", i18n(s));
  for (k = 0; up[k]; k++)
    if (up[k] >= 'a' && up[k] <= 'z') up[k] = (char)(up[k] - 32);
    else if ((unsigned char)up[k] == 0xC3 && up[k + 1] && (unsigned char)up[k + 1] >= 0xA0 &&
             (unsigned char)up[k + 1] <= 0xBE) { up[k + 1] = (char)((unsigned char)up[k + 1] - 0x20); k++; }
  return txt_tracking(TXT_MINI, up, r, g, b, x, y, a, 2.1f);
}
static float secaoAlt(int primeira) { return primeira ? SP_SECAO_H1 : SP_SECAO_H; }
static void desenhaSecao(float x, float y, const char *rotulo, float a, int primeira) {
  TxtLinha t = txt_linha_corta(TXT_ILHA_SECAO, rotulo, 243, 242, 239, 255, SP_INTERNO);
  float ty = y + (primeira ? SP_SECAO_AR1 : SP_SECAO_AR);
  txt_desenhar_alpha(t, x + SP_PAD, ty, a);
  gfx_cor((GfxRect){ x + SP_LINHA_X, ty + SP_SECAO_TXT + 10.0f, SP_LINHA_W, 1.0f }, 0.0f,
          1.0f, 1.0f, 1.0f, 0.08f * a);
}

// --- A LINHA DA ILHA ---------------------------------------------------------
// Uma gramatica so para tudo que tem rosto ou capa (feed, recomendacao, amigo,
// sugestao, aviso): a superficie de 26 a 26 das bordas, raio 22, 18/22 de
// recuo; o rosto de 52 a esquerda, 18 px, o texto em ate tres andares e, quando
// ha, a CAPA 52x76 A DIREITA. Os andares, do CSS do mockup:
//   nome    24 semibold, branco a 88 % (100 % no foco) + o verbo 24 regular a 55 %
//   titulo  19 regular a 62 %, 4 px abaixo
//   quando  15 regular a 38 %, 4 px abaixo
// A tinta e a cor do mockup (#f3f2ef) com ALFA, e nao um cinza opaco: sobre o
// vidro o texto secundario deixa a arte passar do mesmo jeito que no CSS, e a
// chave do cache de texto continua uma so por cor.
static TxtLinha txtIlha(TxtEstilo e, const char *s, float maxW) {
  return txt_linha_corta(e, s, SPI_FG_R, SPI_FG_G, SPI_FG_B, 255, maxW);
}
// A superficie da linha: so no foco (as linhas moram direto na ilha).
static GfxRect linhaIlhaRet(float dx, float y, float h) {
  return (GfxRect){ SP_X + dx + SP_LINHA_X, y, SP_LINHA_W, h };
}
// Nome + verbo na mesma linha de base. O nome atravessa de 88 % para branco
// cheio com o foco, como ".row.foco" do mockup; o verbo fica a 55 %.
static float nomeVerboEst(TxtEstilo es, const char *nome, const char *verbo, float x, float y,
                          float larg, float v, float a) {
  TxtLinha n = txtIlha(es, nome, larg * 0.6f);
  TxtLinha nb = txt_linha_corta(es, nome, 255, 255, 255, 255, larg * 0.6f);
  txt_desenhar_alpha(n, x, y, a * 0.88f * (1.0f - v));
  txt_desenhar_alpha(nb, x, y, a * v);
  if (verbo && verbo[0] && larg - (float)n.w - 7.0f > 40.0f) {
    TxtLinha vb = txtIlha(TXT_ILHA_CORPO, verbo, larg - (float)n.w - 7.0f);
    txt_desenhar_alpha(vb, x + (float)n.w + 7.0f, y, a * 0.55f);
  }
  return (float)n.w;
}
static float nomeVerbo(const char *nome, const char *verbo, float x, float y, float larg,
                       float v, float a) {
  return nomeVerboEst(TXT_ILHA_NOME, nome, verbo, x, y, larg, v, a);
}

// A CAPA A DIREITA (".cap"): 52x76, raio 9, com o recuo de 22 da linha.
// A arte ENCHE a caixa (cover), recortada pelo raio: um cartaz que chega
// em outra proporcao (arte 16:9 no lugar do 2:3) sairia com faixas pretas
// dentro da capa, que e o que a primeira captura mostrou.
static void capaArte(GfxRect r, const char *url, float raioPx, float a) {
  GLuint tex = url && url[0] ? tex_obter_larg(url, (int)r.w) : 0;
  float raio = raioPx / (r.w < r.h ? r.w : r.h);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_card_forcar_cover_atual = 0.0f;
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  }
}

// Nota do IMDb em texto ("IMDb 8,1"), com o separador do idioma.
static void notaTexto(char *dst, size_t tam, int nota) {
  if (nota <= 0) { dst[0] = 0; return; }
  snprintf(dst, tam, idioma_ponto_decimal(ajustes_idioma()) ? "IMDb %d.%d" : "IMDb %d,%d",
           nota / 10, nota % 10);
}
// Junta `parte` ao fim de `dst` com " · ".
static void juntar(char *dst, size_t tam, const char *parte) {
  size_t k = strlen(dst);
  if (!parte || !parte[0] || k + 1 >= tam) return;
  snprintf(dst + k, tam - k, "%s%s", k ? " \xc2\xb7 " : "", parte);
}

// `dx` e o deslocamento da animacao de entrada. Ele PRECISA chegar ate aqui: as
// linhas sao desenhadas em coordenada absoluta, e sem somar o mesmo `dx` do
// painel elas ficariam paradas no lugar final enquanto a moldura ainda desliza
// — o conteudo apareceria antes da caixa que o contem.
static const char *tipoRotulo(const SPLinha *l) {
  switch (l->tipoG) {
    case SPT_CANAL:   return "Canal";
    case SPT_COLECAO: return "Coleção";
    case SPT_SERIE:   return "Série";
    default:          return "Filme";
  }
}

// A barra fina de progresso POR CIMA da arte (grade e paisagem): a mesma
// leitura da home, trilho escuro e preenchimento na cor de realce.
static void barraSobreArte(GfxRect arte, int progresso, float a) {
  float p = anim_clamp(progresso / 100.0f, 0.0f, 1.0f), pr, pg, pb;
  GfxRect t = { arte.x + 10.0f, arte.y + arte.h - 14.0f, arte.w - 20.0f, 5.0f };
  GfxRect c = t;
  if (progresso <= 0) return;
  ajustes_acento(&pr, &pg, &pb);
  c.w = t.w * p;
  gfx_cor(t, 0.5f, 0.0f, 0.0f, 0.0f, 0.55f * a);
  if (c.w > t.h) gfx_cor(c, 0.5f, pr, pg, pb, a);
}

// A arte de uma celula: textura pela largura de desenho (tex_obter_larg, a
// nota longa em desenhaLinha), recortada em `cover`, ou o esqueleto.
static void arteCelula(GfxRect r, const char *url, float raio, float a) {
  GLuint tex = url && url[0] ? tex_obter_larg(url, (int)r.w) : 0;
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_card_forcar_cover_atual = 0.0f;
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  }
}

// A LINHA EXPANDIDA (acordeao): o indice da linha aberta e o quanto ela cresce.
#define SP_FAIXA_H 150.0f
static int expIdx(void) {
  int i;
  if (ctx_inline_t() < 0.003f || !menuId[0]) return -1;
  for (i = 0; i < nLinhas; i++) if (!strcmp(linhas[i].id, menuId)) return i;
  return -1;
}
static float expExtra(void) {
  float h = ctx_inline_altura(SP_LINHA_W, SP_FAIXA_H);
  return (h > SP_LINHA_SALVO ? h - SP_LINHA_SALVO : 0.0f) * ctx_inline_t();
}
static float expExtraBloco(void) {
  return (ctx_inline_altura(SP_LINHA_W, SP_FAIXA_H) + 8.0f) * ctx_inline_t();
}
// Grade e paisagem: o bloco se abre logo abaixo da fila do cartao em foco (que
// continua la, aceso), com a faixa de arte, as informacoes e as pilulas.
static void desenhaBlocoAberto(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float e = ctx_inline_t(), hc = ctx_inline_altura(SP_LINHA_W, SP_FAIXA_H);
  float lx = SP_X + dx + SP_LINHA_X, ca = e < 0.6f ? 0.0f : (e - 0.6f) / 0.4f;
  GfxRect row = { lx, y, SP_LINHA_W, hc * e }, faixa = { lx + 18.0f, y + 18.0f, SP_LINHA_W - 36.0f, SP_FAIXA_H };
  superficieItem(row, SP_LINHA_RAIO / row.h, 1.0f, a * (e < 0.3f ? e / 0.3f : 1.0f));
  if (ca > 0.0f) {
    capaArte(faixa, l->fundo[0] ? l->fundo : l->poster, 22.0f, a * ca);
    gfx_veu_base(faixa, 22.0f / faixa.h, 0.62f, 0.7f * a * ca);
    ctx_inline_desenhar(lx, y, SP_LINHA_W, SP_FAIXA_H, a * ca);
  }
}

// A linha em foco aberta DENTRO do painel: a capa cresce numa faixa de arte (com
// o logo) no topo e, embaixo, meta, notas, sinopse e as pilulas de acao.
static void desenhaLinhaAberta(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float e = ctx_inline_t(), h = SP_LINHA_SALVO + expExtra();
  float px = SP_X + dx + SP_PAD, lx = SP_X + dx + SP_LINHA_X;
  GfxRect poster = { px, y + SP_LINHA_PADY, SP_POSTER_W, SP_POSTER_H };
  GfxRect faixa = { lx + 18.0f, y + 18.0f, SP_LINHA_W - 36.0f, SP_FAIXA_H }, r, row;
  float ca = e < 0.6f ? 0.0f : (e - 0.6f) / 0.4f;
  r.x = poster.x + (faixa.x - poster.x) * e; r.y = poster.y + (faixa.y - poster.y) * e;
  r.w = poster.w + (faixa.w - poster.w) * e; r.h = poster.h + (faixa.h - poster.h) * e;
  row = linhaIlhaRet(dx, y, h);
  superficieItem(row, SP_LINHA_RAIO / row.h, 1.0f, a);
  // O titulo e o apoio da linha comum somem logo; a capa vira a faixa.
  if (e < 0.5f) {
    char meta[256];
    TxtLinha t = txtIlha(TXT_ILHA_SUB, l->titulo, SP_INTERNO - SP_POSTER_W - 40.0f);
    meta[0] = 0;
    txt_desenhar_alpha(t, SP_X + dx + SP_TEXTO_X, y + SP_LINHA_PADY + 8.0f, a * (1.0f - e * 2.0f));
    (void)meta;
  }
  capaArte(r, l->fundo[0] && e > 0.4f ? l->fundo : l->poster, 10.0f + 12.0f * e, a);
  if (e > 0.3f) gfx_veu_base(r, (10.0f + 12.0f * e) / r.h, 0.62f, 0.7f * a * e);
  ctx_inline_desenhar(lx, y, SP_LINHA_W, SP_FAIXA_H, a * ca);
}

// GRADE DE POSTERES: o cartaz e o titulo embaixo. A superficie so existe no
// foco — em repouso a grade e so arte, que e o ponto do estilo.
static void desenhaCelulaGrade(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float f = animFoco[i], v = focoTexto(f);
  float x = SP_X + dx + SP_PAD + l->lx;
  int tf = ajustes_tinta_foco(), tf2 = ajustes_tinta_foco2();
  GfxRect poster = { x, y, l->lw, SPG_POSTER_H };
  if (f > 0.01f) {
    GfxRect r = { x - 10.0f, y - 10.0f, l->lw + 20.0f, l->lh + 14.0f };
    superficieItem(r, SP_LINHA_RAIO / r.h, f, a);
  }
  arteCelula(poster, l->poster, 0.06f, a);
  barraSobreArte(poster, l->progresso, a);
  // O titulo na tinta da ilha: 19 a 88 %, branco cheio no foco.
  { TxtLinha r = txtIlha(TXT_ILHA_SUB, l->titulo, l->lw);
    TxtLinha fo = txt_linha_corta(TXT_ILHA_SUB, l->titulo, 255, 255, 255, 255, l->lw);
    float fv = focoVisual(f);
    txt_desenhar_alpha(r, x, y + SPG_POSTER_H + 12.0f, a * 0.88f * (1.0f - fv));
    txt_desenhar_alpha(fo, x, y + SPG_POSTER_H + 12.0f, a * fv); }
  (void)tf2; (void)tf; (void)v;
}

// CARTOES PAISAGEM: a arte 16:9 do catalogo (o cartaz recortado quando ela
// nao veio), o titulo e uma linha de apoio — o que falta, quando ha progresso;
// senao tipo e ano.
static void desenhaCelulaPaisagem(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float f = animFoco[i], v = focoTexto(f);
  float x = SP_X + dx + SP_PAD + l->lx;
  int tf = ajustes_tinta_foco(), tf2 = ajustes_tinta_foco2();
  char apoio[192];
  GfxRect img = { x, y, l->lw, SPP_IMG_H };
  if (f > 0.01f) {
    GfxRect r = { x - 10.0f, y - 10.0f, l->lw + 20.0f, l->lh + 14.0f };
    superficieItem(r, SP_LINHA_RAIO / r.h, f, a);
  }
  arteCelula(img, l->fundo[0] ? l->fundo : l->poster, 0.07f, a);
  barraSobreArte(img, l->progresso, a);
  if (l->progresso > 0 && l->txtRestante[0]) snprintf(apoio, sizeof apoio, "%s", l->txtRestante);
  else if (l->ano > 0) snprintf(apoio, sizeof apoio, "%s · %d", i18n(tipoRotulo(l)), l->ano);
  else snprintf(apoio, sizeof apoio, "%s", i18n(tipoRotulo(l)));
  // Os dois andares de cima da linha da ilha: o titulo (24 semibold) e o
  // apoio (19 a 62 %).
  nomeVerbo(l->titulo, NULL, x, y + SPP_IMG_H + 12.0f, l->lw / 0.6f, focoVisual(f), a);
  txt_desenhar_alpha(txtIlha(TXT_ILHA_SUB, apoio, l->lw), x, y + SPP_IMG_H + 45.0f, a * 0.62f);
  (void)tf; (void)tf2; (void)v;
}

// A LINHA DE SALVOS, na gramatica da ilha: o cartaz e a coisa (fica a
// esquerda, onde o feed poe o rosto), o titulo em 24 semibold, o que ele e em
// 19 a 62 % ("Série · 2016 · IMDb 8,7 · Kids") e o andar de baixo em 15 —
// quando foi salvo, ou o trilho fino com o que falta. Os SELOS em pilula
// (tipo, ano, IMDb amarelo) sairam: o mockup escreve qualidade como texto, e
// tres pilulas por linha eram o "menos polido" que o dono apontou.
static void desenhaLinhaAberta(int i, float dx, float y, float a);
static void desenhaBlocoAberto(int i, float dx, float y, float a);
static void desenhaLinha(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float f = animFoco[i], v = focoVisual(f);
  float px = SP_X + dx + SP_PAD, tx = SP_X + dx + SP_TEXTO_X;
  char buf[192], meta[256];
  GfxRect poster = { px, y + SP_LINHA_PADY, SP_POSTER_W, SP_POSTER_H };
  if (i == expIdx()) {
    desenhaLinhaAberta(i, dx, y, a);
    return;
  }
  { GfxRect r = linhaIlhaRet(dx, y, SP_LINHA_SALVO);
    superficieItem(r, SP_LINHA_RAIO / r.h, f, a); }

  // PELA LARGURA DE DESENHO, e nao tex_obter: este decodificava cada cartaz a
  // 640 de largura (~2,4 MB) para desenha-lo pequeno: 121 salvos rolados
  // pedem ~290 MB a um cache de 300 (o log da C9 de 24/09, com o painel
  // aberto, mostrava gpu-cache=221 269MB). tex_cache.h: "Prefira esta a
  // tex_obter em qualquer arte de lista".
  // Esqueleto VISIVEL (#2C2C2C) enquanto nao chega, o mesmo da home: um
  // retangulo da cor do fundo le como card quebrado, nao como carregando.
  capaArte(poster, l->poster, 10.0f, a);

  meta[0] = 0;
  juntar(meta, sizeof meta, i18n(tipoRotulo(l)));
  juntar(meta, sizeof meta, l->meta);
  notaTexto(buf, sizeof buf, l->nota);
  juntar(meta, sizeof meta, buf);
  // A CATEGORIA DA PESSOA, quando a lista nao esta agrupada por ela: e a
  // resposta visivel do "Mover para categoria" — sem ela, mover com a lista
  // agrupada por progresso nao mudaria nada na tela.
  if (l->cat && grupoAtual != SORG_GRUPO_CATEGORIA)
    juntar(meta, sizeof meta, sorg_categoria_nome_id(l->cat));

  { float ty = y + SP_LINHA_PADY + (SP_POSTER_H - 92.0f) * 0.5f;
    nomeVerboEst(TXT_ILHA_NOME_L, l->titulo, NULL, tx, ty, SP_TEXTO_W / 0.6f, v, a);
    txt_desenhar_alpha(txtIlha(TXT_ILHA_SUB_L, meta, SP_TEXTO_W), tx, ty + 39.0f, a * 0.62f);
    ty += 71.0f;
    if (l->progresso > 0) {
      // O TRILHO DA HOME em miniatura: branco a 18 % e o preenchimento no
      // acento (e estado — o quanto falta). Ao lado, o que FALTA ("T3E4 · 43
      // min restantes") a 62 %, e o ja visto, contexto, a 38 %.
      float p = anim_clamp(l->progresso / 100.0f, 0.0f, 1.0f), pr, pg, pb;
      float lx = tx + SP_BARRA_W + SP_BARRA_LABEL_GAP;
      GfxRect trilho = { tx, ty + (21.0f - SP_BARRA_H) * 0.5f, SP_BARRA_W, SP_BARRA_H };
      GfxRect cheio = trilho;
      TxtLinha r = txtIlha(TXT_ILHA_HORA_L, l->txtRestante, SP_TEXTO_W - SP_BARRA_W - SP_BARRA_LABEL_GAP);
      ajustes_acento(&pr, &pg, &pb);
      cheio.w = trilho.w * p;
      gfx_cor(trilho, 0.5f, 1.0f, 1.0f, 1.0f, 0.18f * a);
      if (cheio.w > SP_BARRA_H) gfx_cor(cheio, 0.5f, pr, pg, pb, a);
      txt_desenhar_alpha(r, lx, ty, a * 0.62f);
      if (l->txtVisto[0] && lx + (float)r.w + 20.0f < tx + SP_TEXTO_W) {
        snprintf(buf, sizeof buf, "\xc2\xb7 %s", l->txtVisto);
        txt_desenhar_alpha(txtIlha(TXT_ILHA_HORA_L, buf, tx + SP_TEXTO_W - lx - (float)r.w - 7.0f),
                           lx + (float)r.w + 7.0f, ty, a * 0.38f);
      }
    } else {
      quandoTexto(buf, sizeof buf, l->quandoS);
      if (buf[0]) txt_desenhar_alpha(txtIlha(TXT_ILHA_HORA_L, buf, SP_TEXTO_W), tx, ty, a * 0.38f);
    } }
}


// A LINHA DE ABAS. Sem animacao de cor de proposito: a cor faz parte da chave
// do cache de linhas de text.c, e uma cor por quadro cria uma rasterizacao TTF
// e uma textura GL por quadro — estourado o orcamento, a linha simplesmente
// NAO E DESENHADA (ver a nota longa em ctxmenu.c). O foco aparece no anel, que
// e geometria e nao custa texto.
// Largura da faixa de abas, para a contagem do cabecalho parar antes dela.
// AS ABAS SAO UM SELETOR SEGMENTADO (Glass UI, mockup "ilha" tela 3; o mesmo
// seletor de addon da folha de Fontes): um conteiner pilula em branco a 5 %
// (solido: cinza opaco), a aba aberta num segmento um degrau mais claro e, com
// o D-pad na faixa, esse segmento vira a pilula cheia no acento (foco de botao).
// Era rotulo solto com um traco de acento embaixo da aba aberta: lia como
// texto, nao como controle, e o traco gastava o acento num estado que a
// superficie ja diz.
#define SP_SEG_PAD   5.0f
#define SP_SEG_VAO   4.0f
#define SP_SEG_ICO  26.0f   // o icone Lucide da aba
#define SP_SEG_PX   16.0f   // recuo lateral de cada segmento
#define SP_SEG_NUM   8.0f
static const char *rotuloAba(int i) {
  return i == SP_ABA_SALVOS ? "Salvos" : i == SP_ABA_ATIVIDADE ? "Atividade"
       : i == SP_ABA_SOCIAL ? "Amigos" : i == SP_ABA_AGENDA ? "Agenda" : "Avisos";
}
static const char *iconeAba(int i) {
  return i == SP_ABA_SALVOS ? "aj_bookmark" : i == SP_ABA_ATIVIDADE ? "aj_activity"
       : i == SP_ABA_SOCIAL ? "aj_users" : i == SP_ABA_AGENDA ? "aj_calendar" : "aj_bell";
}
// A CONTAGEM AO LADO DO ICONE (".sg .n" do mockup), 16 px a 35 %. O que ha de
// NOVO (recomendacao recebida, aviso nao lido) e estado: o numero passa a ser o
// das novidades e vai no acento. `*novo` diz qual dos dois. 0 = sem numero.
static int contaDaAba(int i, int *novo) {
  int n;
  *novo = 0;
  if (i == SP_ABA_SALVOS) return nLinhas;
  if (i == SP_ABA_AVISOS) {
    if ((n = avisos_n_novos()) > 0) { *novo = 1; return n; }
    return avisos_lista_n();
  }
  if (i == SP_ABA_AGENDA) return nAg;
  return 0;
}
static float segLargura(int i, TxtLinha *num, int cor, int comNum) {
  int novo, n = comNum ? contaDaAba(i, &novo) : 0;
  char b[16];
  num->w = 0; num->h = 0; num->tex = 0;
  if (n > 0) {
    float ar, ag, ab;
    snprintf(b, sizeof b, "%d", n > 99 ? 99 : n);
    ajustes_acento(&ar, &ag, &ab);
    *num = novo ? txt_linha(TXT_ILHA_NUM, b, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255)
                : txt_linha(TXT_ILHA_NUM, b, cor, cor, cor, 255);
  }
  return SP_SEG_ICO + SP_SEG_PX * 2.0f + (num->w > 0 ? SP_SEG_NUM + (float)num->w : 0.0f);
}
#define SP_SEG_LAPIS (SP_SEG_ICO + SP_SEG_PX * 2.0f)
// Largura da faixa. Com o titulo grande ao lado, o que sobra e o painel menos o
// titulo: se nao cabe com todas as contagens, so a aba aberta mostra a dela.
static float abasLargura(int todasContagens) {
  float w = SP_SEG_PAD * 2.0f - SP_SEG_VAO + SP_SEG_LAPIS + SP_SEG_VAO;
  int i;
  for (i = 0; i < SP_ABA_N; i++) {
    TxtLinha n;
    int id = abaOrdem[i];
    if (!abaNaFaixa(id)) continue;
    w += segLargura(id, &n, 255, todasContagens || id == aba) + SP_SEG_VAO;
  }
  return w;
}

// A FAIXA DE ABAS na linha do titulo, a direita dele: um seletor segmentado
// compacto (icone + contagem), a aba aberta num segmento um degrau mais claro
// e, com o D-pad na faixa, esse segmento vira a pilula cheia no acento (foco
// de botao). O NOME da aba aberta e o titulo grande; as outras se leem pelo
// icone e pela posicao. No fim, o lapis (Editar). Sem animacao de cor de
// proposito: a cor faz parte da chave do cache de linhas de text.c.
static void desenhaAbas(float dx, float a, float larguraDisp) {
  float ar, ag, ab, w0;
  int pos, todas = 1;
  float x;
  if (abasLargura(1) > larguraDisp) todas = 0;
  w0 = abasLargura(todas);
  x = SP_X + dx + SP_W - SP_ABAS_X - w0;
  ajustes_acento(&ar, &ag, &ab);
  { GfxRect caixa = { x, SP_ABAS_Y, w0, SP_ABAS_H };
    if (ajustes_vidro()) gfx_cor(caixa, 0.5f, 1, 1, 1, .06f * a);
    else gfx_cor(caixa, 0.5f, .113f, .118f, .137f, a); }
  x += SP_SEG_PAD;
  // A PILULA DA ABA ABERTA ANDA ate a nova na mola da ilha (movimento.h); o
  // icone e a contagem ficam no segmento. Primeiro o alvo (a posicao do
  // segmento da aba aberta), depois a pilula, por BAIXO dos icones. Quem a
  // move e spainel_atualizar; na primeira vez ela cola no alvo.
  { float xs = x;
    for (pos = 0; pos < SP_ABA_N; pos++) {
      int i = abaOrdem[pos];
      TxtLinha num;
      float w;
      if (!abaNaFaixa(i)) continue;
      w = segLargura(i, &num, 255, todas || i == aba);
      if (i == aba) {
        GfxRect p;
        pilAlvoX = xs; pilAlvoW = w;
        if (!pilIni) { pilX = pilAlvoX; pilW = pilAlvoW; pilXv = pilWv = 0.0f; pilIni = 1; }
        p = (GfxRect){ pilX, SP_ABAS_Y + SP_SEG_PAD, pilW, SP_ABAS_H - SP_SEG_PAD * 2.0f };
        if (foco == SP_FOCO_ABAS && !editLapis) {
          if (ajustes_vidro()) gfx_vidro_pilula_cheia(p, 0.5f, 1.0f, a);
          else { botao_luz(p, .55f, a); gfx_cor(p, 0.5f, ar, ag, ab, a); }
        } else {
          if (ajustes_vidro()) gfx_cor(p, 0.5f, 1, 1, 1, .14f * a);
          else gfx_cor(p, 0.5f, .204f, .212f, .243f, a);
        }
      }
      xs += w + SP_SEG_VAO;
    } }
  for (pos = 0; pos <= SP_ABA_N; pos++) {
    int lapis = (pos == SP_ABA_N);
    int i = lapis ? -1 : abaOrdem[pos];
    int ativa = !lapis && i == aba;
    int focada = foco == SP_FOCO_ABAS && (lapis ? editLapis : (ativa && !editLapis));
    int cor = focada ? ajustes_tinta_foco() : 255;
    float alfaTxt = focada || ativa ? 1.0f : 0.55f;
    TxtLinha num;
    float w, ic;
    GfxRect p;
    if (!lapis && !abaNaFaixa(i)) continue;
    w = lapis ? SP_SEG_LAPIS : segLargura(i, &num, cor, todas || ativa);
    if (lapis) num.w = 0;
    p = (GfxRect){ x, SP_ABAS_Y + SP_SEG_PAD, w, SP_ABAS_H - SP_SEG_PAD * 2.0f };
    if (focada && lapis) {
      if (ajustes_vidro()) gfx_vidro_pilula_cheia(p, 0.5f, 1.0f, a);
      else { botao_luz(p, .55f, a); gfx_cor(p, 0.5f, ar, ag, ab, a); }
    }
    ic = (float)cor / 255.0f;
    gfx_icone((GfxRect){ x + SP_SEG_PX, p.y + (p.h - SP_SEG_ICO) * 0.5f, SP_SEG_ICO, SP_SEG_ICO },
              lapis ? "aj_pencil" : iconeAba(i), ic, ic, ic, a * alfaTxt);
    if (num.w > 0) {
      int novo;
      contaDaAba(i, &novo);
      // Sobre a pilula de acento a contagem vai na tinta do foco: no acento
      // ela sumiria justamente na aba em que o dedo esta.
      if (focada) {
        char b[16];
        int n = contaDaAba(i, &novo);
        snprintf(b, sizeof b, "%d", n > 99 ? 99 : n);
        num = txt_linha(TXT_ILHA_NUM, b, cor, cor, cor, 255);
        novo = 0;
      }
      txt_desenhar_alpha(num, x + SP_SEG_PX + SP_SEG_ICO + SP_SEG_NUM,
                         p.y + (p.h - num.h) * 0.5f,
                         a * (novo ? 1.0f : focada ? 0.7f : 0.45f));
    }
    x += w + SP_SEG_VAO;
  }
}

// A BARRA DE OPCOES. Cada pilula diz O QUE ela muda (rotulo pequeno em cima)
// e O QUE VALE AGORA (embaixo, na tinta principal): "Ordenar / Recentes". A
// ultima e a porta das categorias. Mesma superficie e mesmo foco das linhas,
// e por isso o mesmo vidro quando o vidro esta ligado.
static void chipTextos(int k, const char **cap, const char **val, const char **icone,
                       char *buf, size_t tam) {
  *cap = NULL; *val = NULL; *icone = NULL;
  switch (k) {
    case SPB_ORDEM:  *cap = "Ordenar"; *val = ORDEM_CURTO[sorg_ordem()]; break;
    case SPB_GRUPO:  *cap = "Agrupar"; *val = GRUPO_CURTO[sorg_grupo()]; break;
    case SPB_ESTILO: *cap = "Estilo";  *val = ESTILO_CURTO[sorg_estilo()];
                     *icone = ESTILO_ICONE[sorg_estilo()]; break;
    default:
      if (sorg_n_categorias() < 1) { *val = "Nova categoria"; *icone = "mais"; }
      else {
        *cap = "Categorias";
        snprintf(buf, tam, "%d", sorg_n_categorias());
        *val = buf; *icone = "aj_folders";
      }
      break;
  }
}

// Os chips da ilha (".chip" do mockup): 56 de altura, branco a 8 % (solido
// #24262c), o valor em 19 semibold a 85 % e o rotulo do que ele muda em
// caixa alta pequena a 45 % por cima — "ORDENAR / Recentes". Foco = pilula
// cheia no acento (botaoSup), como os chips da folha de Fontes.
static void desenhaBarra(float dx, float a) {
  float x = SP_X + dx + SP_ABAS_X;
  int k, n = nChips(), tf = ajustes_tinta_foco();
  float tinta = ajustes_acento_tinta(NULL, NULL, NULL);
  for (k = 0; k < n; k++) {
    const char *cap, *val, *icone;
    char buf[24];
    float f = animBarra[k], v = focoVisual(f), w, ix, cw = 0.0f;
    TxtLinha vr, vf;
    GfxRect r;
    chipTextos(k, &cap, &val, &icone, buf, sizeof buf);
    vr = txt_linha(TXT_ILHA_SEG, i18n(val), SPI_FG_R, SPI_FG_G, SPI_FG_B, 255);
    vf = txt_linha(TXT_ILHA_SEG, i18n(val), tf, tf, tf, 255);
    w = (float)vr.w;
    if (cap) { cw = caixaAltaIlha(cap, 255, 255, 255, -1.0f, 0.0f, 1.0f); if (cw > w) w = cw; }
    w += 44.0f + (icone ? 30.0f : 0.0f);
    r = (GfxRect){ x, SP_OPC_Y, w, SP_OPC_H };
    botaoSup(r, 0.5f, f, a);
    ix = x + 22.0f;
    if (icone) {
      float ic = anim_mistura(.85f * 243.0f / 255.0f, tinta, v);
      gfx_icone((GfxRect){ ix, SP_OPC_Y + (SP_OPC_H - 22.0f) * 0.5f, 22.0f, 22.0f },
                icone, ic, ic, ic, a);
      ix += 30.0f;
    }
    if (cap) {
      float bloco = 18.0f + 1.0f + (float)vr.h;
      float ty = SP_OPC_Y + (SP_OPC_H - bloco) * 0.5f;
      // O rotulo em caixa alta nao cruza para a tinta do foco por alfa: ele
      // e desenhado duas vezes (repouso a 45 %, foco na tinta), como o valor.
      caixaAltaIlha(cap, 255, 255, 255, ix, ty, a * 0.45f * (1.0f - v));
      caixaAltaIlha(cap, tf, tf, tf, ix, ty, a * 0.8f * v);
      txt_desenhar_alpha(vr, ix, ty + 19.0f, a * 0.85f * (1.0f - v));
      txt_desenhar_alpha(vf, ix, ty + 19.0f, a * v);
    } else {
      float ty = SP_OPC_Y + (SP_OPC_H - (float)vr.h) * 0.5f;
      txt_desenhar_alpha(vr, ix, ty, a * 0.85f * (1.0f - v));
      txt_desenhar_alpha(vf, ix, ty, a * v);
    }
    x += w + NV_CTRL_VAO;
  }
}

// A ESCOLHA por cima do painel: um veu sobre a lista, a folha no centro do
// painel com o titulo, a frase de apoio e as opcoes. A atual leva o "check".
static void desenhaPop(float a) {
  float e, h, w, x, y, topoL, janela;
  int i, vis, algumIcone;
  if (!pop) return;
  e = anim_suave(popEntrada) * a;
  vis = popN < POP_VISIVEIS ? popN : POP_VISIVEIS;
  janela = (float)vis * (POP_LINHA_H + POP_LINHA_VAO) - POP_LINHA_VAO;
  topoL = popSub[0] ? 118.0f : 84.0f;
  w = SP_W - 80.0f;
  h = topoL + janela + 28.0f;
  x = SP_X + 40.0f;
  y = SP_Y + (SP_H - h) * 0.42f + (1.0f - e) * 18.0f;
  gfx_cor((GfxRect){ SP_X, SP_Y, SP_W, SP_H }, SP_RAIO / SP_W, 0.0f, 0.0f, 0.0f, 0.50f * e);
  // A ESCOLHA E UMA ILHA por cima da ilha do painel: o mesmo material, um
  // degrau mais clara no solido para se separar dele sem aro.
  { GfxRect r = { x, y, w, h };
    const int vid = ajustes_vidro();
    gfx_rect((GfxRect){ x - 18.0f, y - 8.0f, w + 36.0f, h + 40.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, .42f * e);
    if (vid) gfx_vidro_folha(r, 28.0f / h, e);
    else gfx_cor(r, 28.0f / h, .098f, .102f, .118f, .99f * e);
    gfx_luz_canto(r, 28.0f / h, w * .25f, -h * .25f, w * .9f, 1, 1, 1, (vid ? .06f : .04f) * e); }
  // O TEXTO NA ESCALA DA ILHA: titulo 24 semibold, apoio 19 a 62 %, opcoes
  // 24 regular a 88 % (branco cheio na focada).
  { TxtLinha t = txtIlha(TXT_ILHA_NOME, popTitulo, w - 64.0f);
    txt_desenhar_alpha(t, x + 32.0f, y + 26.0f, e); }
  if (popSub[0]) {
    TxtLinha t = txtIlha(TXT_ILHA_SUB, popSub, w - 64.0f);
    txt_desenhar_alpha(t, x + 32.0f, y + 66.0f, e * 0.62f);
  }
  // Com icone em alguma opcao, todas reservam a coluna dele: texto alinhado
  // numa coluna so, como num menu, e nao em degraus.
  { int k; algumIcone = 0; for (k = 0; k < popN; k++) if (popL[k].icone) algumIcone = 1; }
  gfx_recorte(x, y + topoL - 8.0f, w, janela + 16.0f);
  for (i = 0; i < popN; i++) {
    float ry = y + topoL + (float)i * (POP_LINHA_H + POP_LINHA_VAO) - popRol;
    float f = popAnim[i], v = focoTexto(f), tx = x + 44.0f;
    int tf = ajustes_tinta_foco();
    float tinta = ajustes_acento_tinta(NULL, NULL, NULL);
    GfxRect r = { x + 20.0f, ry, w - 40.0f, POP_LINHA_H };
    if (ry + POP_LINHA_H < y + topoL - 8.0f || ry > y + topoL + janela + 8.0f) continue;
    if (f > 0.01f) superficieItem(r, SP_LINHA_RAIO / POP_LINHA_H, f, e);
    if (popL[i].icone) {
      float ic = anim_mistura(200.0f / 255.0f, tinta, v);
      gfx_icone((GfxRect){ tx, ry + (POP_LINHA_H - 24.0f) * 0.5f, 24.0f, 24.0f },
                popL[i].icone, ic, ic, ic, e);
    }
    if (algumIcone) tx += 38.0f;
    { TxtLinha rp = txtIlha(TXT_ILHA_CORPO, popL[i].rot, w - 160.0f);
      TxtLinha fo = txt_linha_corta(TXT_ILHA_CORPO, popL[i].rot, 255, 255, 255, 255, w - 160.0f);
      float fv = focoVisual(f);
      (void)tf;
      txt_desenhar_alpha(rp, tx, ry + (POP_LINHA_H - (float)rp.h) * 0.5f, e * 0.88f * (1.0f - fv));
      txt_desenhar_alpha(fo, tx, ry + (POP_LINHA_H - (float)fo.h) * 0.5f, e * fv); }
    if (popL[i].marcado) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      if (v > 0.5f) ar = ag = ab = tinta;
      gfx_icone((GfxRect){ r.x + r.w - 52.0f, ry + (POP_LINHA_H - 26.0f) * 0.5f, 26.0f, 26.0f },
                "check", ar, ag, ab, e);
    }
  }
  gfx_sem_recorte();
}



// Uma linha-botao compacta: pilula que inverte no foco, com um subtitulo
// opcional. A altura e menor que a de um card porque isto e uma acao da lista,
// nao conteudo para consumir a tela.
// Acao compacta da lista. A caixa neutra permanece visivel em repouso e recebe
// o mesmo foco tonal dos cartoes: sem contorno fino e sem glow, que em 1080p
// viravam um aro luminoso ao redor de uma acao pequena. `sub` e mantido porque
// a assinatura atende os dois antigos chamadores.
static void desenhaBotaoLinha(int i, float dx, float y, float alt, float a,
                              const char *titulo, const char *sub,
                              const char *icone, int primario) {
  float f = (i >= 0 && i < SP_MAX) ? animFoco[i] : 0.0f;
  float v = focoVisual(f), tinta = ajustes_acento_tinta(NULL, NULL, NULL);
  int tintaFoco = ajustes_tinta_foco();
  float h = NV_CTRL_H;
  TxtLinha repouso, foco;
  float grupo, x0, w;
  GfxRect r;
  (void)sub; (void)primario;
  // O CHIP DA ILHA (".chip": 56 de altura, 19 semibold a 85 %, 22 de recuo),
  // e nao mais a pilula de 25 px da tela de detalhe: as acoes da lista sao do
  // mesmo tamanho que os chips da folha de Fontes e da barra de Salvos. O
  // primario nao ganha corpo maior — e o foco que diz onde o OK cai.
  repouso = txt_linha(TXT_ILHA_SEG, titulo, SPI_FG_R, SPI_FG_G, SPI_FG_B, 255);
  foco = txt_linha(TXT_ILHA_SEG, titulo, tintaFoco, tintaFoco, tintaFoco, 255);
  grupo = (float)repouso.w + ((icone && icone[0]) ? 20.0f + 10.0f : 0.0f);
  w = grupo + 44.0f;
  if (w > SP_INTERNO) w = SP_INTERNO;
  r = (GfxRect){ SP_X + dx + SP_PAD, y + (alt - h) * 0.5f, w, h };
  botaoSup(r, 0.5f, f, a);
  x0 = r.x + 22.0f;
  if (icone && icone[0]) {
    float ic = anim_mistura(.85f * 243.0f / 255.0f, tinta, v);
    gfx_icone((GfxRect){ x0, r.y + (r.h - 20.0f) * 0.5f, 20.0f, 20.0f }, icone, ic, ic, ic, a);
    x0 += 30.0f;
  }
  txt_desenhar_alpha(repouso, x0, r.y + (r.h - (float)repouso.h) * 0.5f, a * 0.85f * (1.0f - v));
  txt_desenhar_alpha(foco, x0, r.y + (r.h - (float)foco.h) * 0.5f, a * v);
}


static void desenhaAgendaVazia(float dx, float y, float a) {
  float cx = SP_X + dx + SP_W * 0.5f;
  TxtLinha t1 = txt_linha(TXT_ILHA_NOME, i18n("Nada a caminho"), 243, 242, 239, 255);
  TxtLinha t2 = txt_linha_corta(TXT_ILHA_SUB, i18n("Siga uma série e os próximos episódios aparecem aqui."),
                                243, 242, 239, 255, SP_INTERNO);
  txt_desenhar_alpha(t1, cx - t1.w * 0.5f, y + 28.0f, a);
  txt_desenhar_alpha(t2, cx - t2.w * 0.5f, y + 74.0f, a * 0.42f);
}

// A FOLHA DE EDITAR ABAS: uma linha por aba que existe, com o olho (liga e
// desliga) e as setas de subir e descer. O foco e o de botao (pilula cheia no
// acento); aba desligada fica a 40 %. Sem mola: a folha tem cinco linhas.
#define SPE_LIN_H 82.0f
#define SPE_BTN   56.0f
static void desenhaEditar(float dx, float a) {
  int ids[SP_ABA_N], n = editIds(ids), i, c;
  float y = SP_LISTA_Y + SP_FOCO_AR + 6.0f, tinta = ajustes_acento_tinta(NULL, NULL, NULL);
  int tf = ajustes_tinta_foco();
  for (i = 0; i < n; i++) {
    int id = ids[i], lig = abaLiga[id];
    float x0 = SP_X + dx + SP_PAD, xb = SP_X + dx + SP_W - SP_PAD - SPE_BTN * 3.0f - 16.0f;
    float al = lig ? 1.0f : 0.4f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_NOME, i18n(rotuloAba(id)), SPI_FG_R, SPI_FG_G, SPI_FG_B, 255,
                                 xb - x0 - 60.0f);
    gfx_icone((GfxRect){ x0, y + (SPE_LIN_H - 28.0f) * 0.5f, 28.0f, 28.0f }, iconeAba(id), .9f, .9f, .9f, a * al);
    txt_desenhar_alpha(t, x0 + 48.0f, y + (SPE_LIN_H - t.h) * 0.5f, a * al);
    for (c = 0; c < 3; c++) {
      GfxRect r = { xb + (float)c * (SPE_BTN + 8.0f), y + (SPE_LIN_H - SPE_BTN) * 0.5f, SPE_BTN, SPE_BTN };
      int foc = (i == editLin && c == editCol);
      float ic = foc ? (float)tf / 255.0f : tinta;
      int sem = (c == 1 && i == 0) || (c == 2 && i == n - 1) || (c == 0 && lig && nLigadas() <= 1);
      botaoSup(r, 0.5f, foc ? 1.0f : 0.0f, a);
      gfx_icone((GfxRect){ r.x + 14.0f, r.y + 14.0f, 28.0f, 28.0f },
                c == 0 ? (lig ? "aj_eye" : "aj_eye-off") : c == 1 ? "aj_arrow-up" : "aj_arrow-down",
                ic, ic, ic, a * (sem ? 0.3f : 1.0f));
    }
    y += SPE_LIN_H;
  }
  { TxtLinha t = txt_linha(TXT_ILHA_SEG, i18n("Concluir"), SPI_FG_R, SPI_FG_G, SPI_FG_B, 255);
    TxtLinha tb = txt_linha(TXT_ILHA_SEG, i18n("Concluir"), tf, tf, tf, 255);
    int foc = editLin >= n;
    float w = (float)t.w + 44.0f;
    GfxRect r = { SP_X + dx + SP_PAD, y + 18.0f, w, NV_CTRL_H };
    botaoSup(r, 0.5f, foc ? 1.0f : 0.0f, a);
    txt_desenhar_alpha(foc ? tb : t, r.x + 22.0f, r.y + (r.h - (float)t.h) * 0.5f, a * (foc ? 1.0f : 0.85f)); }
}

static void desenhaVazio(float dx, float a) {
  float cx = SP_X + dx + SP_W * 0.5f;
  // DESTINO SIMKL SEM VINCULO (issue #110): a lista vazia muda seria lida como
  // "o Plan to Watch esta vazio". O que falta e o vinculo, e e isso que sai.
  const char *semSimkl = simkl_aviso_sem_vinculo(ajustes_salvos_no_simkl());
  TxtLinha t1 = txt_linha(TXT_ILHA_NOME, semSimkl ? semSimkl : "Nada salvo por enquanto",
                          243, 242, 239, 255);
  TxtLinha t2 = txt_linha_corta(TXT_ILHA_SUB,
      semSimkl ? "O + salva no Plan to Watch do Simkl, e ele ainda não está vinculado nesta TV."
               : "Aperte + em um filme ou série e ele aparece aqui.",
      243, 242, 239, 255, SP_INTERNO);
  gfx_icone((GfxRect){ cx - 30.0f, listaTopo() + 140.0f, 60.0f, 60.0f },
            "mais", 0.55f, 0.57f, 0.62f, a);
  txt_desenhar_alpha(t1, cx - t1.w * 0.5f, listaTopo() + 232.0f, a);
  txt_desenhar_alpha(t2, cx - t2.w * 0.5f, listaTopo() + 278.0f, a * 0.42f);
}

// 1 = o veu ja esta no fundo (a copia parada foi pintada com ele).
static int veuNoFundo;

// O veu usa a rampa CRUA e o painel a suavizada, pelo mesmo motivo do menu
// lateral: a medida da referencia para o escurecimento e uma reta, e um bloco
// deste tamanho parando de vez no fim do percurso le como corte.
// Tela REAL inteira: o veu vai tambem para a copia parada (spainel_fundo), que
// e pintada fora da camada ampliada.
static void veuInteiro(void) {
  ESCALA_REAL_INI();
  gfx_cor((GfxRect){ 0, 0, 1920.0f, 1080.0f }, 0.0f, 0, 0, 0, SP_VEU * entrada);
  ESCALA_REAL_FIM();
}

// O FUNDO PARADO. Com o painel inteiro na tela (a entrada terminou), o que
// esta atras dele — a home escurecida pelo veu — nao muda de um quadro para o
// outro: nao ha foco la, o trailer do destaque fecha com o painel aberto e a
// troca de arte do destaque so acontece no desenho da home. Entao ele e pintado
// UMA vez no FBO do snapshot (gfx_snap), com o veu por cima, e dali em diante o
// quadro e a copia (uma textura de tela cheia, sem SDF) e o painel.
//
// O PORQUE, MEDIDO na C9 do dono (24/09/2026): home sozinha a 60 fps; com o
// painel aberto 52-54 fps, `clr` de 21-30 ms nos piores quadros (o glClear
// esperando a GPU terminar o anterior) e fill=4,80x. A home inteira, mais um
// veu de tela cheia, mais o painel, eram redesenhados a cada quadro para
// mostrar uma imagem parada a 42 % de brilho — e gfx.c ja registrava que duas
// camadas de tela cheia derrubam a Mali-G71 para ~40 fps.
//
// `podeParar` e de quem chama (app.c: so na home, sem menu, detalhe ou outra
// camada no meio). `rev` e o que invalida a copia — app.c passa cat_revisao,
// que sobe quando as fileiras de baixo mudam. Sem FBO, ou com o painel
// entrando ou saindo, `fundo` e desenhado direto, como sempre foi.
// A COPIA E REFEITA TRES VEZES NOS PRIMEIROS SEGUNDOS (0,4 s, 1,5 s e 4 s
// depois da primeira) e depois fica. Arte da home que ainda estava subindo
// quando o painel abriu — um cartaz, o fundo do destaque logo depois do
// arranque — ficaria congelada como esqueleto; tres pinturas a mais custam
// tres quadros no ritmo de antes, e so na abertura.
//
// A COPIA TAMBEM E REFEITA ENQUANTO SAIU FALTANDO ARTE (relato do dono, 03/10:
// "quando a sidebar social ta aberta, depois de um tempo a home some e so
// volta quando eu fecho"). Parada, a home nao e desenhada, entao a arte dela
// esfria no cache e e a primeira a ser despejada quando as capas e os rostos
// do painel entram. Ate ai a copia segura a imagem. Mas a primeira
// republicacao do catalogo (Continuar assistindo refeito, sync) sobe
// cat_revisao, a copia e repintada com o cache sem a arte da home — e ficava
// assim: os pedidos daquele unico quadro caducavam (pedido velho, tex_cache.c)
// e nada mais repintava. REPRODUZIDO em tests/spainel_fundo_tempo.sh: arte
// nova no cache + republicar = 60 % -> 3 % de conteudo na faixa da home.
// Agora, se a pintura teve pedido sem textura (tex_n_falta), ela e refeita a
// cada SP_FUNDO_FALTA_MS — abaixo dos 200 ms que fazem um pedido caducar — ate
// sair inteira, com um teto para arte que nunca chega.
static const Uint32 SP_FUNDO_REFAZ_MS[] = { 400, 1500, 4000 };
#define SP_FUNDO_FALTA_MS   150
#define SP_FUNDO_FALTA_MAX  80     /* ~12 s de tentativas por invalidacao */
void spainel_fundo(int podeParar, unsigned rev, void (*fundo)(void *), void *ctx) {
  static int pronto, refeitas, faltou, tentativas, copiaPreta;
  static unsigned revPronto;
  static Uint32 desde, pintadoEm;
  int parado = podeParar && aberto && entrada >= 0.999f && gfx_snap_ok();
  const char *motivo = "";
  if (!parado) { pronto = 0; refeitas = 0; faltou = 0; tentativas = 0; }
  // A copia saiu preta (ver gfx_snap_vazio): enquanto o painel estiver aberto
  // a home e desenhada direto, como antes do fundo parado.
  if (!aberto) copiaPreta = 0;
  if (copiaPreta) parado = 0;
  else if (rev != revPronto && pronto) {
    // OS TEMPOS DE REPINTURA RECOMECAM A CADA REPUBLICACAO (relato do dono,
    // 05/10, na TCL: a home continuou sumindo com o painel aberto). A home
    // tem animacoes de ENTRADA presas ao relogio: a arte que chega esvanece
    // por 220 ms (revela_arte) e a fileira nova sobe com atraso por coluna
    // (revela_entra, ate ~1 s). A republicacao de um catalogo que ja estava
    // aberto (Continuar assistindo refeito a cada 10 min, sync) pinta a copia
    // NO QUADRO EM QUE ISSO COMECA: cartaz em opacidade 0, e `faltou` fica 0
    // porque a textura existe. As tres repinturas de 0,4/1,5/4 s so valiam
    // para a PRIMEIRA copia do painel; depois delas a copia ficava com a
    // fileira apagada ate o painel fechar. Reproduzido em
    // tests/spainel_fundo_tempo.sh (cartazes em branco no rodape da home
    // depois de republicar, com faltou=0 na linha [spainel] abaixo).
    pronto = 0; tentativas = 0; refeitas = 0; motivo = "revisao";
  }
  else if (rev != revPronto) { pronto = 0; tentativas = 0; }
  else if (pronto && faltou && tentativas < SP_FUNDO_FALTA_MAX &&
           SDL_GetTicks() - pintadoEm >= SP_FUNDO_FALTA_MS) {
    pronto = 0;
    tentativas++;
    motivo = "arte faltando";
  }
  else if (pronto && refeitas < (int)(sizeof SP_FUNDO_REFAZ_MS / sizeof *SP_FUNDO_REFAZ_MS) &&
           SDL_GetTicks() - desde >= SP_FUNDO_REFAZ_MS[refeitas]) {
    pronto = 0;
    refeitas++;
    motivo = "tempo";
  }
  if (parado && !pronto && !motivo[0]) motivo = "abertura";
  veuNoFundo = parado;
  if (parado && pronto) { gfx_snap_desenhar(); return; }
  if (parado) {
    gfx_snap_comecar();
    gfx_sem_recorte();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  { unsigned faltas0 = tex_n_falta;
    if (fundo) fundo(ctx);
    faltou = tex_n_falta != faltas0; }
  // UMA LINHA POR REPINTURA (as de "arte faltando" podem ser 80 por
  // republicacao: saem a primeira e a ultima). E o que diria quando e por que
  // a copia da home de tras foi refeita e se saiu inteira (faltou=0).
  if (parado && motivo[0] && (motivo[0] != 'a' || motivo[1] != 'r' || tentativas == 1 ||
                              tentativas == SP_FUNDO_FALTA_MAX)) {
    printf("[spainel] fundo repintado: motivo=%s rev=%u faltou=%d por-tempo=%d tentativas=%d%s\n",
           motivo, rev, faltou, refeitas, tentativas,
           tentativas == SP_FUNDO_FALTA_MAX && faltou ? " (DESISTIU: arte ainda faltando)" : "");
    fflush(stdout);
  }
  if (parado) {
    int mx = 0;
    veuInteiro();
    gfx_sem_recorte();
    if (gfx_snap_vazio(&mx)) {
      copiaPreta = 1;
      printf("[spainel] copia parada saiu PRETA (max=%d): desenhando a home direto\n", mx);
      fflush(stdout);
    }
    gfx_snap_terminar();
    if (copiaPreta) { veuNoFundo = 0; pronto = 0; if (fundo) fundo(ctx); return; }
    gfx_snap_desenhar();
    if (!refeitas) desde = SDL_GetTicks();
    pintadoEm = SDL_GetTicks();
    pronto = 1;
    revPronto = rev;
    fundosPintados++;
  }
}

// O RECORTE DO MORPH. Nascendo da ilha, o painel e um retangulo que cresce da
// pilula ate SP_X/SP_W; o conteudo fica no lugar FINAL e aparece por dentro do
// retangulo, sem ser escalado (nenhuma textura nova, nenhum FBO: o mesmo
// desenho de sempre com um recorte). Todo recorte de dentro do painel passa
// por aqui para nao vazar do retangulo enquanto ele cresce.
static int morfOn;
static GfxRect morfR;
static void spRecorte(float x, float y, float w, float h) {
  if (morfOn) {
    float x1 = x + w, y1 = y + h, mx1 = morfR.x + morfR.w, my1 = morfR.y + morfR.h;
    if (x < morfR.x) x = morfR.x;
    if (y < morfR.y) y = morfR.y;
    if (x1 > mx1) x1 = mx1;
    if (y1 > my1) y1 = my1;
    w = x1 - x; h = y1 - y;
  }
  gfx_recorte(x, y, w, h);
}
#define gfx_recorte spRecorte

static void desenharPainel(Uint32 agora);
static void spainel_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void spainel_desenhar(Uint32 agora) {
  { ESCALA_INI();
    spainel_desenharCorpo_(agora);
    ESCALA_FIM(); }
}
static void spainel_desenharCorpo_(Uint32 agora) {
  desenharPainel(agora);
  if (entrada < 0.002f) return;
  // POR CIMA DE TUDO DO PAINEL: a escolha aberta e o teclado de nome.
  desenhaPop(entrada);
  if (tecladoPara) teclado_desenhar(agora);
}

static void desenharPainel(Uint32 agora) {
  float a = entrada, x, y, xp, xl, al;
  int i;
  char buf[160];
  GfxRect forma = { SP_X, SP_Y, SP_W, SP_H };
  float raioForma = SP_RAIO / SP_W;
  morfOn = 0;
  if (entrada < 0.002f) return;

  // Entra deslizando da BORDA DIREITA. `x` e o deslocamento: em a=0 o painel
  // esta inteiro fora da tela.
  x = (1.0f - a) * (NV_TELA_W - SP_X);
  // ...ou NASCE DA ILHA: o retangulo vai da pilula (origem) ate o painel, o
  // raio em pixels de meia altura da pilula ate o do painel, e o conteudo
  // entra na segunda metade. Animacoes reduzidas: entrada ja e 0 ou 1 (anim.h).
  if (deIlha) {
    GfxRect o = (aberto || !destinoOk) ? origem : destino;
    float e = entradaP < 0.0f ? 0.0f : entradaP;   // a mola ja tem o repique (movimento.h)
    float rFim = raioForma * SP_H, rIni = o.h * 0.5f, ec = e > 1.0f ? 1.0f : e;
    forma.x = o.x + (SP_X - o.x) * e;  forma.y = o.y + (SP_Y - o.y) * e;
    forma.w = o.w + (SP_W - o.w) * e;  forma.h = o.h + (SP_H - o.h) * e;
    raioForma = (rIni + (rFim - rIni) * ec) / (forma.h > 1.0f ? forma.h : 1.0f);
    x = 0.0f;
    a = (entrada - 0.45f) / 0.55f;
    a = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
    morfOn = entrada < 0.999f;
    morfR = forma;
  }
  // Com o fundo parado o veu ja esta na copia (spainel_fundo).
  //
  // O VEU CONTINUA INTEIRO, e nao so em volta do painel. Tentei faixas em volta
  // (o painel e 94 % opaco, o veu debaixo dele e quase todo desperdicio): a
  // borda de todo gfx_cor e suavizada em ~0,6 % da ALTURA do retangulo, e numa
  // faixa de tela inteira isso sao ~6 px de meio-veu colados no contorno do
  // painel — um fio claro na comparacao pixel a pixel. Com o fundo parado o
  // veu ja nao custa nada por quadro; o que sobra sao a entrada, a saida e o
  // painel sobre outras telas.
  if (!veuNoFundo) veuInteiro();
  // Painel flutuante escuro e neutro; o veu separa a camada do conteudo sem
  // uma luz decorativa colorida competindo com posters e selos.
  { GfxRect p = { SP_X + x, SP_Y, SP_W, SP_H };
    float as = a;
    const int vid = ajustes_vidro();
    if (deIlha) { p = forma; as = entrada > 0.08f ? 1.0f : entrada / 0.08f; }
    // O PAINEL E UMA ILHA (Glass UI, mockup tela 3), no material da folha de
    // Fontes: sombra curta, miolo de vidro (gfx_vidro_folha) ou solido e uma
    // luz larga e fraca no canto de cima. Sem aro. Nascendo da pilula do
    // relogio a forma e a mesma, so o retangulo muda.
    gfx_sombra_sob((GfxRect){ p.x - 18.0f, p.y - 8.0f, p.w + 36.0f, p.h + 40.0f }, 1.0f, 0, 0.5f,
                   0, 0, 0, .38f * as, p, raioForma * p.h, vid ? 0.0f : .98f * as);
    if (vid) gfx_vidro_folha(p, raioForma, as);
    else gfx_cor(p, raioForma, .071f, .075f, .086f, .98f * as);
    gfx_luz_canto(p, raioForma, p.w * .25f, -p.h * .25f, p.w * .9f, 1, 1, 1, (vid ? .06f : .04f) * as);
  }

  // Tudo daqui para baixo fica preso ao painel: sem o recorte, a lista rolada
  // desenha por cima do cabecalho e por baixo da borda inferior.
  gfx_recorte(SP_X + x, SP_Y, SP_W, SP_H);

  // O CABECALHO DO MOCKUP (".kick" + ".ttl" + ".dsc"): o resumo da aba em
  // caixa alta pequena e espacada a 45 %, "Social" em 40 bold embaixo (o
  // disco de fechar do mockup saiu a pedido do dono, 03/10). Era a contagem
  // solta a direita das abas e nenhum titulo — o painel nao dizia o nome
  // dele (dono, 02/10, comparando com o mockup). As PARTES passam por i18n; a
  // juncao, nao (ver metaTexto).
  // recomenda_n() E NAO nRecs: com a pergunta de consentimento na tela a lista
  // local esta vazia de proposito, e escrever "0 recomendações" ao lado de uma
  // aba com o selo em 2 seria o painel se contradizendo em dois centimetros.
  if (aba == SP_ABA_AVISOS) {
    int n = avisos_lista_n(), nv = avisos_n_novos();
    if (nv > 0) snprintf(buf, sizeof buf, i18n("%d avisos · %d novos"), n, nv);
    else snprintf(buf, sizeof buf, "%d %s", n, i18n(n == 1 ? "aviso" : "avisos"));
  }
  else if (aba == SP_ABA_AGENDA) {
    if (nAg == 0) snprintf(buf, sizeof buf, "%s", i18n("Agenda"));
    else if (nAg == 1) snprintf(buf, sizeof buf, "%s", i18n("1 próximo episódio"));
    else snprintf(buf, sizeof buf, i18n("%d próximos episódios"), nAg);
  }
  else if (nCont > 0) {
    snprintf(buf, sizeof buf, "%d %s \xc2\xb7 %d %s", nLinhas,
             i18n(nLinhas == 1 ? "título" : "títulos"),
             nCont, i18n("para retomar"));
  }
  else snprintf(buf, sizeof buf, "%d %s", nLinhas, i18n(nLinhas == 1 ? "título" : "títulos"));
  if (editando) snprintf(buf, sizeof buf, "%s", i18n("Ligue ou desligue as abas e mude a ordem."));
  xp = x;
  // A TROCA DE ABA (e de/para a folha Editar). `tt` e a mola do corpo, `ft` o
  // fade curto (some na primeira metade do deslize). O resumo e o titulo novos
  // entram do lado da aba nova, o titulo antigo sai para o outro; so alfa e
  // posicao, as linhas de texto ficam no cache de text.c.
  { float tt = trocaT, ft = anim_clamp(tt / 0.55f, 0.0f, 1.0f);
    caixaAltaIlha(buf, SPI_FG_R, SPI_FG_G, SPI_FG_B,
                  SP_X + x + SP_PAD + (float)trocaDir * SP_TITULO_DESLIZE * 0.5f * (1.0f - tt),
                  SP_KICK_Y, a * 0.45f * ft);
    // O que vem abaixo (barra de opcoes e lista) entra deslizando.
    xl = (float)trocaDir * SP_TROCA_DESLIZE * (1.0f - tt);
    al = ft; }
  // O TITULO GRANDE E A ABA ABERTA (pedido do dono, 07/10); as abas, compactas,
  // ficam na mesma linha, a direita. O titulo cede se o idioma o faz comprido.
  { float maxT = SP_W - SP_ABAS_X - SP_PAD - abasLargura(0) - 28.0f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO,
                                 editando ? i18n("Editar abas") : i18n(rotuloAba(aba)),
                                 SPI_FG_R, SPI_FG_G, SPI_FG_B, 255, editando ? SP_INTERNO : maxT);
    { float tt = trocaT;
      if (tt < 0.999f && tituloAnt[0]) {
        TxtLinha o = txt_linha_corta(TXT_ILHA_TITULO, tituloAnt, SPI_FG_R, SPI_FG_G, SPI_FG_B, 255, maxT);
        float fo = 1.0f - anim_clamp(tt / 0.35f, 0.0f, 1.0f);
        if (fo > 0.0f) txt_desenhar_alpha(o, SP_X + x + SP_PAD - (float)trocaDir * SP_TITULO_DESLIZE * tt,
                                          SP_TIT_Y, a * fo);
      }
      txt_desenhar_alpha(t, SP_X + x + SP_PAD + (float)trocaDir * SP_TITULO_DESLIZE * (1.0f - tt),
                         SP_TIT_Y, a * anim_clamp((tt - 0.2f) / 0.5f, 0.0f, 1.0f)); }
    if (temAbas() && !editando) desenhaAbas(x, a, SP_W - SP_ABAS_X - SP_PAD - (float)t.w - 28.0f); }
  x = xp + xl; a *= al;   // dai para baixo e o corpo, que desliza na troca
  if (editando) { desenhaEditar(x, a); gfx_sem_recorte(); return; }
  if (temBarra()) desenhaBarra(x, a);


  if (aba == SP_ABA_AGENDA) {
    gfx_recorte(SP_X + xp, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
    y = listaTopo() + SP_FOCO_AR - scrollY;
    if (nAg == 0) desenhaAgendaVazia(x, y, a);
    // AS PECAS DA TELA AGENDA (agendaui.c): fio do tempo com um ponto por
    // episodio, cabecalho de grupo (Hoje / Esta semana / Mais tarde), linha com
    // arte, nome, "Qui 17 · T2 E4 · rede" e o sino. Mesmo codigo, uma lista so.
    if (nAg > 0) {
      float lsX = SP_X + x + SP_LINHA_X;
      agendaui_painel_fio(lsX, listaTopo(), SP_LISTA_BASE - listaTopo(), y + 10.0f,
                          y + topoDe(nAg) - 10.0f - 10.0f);
      for (i = 0; i < nAg; i++) {
        float cab = agAntes(i);
        if (cab > 0.0f) {
          if (y + cab >= listaTopo() && y <= SP_LISTA_BASE)
            agendaui_painel_grupo(lsX, SP_LINHA_W, agendaui_painel_grupo_de(agenda_lista(agIdx[i])), y);
          y += cab;
        }
        if (y + SPAG_H >= listaTopo() && y <= SP_LISTA_BASE)
          agendaui_painel_linha(lsX, SP_LINHA_W, agenda_lista(agIdx[i]), y,
                                (i < SP_MAX) ? focoVisual(animFoco[i]) : 0.0f);
        y += SPAG_H;
      }
    }
    y = listaTopo() + SP_FOCO_AR - scrollY + topoDe(nAg);
    if (y + SPS_H_ACAO >= listaTopo() && y <= SP_LISTA_BASE)
      desenhaBotaoLinha(nAg, x, y, SPS_H_ACAO, a, i18n("Abrir a agenda completa"), NULL, "aj_calendar", 1);
    gfx_sem_recorte();
    return;
  }
  if (aba == SP_ABA_AVISOS) {
    gfx_recorte(SP_X + xp, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
    avisos_lista_desenhar(SP_X + x + SP_LINHA_X, listaTopo() + SP_FOCO_AR - scrollY, SP_LINHA_W, a, foco);
    gfx_sem_recorte();
    return;
  }



  if (nLinhas == 0) { desenhaVazio(x, a); gfx_sem_recorte(); return; }

  // A lista rola dentro da propria janela, com um segundo recorte: o cabecalho
  // fica de fora dele e por isso nunca e coberto por um card subindo.
  gfx_recorte(SP_X + xp, listaTopo(), SP_W, SP_LISTA_BASE - listaTopo());
  y = listaTopo() + SP_FOCO_AR - scrollY;

  // OS ROTULOS DAS SECOES, onde montarLayout os pos. A categoria vazia leva
  // a dica de como por algo nela, no lugar das celulas que ainda nao tem.
  for (i = 0; i < nSecoes; i++) {
    float sy = y + secoes[i].y;
    float sa = secaoAlt(secoes[i].y <= 0.0f);
    float sh = sa + (secoes[i].vazia ? SP_VAZIA_H : 0.0f);
    if (sy + sh < listaTopo() || sy > SP_LISTA_BASE) continue;
    desenhaSecao(SP_X + x, sy, secoes[i].rot, a, secoes[i].y <= 0.0f);
    if (secoes[i].vazia) {
      TxtLinha t = txtIlha(TXT_ILHA_SUB,
          "Vazia. Segure OK num título e escolha \xe2\x80\x9cMover para categoria\xe2\x80\x9d.",
          SP_INTERNO);
      txt_desenhar_alpha(t, SP_X + x + SP_PAD, sy + sa + 6.0f, a * 0.5f);
    }
  }
  { int estilo = sorg_estilo();
    int ex = expIdx();
    int lista = estilo == SORG_ESTILO_LISTA;
    float extra = ex >= 0 ? (lista ? expExtra() : expExtraBloco()) : 0.0f;
    for (i = 0; i < nLinhas; i++) {
      float cy = y + linhas[i].ly +
                 (ex >= 0 && (lista ? i > ex : linhas[i].fila > linhas[ex].fila) ? extra : 0.0f);
      // Fora da janela nao custa texto nem textura: numa lista de 200 titulos
      // rasterizar as 195 invisiveis estouraria o orcamento de linhas por
      // quadro de text.c e as visiveis sairiam EM BRANCO (ver ctxmenu.c).
      if (cy + linhas[i].lh < listaTopo() || cy > SP_LISTA_BASE) continue;
      if (estilo == SORG_ESTILO_GRADE) desenhaCelulaGrade(i, x, cy, a);
      else if (estilo == SORG_ESTILO_PAISAGEM) desenhaCelulaPaisagem(i, x, cy, a);
      else desenhaLinha(i, x, cy, a);
    }
    if (ex >= 0 && !lista) {
      float passo = estilo == SORG_ESTILO_GRADE ? SPG_PASSO : SPP_PASSO;
      desenhaBlocoAberto(ex, x, y + linhas[ex].ly + passo - 12.0f, a);
    } }

  gfx_sem_recorte();
}

const char *spainel_foco_social(void) {
  return "";
}

int spainel_n_continuar(void) { if (listaVelha()) reconstruir(); return nCont; }
