#include "trailer.h"
#include "plrui.h"
#include "text.h"
#include "layout.h"
#include "ajustes.h"
#include "trailerfonte.h"
#include "video.h"      // trailer_osd_desenhar reads video_pos/duracao on every target
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int    aberto, cheia, comSom;
static int    falhouUltima;
static GfxRect rect;
static char   fonteAtual[1024];
static Uint32 abertoEm;   // SDL_GetTicks da abertura da fonte atual, para o log
static int    dono;       // TRAILER_DONO_* (trailer.h)
static char   donoImdb[32];

void trailer_recorte(int vw, int vh, float z, float dw, float dh,
                     int *sx, int *sy, int *sw, int *sh) {
  int w, h;
  // QUADRO MATTED (Apple: 1920x804, 3836x1606) nao tem tarja embutida — a
  // tarja e o proprio plano encaixando 2.39 em 16:9. Encher a tela e
  // recortar as LATERAIS ate 16:9, sem o zoom fixo. Quadro 16:9 (IMDb, com
  // a tarja dentro da imagem) leva o zoom do ajuste. "Original" (1.0) nao
  // tira zoom nenhum em nenhum dos dois.
  if (z <= 1.001f) { w = vw; h = vh; }
  else if ((float)vw / (float)vh > 1.85f) { h = vh; w = (int)(vh * 16.0f / 9.0f); if (w > vw) w = vw; }
  else { w = (int)(vw / z); h = (int)(vh / z); }
  // E DEPOIS O "COVER" NO DESTINO. O plano estica a fonte no retangulo de
  // destino, sem guardar proporcao: a conta acima so servia a um destino
  // 16:9. O banner do layout Padrao e 1920x528 (3,6:1) e MEDIDO no log da C9
  // do dono (01/10/2026, 1.6.4): "fonte 244,138 1432x804 -> destino 0,0
  // 1920x528" — o trailer achatado pela metade na altura, a foto dele de
  // "so a imagem espremida". Corta-se a fonte na proporcao do destino, pelo
  // centro, como a arte do destaque faz (cover).
  //
  // Destino MAIS LARGO que o recorte do zoom: a largura volta a crescer ate
  // o quadro inteiro antes de se cortar altura. O zoom existe para tirar a
  // tarja de CIMA e de BAIXO; as laterais que ele cortava eram so para manter
  // 16:9. No banner a altura que sobra (528 de 1920) ja cai dentro da imagem,
  // sem tarja, e o trailer sai sem zoom a mais.
  if (dw > 1.0f && dh > 1.0f && w > 0 && h > 0) {
    float ad = dw / dh, af = (float)w / (float)h;
    if (af < ad * 0.995f) {
      float qw = (float)h * ad;
      if (qw > (float)vw) qw = (float)vw;
      w = (int)qw;
      h = (int)(qw / ad);
    } else if (af > ad * 1.005f) {
      w = (int)((float)h * ad);
    }
  }
  // PAR, como o player faz (player.c, aplicarAspecto): o escalonador
  // trabalha em 4:2:0 e origem ou tamanho impar da meio pixel de croma na
  // borda — e 803 de altura era o que saia daqui.
  w &= ~1; h &= ~1;
  *sw = w; *sh = h;
  *sx = ((vw - w) / 2) & ~1; *sy = ((vh - h) / 2) & ~1;
}

// LG (e Mac, onde video_iniciar devolve 0 e nada disto acontece): o trailer
// e um MP4 no plano de video da TV. O volume so pode ser mexido depois de o
// pipeline existir (mediaId), e o recorte que tira a tarja preta so depois de
// o quadro ter tamanho — os dois ficam pendentes e trailer_atualizar aplica.
#include "video.h"
static int volumePendente, recortePendente, pausado;
/* Prepared only means metadata/demuxer ready. A trailer must reach Playing
 * once before autoplay regards it as started. Sticky after that, so a paused
 * or buffering trailer keeps its existing image instead of closing the hole. */
static int tocouFonte;
// O recorte e REPETIDO nos primeiros segundos (ver reaplicarAte): o pipeline
// desta TV prende o plano em mais de um ponto depois do load (bind do ACB,
// `playing`), e um recorte pedido cedo demais pode ser engolido por um deles.
static Uint32 reaplicarAte, reaplicarEm, tocandoDesde;
static int quadroInteiroEnviado;
int trailer_suportado(void) {
#if defined(NV_LINUX_DESKTOP)
  return 0;
#else
  // O FRACASSO NAO TRAVA, MAS TEM TETO. O deploy mata o processo e relanca
  // em seguida, e o hub LS2 pode demorar para soltar o nome anterior; so o
  // SUCESSO e definitivo. O recuo e o teto moram em video_iniciar_auto
  // (lsregistro.h): sem teto, os registros 1720-1774 chamaram LSRegister a cada
  // 15 s pela sessao inteira — 1327 recusas de PERMISSION, que nao muda
  // sozinha. O play continua tentando (video_iniciar, pedido da pessoa).
  static int sabe = 0;
  if (sabe) return 1;
  if (video_iniciar_auto()) sabe = 1;
  return sabe;
#endif
}
static void nativoAplicar(void) {
  if (!aberto) return;
  if(video_tocando())tocouFonte=1;
  // O uMS setVolume funciona nesta TV (provado ao contrario: sem ele o
  // trailer tocou com som).
  if (volumePendente && video_ativo()) {
    video_volume(comSom ? 100 : 0); volumePendente = 0;
  }
  // O RECORTE SO DEPOIS DE `playing` + um respiro. E a ordem do player, a
  // unica em que o recorte comprovadamente pega nesta TV: la o modo salvo vai
  // ao plano no videoInfo como quadro INTEIRO e o zoom de verdade so e pedido
  // pela pessoa com o filme ja tocando. Pedido antes de tocar, o recorte era
  // aceito (-> 1) e ignorado (20/09/2026, meia noite de tentativas).
  if (recortePendente && video_pronto() && video_largura() > 0 && video_altura() > 0 &&
      !video_tocando() && !quadroInteiroEnviado) {
    video_janela_fonte(0, 0, video_largura(), video_altura(),
                       (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h);
    quadroInteiroEnviado = 1;
  }
  if (recortePendente && video_tocando() && !tocandoDesde) tocandoDesde = SDL_GetTicks();
  if (recortePendente && video_pronto() && video_largura() > 0 && video_altura() > 0 &&
      tocandoDesde && SDL_GetTicks() - tocandoDesde >= 800) {
    int sw, sh, sx, sy;
    trailer_recorte(video_largura(), video_altura(), ajustes_trailer_zoom(),
                    rect.w, rect.h, &sx, &sy, &sw, &sh);
    if (video_recorte_fonte_trailer())
      video_janela_fonte(sx, sy, sw, sh,
                         (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h);
    recortePendente = 0;
    if (!reaplicarAte) { reaplicarAte = SDL_GetTicks() + 6000; reaplicarEm = SDL_GetTicks() + 1500; }
  }
  if (reaplicarAte && SDL_GetTicks() >= reaplicarEm) {
    if (SDL_GetTicks() >= reaplicarAte) reaplicarAte = 0;
    else { reaplicarEm = SDL_GetTicks() + 1500; video_recorte_reaplicar(); }
  }
}

void trailer_abrir(const char *fonte, GfxRect r, int som, int modoCheia) {
  int nova;
  if (!trailer_suportado() || !fonte || !fonte[0]) return;
  // SAMSUNG E SEMPRE MUDO, tambem em tela cheia (dono, 22/09/2026: "trailer
  // fica mudo"). A trava fica AQUI, no unico caminho ate o elemento, e nao so
  // em quem chama: um `som` 1 esquecido num chamador novo desmutaria o embed
  // do YouTube (mute=0) e voltaria a prometer som onde a Apple nao tem.
  nova = strcmp(fonteAtual, fonte) != 0;
  if (nova) falhouUltima = 0;
  if (nova) {
    if (!video_tocar(fonte)) return;
    tocouFonte = 0;
    volumePendente = 1; recortePendente = 1; pausado = 0; reaplicarAte = 0;
    tocandoDesde = 0; quadroInteiroEnviado = 0;
  } else if (comSom != som) volumePendente = 1;
  video_janela((int)r.x, (int)r.y, (int)r.w, (int)r.h);
  if (!nova) recortePendente = 1;
  if (nova) {
    snprintf(fonteAtual, sizeof fonteAtual, "%s", fonte);
    abertoEm = SDL_GetTicks();
    printf("[trailer] %.60s %s%s\n", fonte, modoCheia ? "tela cheia" : "no fundo", som ? " com som" : " mudo");
    fflush(stdout);
  }
  rect = r; aberto = 1; cheia = modoCheia; comSom = som;
  nativoAplicar();
}


void trailer_fechar(void) {
  if (!aberto) return;
  video_parar();
  aberto = 0; cheia = 0; fonteAtual[0] = 0;
  tocouFonte = 0;
  dono = TRAILER_DONO_NENHUM; donoImdb[0] = 0;
}

void trailer_marcar_dono(int d, const char *imdb) {
  if (!aberto) return;
  dono = d;
  snprintf(donoImdb, sizeof donoImdb, "%s", imdb ? imdb : "");
}
int trailer_dono(void) { return aberto ? dono : TRAILER_DONO_NENHUM; }

int trailer_continuar(GfxRect r, int som) {
  char f[sizeof fonteAtual];
  if (!aberto || cheia || !fonteAtual[0]) return 0;
  // Copia: trailer_abrir le `fonte` e grava fonteAtual. Mesma fonte = nao e
  // "nova": so o volume e o retangulo (com o recorte do zoom) mudam.
  snprintf(f, sizeof f, "%s", fonteAtual);
  trailer_abrir(f, r, som, 0);
  printf("[trailer] continua %.60s %s\n", f, comSom ? "com som" : "mudo");
  fflush(stdout);
  return 1;
}

int trailer_aberto(void)  { return aberto; }
int trailer_com_som(void) { return aberto && comSom; }
int trailer_cheia(void)   { return aberto && cheia; }
int trailer_tocando(void) {
  if(!aberto||!video_pronto()||video_falhou()||video_terminou()||video_conflito_recurso())return 0;
  if(video_tocando())tocouFonte=1;
  return tocouFonte;
}
int trailer_mostra_video(void) {
  return 1;
}
GfxRect trailer_retangulo(void) { return rect; }
int trailer_estado(void) {
  return -9;
}

int trailer_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !cheia || e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { trailer_fechar(); return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    pausado = !pausado;
    video_pausar(pausado);
    return 1;
  }
  return 1;   // em tela cheia o resto do teclado nao vai a pagina de tras
}

void trailer_atualizar(Uint32 agora) {
  (void)agora;
  if (!aberto) return;
  video_bombear();
  nativoAplicar();
  // Acabou ou a fonte falhou: fecha e a pagina volta a arte.
  // conflito: outro app (YouTube) tomou o plano de video — fecha, fica a arte.
  if (video_terminou() || video_falhou() || video_conflito_recurso()) {
    falhouUltima = video_falhou() || video_conflito_recurso() ? 1 : 0;
    trailer_fechar();
  }
}

int trailer_falhou(void) { return falhouUltima; }

// O OSD DO TRAILER EM TELA CHEIA (Glass UI, mockup de 03/10, proposta
// aprovada): so com o trailer PAUSADO — o trailer e o proprio heroi. A pilula
// da ilha diz o que e ("Trailer · titulo"), a barra fina do player embaixo e
// o tempo; o unico botao e Continuar (o que o OK faz). Quem desenha a tela
// cheia (detail.c) chama isto por cima do furo.
int trailer_pausado(void) {
  return aberto && cheia && pausado;
}
void trailer_osd_desenhar(const char *titulo, float a) {
  double pos = video_pos(), dur = video_duracao();
  if (!trailer_pausado() || a < 0.01f) return;
  gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, 260.0f }, 1, 1.38f, 1.0f, 0.52f * a);
  gfx_veu_css((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * a);
  { TxtLinha l1 = txt_linha(TXT_ILHA_NOME, "Trailer", 243, 242, 239, 255);
    TxtLinha l2 = txt_linha_corta(TXT_ILHA_CORPO, titulo ? titulo : "", 243, 242, 239, 179, 900.0f);
    float w = 24.0f + 24.0f + 12.0f + (float)l1.w + (l2.w ? 12.0f + 1.0f + 12.0f + (float)l2.w : 0.0f) + 24.0f;
    GfxRect p = { 96.0f, 48.0f, w, 56.0f };
    float x = p.x + 24.0f, yc = p.y + 28.0f;
    plrui_material(p, 28.0f, 0, a);
    gfx_icone((GfxRect){ x, yc - 12.0f, 24.0f, 24.0f }, "pl_clapperboard", 1, 1, 1, a);
    x += 36.0f;
    txt_desenhar_alpha(l1, x, yc - (float)l1.h * 0.5f, a);
    x += (float)l1.w + 12.0f;
    if (l2.w) { plrui_sep(x, yc, a); txt_desenhar_alpha(l2, x + 13.0f, yc - (float)l2.h * 0.5f, a); } }
  if (dur > 1.0) {
    char t1[24], d[24], t2[32];
    plrui_barra(96.0f, 1000.0f, NV_TELA_W - 192.0f, (float)(pos / dur),
                (float)(video_buffer_fim() / dur), 0, NULL, 0, a);
    plrui_tempo(t1, sizeof t1, pos); plrui_tempo(d, sizeof d, dur);
    snprintf(t2, sizeof t2, "/ %s", d);
    { TxtLinha l2 = txt_linha(TXT_ILHA_NOME, t2, 243, 242, 239, 128);
      TxtLinha l1 = txt_linha(TXT_ILHA_NOME, t1, 243, 242, 239, 235);
      float xr = NV_TELA_W - 96.0f - l2.w;
      txt_desenhar_alpha(l2, xr, 916.0f, a);
      txt_desenhar_alpha(l1, xr - 8.0f - l1.w, 916.0f, a); }
  }
  { TxtLinha l = txt_linha(TXT_G21B, "Continuar", 0, 0, 0, 255);
    GfxRect b = { 96.0f, 896.0f, 22.0f + 28.0f + 12.0f + (float)l.w + 28.0f, 68.0f };
    float k = plrui_tinta() / 255.0f;
    plrui_pilula_foco(b, a);
    gfx_icone((GfxRect){ b.x + 22.0f, b.y + 20.0f, 28.0f, 28.0f }, "pl_play-f", k, k, k, a);
    l = txt_linha(TXT_G21B, "Continuar", plrui_tinta(), plrui_tinta(), plrui_tinta(), 255);
    txt_desenhar_alpha(l, b.x + 22.0f + 28.0f + 12.0f, b.y + (68.0f - (float)l.h) * 0.5f, a); }
}
#ifdef NV_SHOT_HOOKS
void trailer_shot_pausado(int sim) { aberto = cheia = sim; pausado = sim; }
#endif
