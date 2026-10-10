// MENU DE CONTEXTO do cartaz, aberto SEGURANDO OK sobre um card da home.
//
// Nasceu do `posterHoldMenu` do app web (getPosterHoldMenuOptions, bundle
// 1.0.4): "Salvar/Remover dos Salvos" e, so em filme e serie, "Marcar como
// assistido/nao assistido". "Ver detalhes" SAIU (dono, 06/10/2026): o toque no
// cartaz ja abre o titulo, e no lugar dele o menu ganhou a EXTENSAO de
// informacoes ao lado (ctxinfo.h) — notas, sinopse, onde parou, amigos.
//
// Existe porque as duas acoes de biblioteca so tinham caminho DENTRO da tela de
// titulo: para marcar um filme como visto era preciso abrir o detalhe, esperar
// a rede e descer ate o botao. Segurando o OK sobre o cartaz sao dois toques.
#ifndef NV_CTXMENU_H
#define NV_CTXMENU_H
#include <SDL2/SDL.h>
#include "catalogo.h"
#include "listas.h"
#include "gfx.h"

// `indice` e a posicao no catalogo global.
// A integracao da pressao longa fica em home.c: ele mede NV_HOLD_MS no KEYUP e
// chama ctx_abrir somente quando o limiar foi atingido. Este modulo nao mede a
// tecla nem abre no KEYDOWN; assim o toque curto continua abrindo o detalhe e
// o modal recebe apenas o foco D-pad depois de estar visivel.
void ctx_abrir(int indice);
int  ctx_aberto(void);
void ctx_evento(const SDL_Event *e);
void ctx_atualizar(float dt, Uint32 agora);
void ctx_desenhar(Uint32 agora);
// O CARTAO DE INFORMACOES no ultimo quadro desenhado (o cartaz crescido, ou a
// ilha de informacoes quando nao ha cartaz): 1 e as caixas (tela virtual) do
// cartao e do menu, e o lado. Com cartaz, lado +1 = o menu a DIREITA do cartao,
// -1 = a esquerda (o mesmo vale sem cartaz: lado e onde fica o MENU). 0 = nao foi
// desenhado. Para os testes de layout.
int  ctx_info_caixa(GfxRect *info, GfxRect *menu, int *lado);
// O titulo do menu aberto (NULL no menu de lista/estilo ou fechado).
const CatItem *ctx_titulo(void);

// O MESMO MENU, aberto pelo painel de Salvos (salvospainel.c). A pressao longa
// la tambem e medida por quem conhece a linha, com o mesmo NV_HOLD_MS, e o
// painel chama isto no limiar. `titulo` e COPIADO: a linha do painel pode nao
// ter indice no catalogo (veio so da lista local). Opcoes: "Remover dos
// Salvos" (o mesmo OP_LISTA do cartaz: lista local + Trakt ou Simkl +
// espelho), "Mover para categoria" e, quando da, "Marcar como assistido". O
// toque na linha abre o titulo, como antes.
void ctx_abrir_salvo(const CatItem *titulo);
// O MESMO MENU NAS ABAS ATIVIDADE E AMIGOS do painel (modo social). Com IMDb
// sao as acoes do titulo do modo painel (sem "Mover para categoria") e, por
// ultimo, as `extras` da linha; sem IMDb (a linha de um amigo: `titulo` e o
// nome, `meta` a linha de apoio, `poster` a foto) sao SO as extras. A extra
// escolhida sai uma vez em ctx_pediu_extra() (o indice em `extras`); quem
// abriu faz o resto. `confirmar` = passa antes pela pagina de confirmacao
// (pergunta com %s = titulo/nome, texto, kicker; o botao e o proprio rotulo).
// Rotulos sao chaves de i18n; os ponteiros tem de viver ate o menu fechar.
#define CTX_EXTRAS_MAX 3
typedef struct {
  const char *rot, *icone;
  int confirmar;
  const char *kicker, *pergunta, *texto;
  // 1 = esta extra TOMA O LUGAR de "Marcar como assistido" e tambem o faz (so
  // marca, nunca desmarca; ja marcado no historico so passa adiante). A extra
  // so sai em ctx_pediu_extra() depois que o historico confirma — falhou: o menu
  // fica com o erro e o mesmo OK tenta de novo. Sem historico possivel
  // (serie fora do catalogo, sem IMDb) ela e so a propria extra.
  int juntaAssistido;
} CtxExtra;
void ctx_abrir_extras(const CatItem *titulo, const CtxExtra *extras, int n);
int  ctx_pediu_extra(void);
// A LINHA EXPANDIDA DO PAINEL DE SALVOS (dono, 06/10): o menu da lista de Salvos
// nao flutua — a linha em foco se abre DENTRO do painel (acordeao). ctx_inline_t
// e a mola 0..1 (0 = nao e o modo inline); ctx_inline_altura a altura da linha
// aberta (largura w); ctx_inline_desenhar desenha, dentro da linha em (x, y), o
// logo sobre a faixa de arte (de altura faixaH, que e de quem chama) e embaixo
// a meta, notas, sinopse e as pilulas de acao, tudo com o alfa `a`.
// ctx_inline_pedir(1) ANTES de ctx_abrir_salvo liga o modo (so a lista em linhas).
void  ctx_inline_pedir(int on);
float ctx_inline_t(void);
float ctx_inline_altura(float w, float faixaH);
void  ctx_inline_desenhar(float x, float y, float w, float faixaH, float a);
// 1 enquanto o menu aberto e o do painel: app.c o desenha POR CIMA do painel e
// entrega a ele as teclas que chegariam ao painel.
int  ctx_do_painel(void);
// IMDb do "Mover para categoria" pedido no modo painel, ou NULL. Consumido uma
// vez; o painel de Salvos abre a escolha de categoria dele.
const char *ctx_pediu_categoria(void);
// ESTILO DA FILEIRA. A home diz de qual fileira e o cartao ANTES de ctx_abrir;
// com uma chave que aceita forma (fil_estilos), o menu ganha "Estilo da
// fileira". NULL/"" = sem a opcao (destaque, Continuar assistindo).
void ctx_fileira(const char *chave, const char *titulo);
// O proximo ctx_abrir e o do cartao "Retomar agora": ganha "Dispensar" (solta o
// titulo da faixa, home_retomar_dispensar). Consumido por ctx_abrir; a home
// passa 0 nos outros cartoes.
void ctx_dispensar_retomar(int on);
// Menu SO da fileira, para o cartao que nao e titulo (pasta de colecao, pilha
// fechada do ranking): abre direto no modal de estilo — formas a esquerda,
// previa da fileira a direita (home_previa_fileira). Nao abre nada se a chave
// nao aceita forma.
void ctx_abrir_fileira(const char *chave, const char *titulo);
// O MESMO MENU, aberto pela BIBLIOTECA (biblioteca.c) segurando OK num cartaz.
// `r` e a caixa do cartaz na tela virtual (1920x1080) e `arte` o poster dele: o
// menu nasce ao lado e o poster volta por cima do veu, como na home. Sem arte
// nem caixa valida, o menu abre no meio, como ctx_abrir quando nao ha cartaz.
void ctx_abrir_cartaz(int indice, GfxRect r, const char *arte);
// O MENU DE UMA LISTA (Biblioteca > Listas, segurando OK num cartao): o resumo
// do que ha nela (nome, quantos titulos, a mistura de filmes e series, quem
// fez, as capas dos primeiros itens e a arte da pasta quando a fonte tem) e as
// acoes que existem de verdade: abrir, fixar na Biblioteca e levar para a Home.
// `l` e COPIADA. Comeca a baixar os itens (lst_abrir) para ter o que mostrar.
void ctx_abrir_lista(const LstLista *l);
// 1 UMA vez quando a pessoa escolheu "Abrir lista"; quem abriu faz o resto.
int  ctx_pediu_lista(void);
// 1 UMA vez quando o menu fixou/desafixou ou ligou/desligou a Home, para a
// grade de listas se refazer (a aba Fixadas perde a lista que saiu).
int  ctx_lista_alterou(void);
// Centro horizontal da barra "Segure OK para opções"; negativo = centro da tela.
void ctx_centro_dica(float cx);
#endif
