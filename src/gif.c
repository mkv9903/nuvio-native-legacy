#include "gif.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GIF_TELA_MAX (1280 * 1280)
typedef struct {
  int esq, topo, larg, alt;
  size_t gce, gceN, ini, fim;
} GifQuadro;
typedef struct GifDec GifDec;
static void gif_dec_fechar(GifDec *d);
static int gif_dec_primeiro(GifDec *d);

// Pula uma cadeia de sub-blocos: cada um comeca com o proprio tamanho, e a
// cadeia acaba num tamanho zero. Devolve a posicao depois do terminador, ou 0
// quando o arquivo acaba no meio (arquivo truncado nao vira laco infinito).
static size_t pularSubBlocos(const unsigned char *b, size_t n, size_t p) {
  while (p < n) {
    size_t len = b[p++];
    if (!len) return p;
    if (p + len > n) return 0;
    p += len;
  }
  return 0;
}

// Cabecalho + descritor de tela logica + paleta global, em bytes. 0 se o
// arquivo nem chega a ser um GIF.
static size_t cabecalhoN(const unsigned char *b, size_t n) {
  size_t p;
  if (n < 14 || memcmp(b, "GIF8", 4)) return 0;
  // 6 da assinatura + 7 do descritor de tela; o bit 7 do quinto byte do
  // descritor diz se ha paleta global, que ocupa 3 * 2^(N+1) bytes depois.
  p = 13;
  if (b[10] & 0x80) p += (size_t)3 << ((b[10] & 7) + 1);
  return p <= n ? p : 0;
}

static int gif_mapear(const unsigned char *b, size_t n, GifQuadro *q, int max) {
  size_t p, gce = 0, gceN = 0;
  int achados = 0;
  if (!b || !q || max < 1) return 0;
  p = cabecalhoN(b, n);
  if (!p) return 0;

  while (p < n && achados < max) {
    unsigned char sep = b[p];
    if (sep == 0x3B) break;                       // fim do arquivo
    if (sep == 0x21) {                            // extensao: rotulo + blocos
      size_t ini = p, fimExt;
      if (p + 2 > n) break;
      fimExt = pularSubBlocos(b, n, p + 2);
      if (!fimExt) break;
      // O CONTROLE GRAFICO VALE PARA O PROXIMO QUADRO, e e dele que saem o
      // atraso e o metodo de descarte. Guardado inteiro (e nao so
      // interpretado) porque o decodificador le dele a cor transparente.
      if (b[p + 1] == 0xF9) { gce = ini; gceN = fimExt - ini; }
      p = fimExt;
      continue;
    }
    if (sep != 0x2C) break;                       // byte inesperado: desiste
    if (p + 10 > n) break;                        // separador + 9 do descritor
    {
      GifQuadro *d = &q[achados];
      unsigned char campos = b[p + 9];
      size_t dados = p + 10;
      d->esq  = b[p + 1] | (b[p + 2] << 8);
      d->topo = b[p + 3] | (b[p + 4] << 8);
      d->larg = b[p + 5] | (b[p + 6] << 8);
      d->alt  = b[p + 7] | (b[p + 8] << 8);
      if (campos & 0x80) dados += (size_t)3 << ((campos & 7) + 1);  // paleta local
      if (dados >= n) break;
      dados = pularSubBlocos(b, n, dados + 1);    // +1: tamanho minimo do codigo
      if (!dados) break;                          // cadeia nao fechou: nao e quadro
      d->ini = p; d->fim = dados;
      d->gce = gce; d->gceN = gceN;
      gce = 0; gceN = 0;
      achados++;
      p = dados;
    }
  }
  return achados;
}

typedef struct {
  const unsigned char *b;
  size_t n, p;            // fim da faixa do quadro; proximo byte
  unsigned sub;           // bytes que faltam no sub-bloco corrente
  int fim;                // a cadeia acabou (terminador ou fim da faixa)
  uint32_t bits;
  int nbits;
} Leitor;

struct GifDec {
  unsigned char *b;
  GifQuadro *q;
  int telaW, telaH, saidaW, saidaH;
  uint32_t *tela;         // RGBA composto, telaW*telaH
  unsigned char *idx;     // indices de um quadro, larg*alt
  size_t idxCap;
  uint32_t *saida;        // RGBA reduzido, saidaW*saidaH
  int *colX, *linY;       // onde comeca cada coluna/linha da saida na tela
  // Tabela do LZW: 4096 codigos de ate 12 bits (ver lzw()).
  uint32_t pos[4096];
  uint16_t compr[4096];
};

static int lerByte(Leitor *l) {
  if (l->fim) return -1;
  while (!l->sub) {
    if (l->p >= l->n) { l->fim = 1; return -1; }
    l->sub = l->b[l->p++];
    if (!l->sub) { l->fim = 1; return -1; }
  }
  if (l->p >= l->n) { l->fim = 1; return -1; }
  l->sub--;
  return l->b[l->p++];
}

// LZW do GIF: codigos de largura variavel (minimo+1 ate 12 bits), bit menos
// significativo primeiro, com CLEAR e END. Escreve ate `total` indices em
// `dst` e devolve quantos escreveu. Codigo fora da tabela (arquivo corrompido)
// PARA o quadro ali: o resto dele nao desenha, como no Chromium.
//
// A TABELA GUARDA ONDE CADA STRING JA ESTA NA SAIDA, e nao a cadeia de
// prefixos: toda entrada nova e "a string do codigo anterior + o primeiro
// simbolo do atual", e essa sequencia acabou de ser escrita em `dst` a partir
// de onde o anterior foi escrito. Emitir um codigo vira copiar `compr` bytes
// de tras — como no LZ77 —, em vez de andar a cadeia um byte por vez. No caso
// KwKwK (o codigo pedido e o que esta sendo criado) a copia sobrepoe o
// destino em um byte, e por isso ela e feita em ordem, byte a byte.
static size_t lzw(GifDec *d, Leitor *l, int minimo, unsigned char *dst, size_t total) {
  int clear, eoi, prox, larg, anterior = -1;
  size_t pos = 0, posAnt = 0;
  uint32_t mascara;
  if (minimo < 1 || minimo > 11) return 0;
  clear = 1 << minimo; eoi = clear + 1; prox = eoi + 1; larg = minimo + 1;
  mascara = (1u << larg) - 1;
  while (pos < total) {
    int cod;
    while (l->nbits < larg) {
      int c;
      if (l->sub && l->p < l->n) { l->sub--; c = l->b[l->p++]; }
      else if ((c = lerByte(l)) < 0) return pos;
      l->bits |= (uint32_t)c << l->nbits;
      l->nbits += 8;
    }
    cod = (int)(l->bits & mascara);
    l->bits >>= larg;
    l->nbits -= larg;
    if (cod < clear) {                        // raiz: um simbolo so
      if (anterior >= 0 && prox < 4096) {
        d->pos[prox] = (uint32_t)posAnt;
        d->compr[prox] = (uint16_t)(d->compr[anterior] + 1);
        if (++prox == (1 << larg) && larg < 12) { larg++; mascara = (1u << larg) - 1; }
      }
      d->compr[cod] = 1;
      posAnt = pos;
      dst[pos++] = (uint8_t)cod;
      anterior = cod;
      continue;
    }
    if (cod == clear) {
      prox = eoi + 1; larg = minimo + 1; mascara = (1u << larg) - 1; anterior = -1;
      continue;
    }
    if (cod == eoi || anterior < 0 || cod > prox || (cod == prox && prox >= 4096)) break;
    if (prox < 4096) {
      d->pos[prox] = (uint32_t)posAnt;
      d->compr[prox] = (uint16_t)(d->compr[anterior] + 1);
      if (++prox == (1 << larg) && larg < 12) { larg++; mascara = (1u << larg) - 1; }
    }
    { size_t n = d->compr[cod], de = d->pos[cod], k;
      if (n > total - pos) n = total - pos;
      if (de + n <= pos) memcpy(dst + pos, dst + de, n);
      else for (k = 0; k < n; k++) dst[pos + k] = dst[de + k];
      posAnt = pos;
      pos += n;
    }
    anterior = cod;
  }
  return pos;
}

// Linha real da n-esima linha decodificada de um quadro entrelacado: passadas
// de 8 em 8 (a partir de 0), de 8 em 8 (de 4), de 4 em 4 (de 2), de 2 em 2 (de 1).
static int linhaEntrelacada(int n, int alt) {
  int p1 = (alt + 7) / 8, p2 = (alt + 3) / 8, p3 = (alt + 1) / 4;
  if (n < p1) return n * 8;
  n -= p1;
  if (n < p2) return 4 + n * 8;
  n -= p2;
  if (n < p3) return 2 + n * 4;
  n -= p3;
  return 1 + n * 2;
}

static void copiarArea(uint32_t *dst, const uint32_t *src, int W, int x0, int y0, int x1, int y1) {
  int y;
  for (y = y0; y < y1; y++)
    memcpy(dst + (size_t)y * W + x0, src + (size_t)y * W + x0, (size_t)(x1 - x0) * 4);
}

// O retangulo do quadro, cortado a tela logica. 0 quando nao sobra nada.
static int areaDe(const GifDec *d, const GifQuadro *q, int *x0, int *y0, int *x1, int *y1) {
  *x0 = q->esq < d->telaW ? q->esq : d->telaW;
  *y0 = q->topo < d->telaH ? q->topo : d->telaH;
  *x1 = q->esq + q->larg < d->telaW ? q->esq + q->larg : d->telaW;
  *y1 = q->topo + q->alt < d->telaH ? q->topo + q->alt : d->telaH;
  return *x1 > *x0 && *y1 > *y0;
}

// MEDIA DE CAIXA: cada pixel da saida e a media dos pixels da tela que ele
// cobre. A cor e a media so dos OPACOS (no GIF o alfa e 0 ou 255, e somar o
// preto dos transparentes escureceria a borda); o alfa e a fracao opaca.
// Recalcula so o retangulo [ox0,ox1) x [oy0,oy1) da saida.
static void reduzir(GifDec *d, int ox0, int oy0, int ox1, int oy1) {
  int ox, oy;
  if (d->saidaW == d->telaW && d->saidaH == d->telaH) {
    copiarArea(d->saida, d->tela, d->telaW, ox0, oy0, ox1, oy1);
    return;
  }
  for (oy = oy0; oy < oy1; oy++) {
    int sy0 = d->linY[oy], sy1 = d->linY[oy + 1];
    uint32_t *o = d->saida + (size_t)oy * d->saidaW;
    for (ox = ox0; ox < ox1; ox++) {
      int sx0 = d->colX[ox], sx1 = d->colX[ox + 1], sx, sy;
      unsigned r = 0, g = 0, bl = 0, cnt = 0, area = (unsigned)((sx1 - sx0) * (sy1 - sy0));
      for (sy = sy0; sy < sy1; sy++) {
        const uint32_t *s = d->tela + (size_t)sy * d->telaW;
        for (sx = sx0; sx < sx1; sx++) {
          uint32_t c = s[sx];
          if (c >> 24) { r += c & 0xFF; g += (c >> 8) & 0xFF; bl += (c >> 16) & 0xFF; cnt++; }
        }
      }
      if (!cnt) { o[ox] = 0; continue; }
      o[ox] = (r / cnt) | ((g / cnt) << 8) | ((bl / cnt) << 16) |
              ((cnt == area ? 255u : cnt * 255u / area) << 24);
    }
  }
}

static void gif_tamanho_saida(int telaW, int telaH, int largAlvo, int *saidaW, int *saidaH) {
  int w = largAlvo, h;
  if (telaW < 1 || telaH < 1) { *saidaW = *saidaH = 0; return; }
  if (w < 1 || w > telaW) w = telaW;
  h = (int)((double)telaH * w / telaW + 0.5);
  if (h < 1) h = 1;
  if (h > telaH) h = telaH;
  *saidaW = w; *saidaH = h;
}

static void gif_dec_fechar(GifDec *d) {
  if (!d) return;
  free(d->b); free(d->q); free(d->tela); free(d->idx);
  free(d->saida); free(d->colX); free(d->linY);
  free(d);
}

static GifDec *decAbrir(unsigned char *b, size_t n, int saidaW, int saidaH) {
  GifDec *d;
  GifQuadro *q;
  int telaW, telaH, i;
  if (!b) return NULL;
  if (n < 14 || memcmp(b, "GIF8", 4)) { free(b); return NULL; }
  telaW = b[6] | (b[7] << 8);
  telaH = b[8] | (b[9] << 8);
  if (telaW < 1 || telaH < 1 || (long)telaW * telaH > GIF_TELA_MAX ||
      saidaW < 1 || saidaH < 1 || saidaW > telaW || saidaH > telaH) { free(b); return NULL; }
  q = (GifQuadro *)malloc(sizeof *q);
  if (!q) { free(b); return NULL; }
  if (!gif_mapear(b, n, q, 1)) { free(q); free(b); return NULL; }
  d = (GifDec *)calloc(1, sizeof *d);
  if (!d) { free(q); free(b); return NULL; }
  d->b = b; d->q = q;
  d->telaW = telaW; d->telaH = telaH; d->saidaW = saidaW; d->saidaH = saidaH;
  d->tela = (uint32_t *)calloc((size_t)telaW * telaH, 4);
  d->saida = (uint32_t *)calloc((size_t)saidaW * saidaH, 4);
  d->colX = (int *)malloc(sizeof(int) * (size_t)(saidaW + 1));
  d->linY = (int *)malloc(sizeof(int) * (size_t)(saidaH + 1));
  if (!d->tela || !d->saida || !d->colX || !d->linY) {
    gif_dec_fechar(d);
    return NULL;
  }
  for (i = 0; i <= saidaW; i++) d->colX[i] = (int)((long)i * telaW / saidaW);
  for (i = 0; i <= saidaH; i++) d->linY[i] = (int)((long)i * telaH / saidaH);
  return d;
}

unsigned char *gif_primeiro_rgba(const unsigned char *b, size_t n, int largMax,
                                 int *w, int *h, int *originalW, int *originalH) {
  int ow, oh, sw, sh;
  unsigned char *copia, *rgba = NULL;
  GifDec *d;
  if (w) *w = 0;
  if (h) *h = 0;
  if (originalW) *originalW = 0;
  if (originalH) *originalH = 0;
  if (!b || n < 14 || n > 32u * 1024u * 1024u || memcmp(b, "GIF8", 4)) return NULL;
  ow = b[6] | (b[7] << 8); oh = b[8] | (b[9] << 8);
  if (ow < 1 || oh < 1 || (long)ow * oh > GIF_TELA_MAX) return NULL;
  gif_tamanho_saida(ow, oh, largMax > 0 ? largMax : ow, &sw, &sh);
  copia = malloc(n);
  if (!copia) return NULL;
  memcpy(copia, b, n);
  d = decAbrir(copia, n, sw, sh);
  if (!d) return NULL;
  if (gif_dec_primeiro(d) == 0) {
    size_t bytes = (size_t)sw * sh * 4;
    rgba = malloc(bytes);
    if (rgba) {
      memcpy(rgba, d->saida, bytes);
      if (w) *w = sw;
      if (h) *h = sh;
      if (originalW) *originalW = ow;
      if (originalH) *originalH = oh;
    }
  }
  gif_dec_fechar(d);
  return rgba;
}

static int gif_dec_primeiro(GifDec *d) {
  const GifQuadro *q;
  const unsigned char *b, *cores;
  size_t p, total, feitos = 0;
  uint32_t pal[256];
  int k, palN = 0, transp = -1, entrel, larg, alt, campos;
  int x0, y0, x1, y1, tem;
  if (!d) return -1;
  q = &d->q[0];
  b = d->b;
  tem = areaDe(d, q, &x0, &y0, &x1, &y1);

  // PALETA: a local do quadro, senao a global. Arquivo sem nenhuma, ou indice
  // alem da paleta, nao desenha. gif_mapear ja conferiu que a local cabe.
  campos = b[q->ini + 9];
  entrel = (campos & 0x40) != 0;
  p = q->ini + 10;
  if (campos & 0x80) {
    palN = 1 << ((campos & 7) + 1);
    cores = b + p;
    p += (size_t)palN * 3;
  } else {
    if (b[10] & 0x80) palN = 1 << ((b[10] & 7) + 1);
    cores = b + 13;
  }
  for (k = 0; k < palN; k++)
    pal[k] = cores[k * 3] | ((uint32_t)cores[k * 3 + 1] << 8) |
             ((uint32_t)cores[k * 3 + 2] << 16) | 0xFF000000u;
  if (q->gceN >= 7 && (b[q->gce + 3] & 1)) transp = b[q->gce + 6];

  larg = q->larg; alt = q->alt;
  total = (size_t)larg * (size_t)alt;
  if (tem && total && p < q->fim) {
    if (total > d->idxCap) {
      unsigned char *novo = (unsigned char *)realloc(d->idx, total);
      if (novo) { d->idx = novo; d->idxCap = total; }
    }
    if (total <= d->idxCap) {
      Leitor l;
      memset(&l, 0, sizeof l);
      l.b = b; l.n = q->fim; l.p = p + 1;
      feitos = lzw(d, &l, b[p], d->idx, total);
    }
  }
  // Compoe as linhas que o LZW entregou (a ultima pode vir pela metade).
  if (feitos) {
    int n, linhas = (int)((feitos + (size_t)larg - 1) / (size_t)larg);
    for (n = 0; n < linhas; n++) {
      int y = q->topo + (entrel ? linhaEntrelacada(n, alt) : n), x, xmax = larg;
      const unsigned char *s = d->idx + (size_t)n * larg;
      uint32_t *o;
      if (y < y0 || y >= y1) continue;
      o = d->tela + (size_t)y * d->telaW;
      if ((size_t)(n + 1) * larg > feitos) xmax = (int)(feitos - (size_t)n * larg);
      if (q->esq + xmax > x1) xmax = x1 - q->esq;
      for (x = 0; x < xmax; x++) {
        int c = s[x];
        if (c == transp || c >= palN) continue;
        o[q->esq + x] = pal[c];
      }
    }
  }

  reduzir(d, 0, 0, d->saidaW, d->saidaH);
  return 0;
}
