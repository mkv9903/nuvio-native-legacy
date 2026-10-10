#include "webp.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdlcompat.h"

// Dois decodificadores atras da mesma porta. Ambos entregam um bloco RGBA
// recem-alocado (bytes R,G,B,A na memoria) e dizem o tamanho; quem monta a
// SDL_Surface e webp_carregar la embaixo, uma vez so.
static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh);
static void     soltar(uint8_t *px);

// -------------------------------------------------------- webOS e Mac ------
#include <dlfcn.h>
#include <pthread.h>

typedef int      (*FnInfo)(const uint8_t *, size_t, int *, int *);
typedef uint8_t *(*FnRgba)(const uint8_t *, size_t, int *, int *);
typedef void     (*FnFree)(void *);

// A API AVANCADA da libwebp (decode.h), so o que a escala precisa. As structs
// sao copiadas do cabecalho porque o alvo LG nao tem os headers no toolchain —
// e a lib entra por dlopen justamente por isso. O layout e estavel desde o ABI
// 0x0200 (os `pad[]` existem para isso), e WebPInitDecoderConfigInternal so
// recusa quando o byte MAIOR da versao difere: 0x0209 casa com a libwebp 0.5
// da TV e com a 1.x do Mac. Se recusar, o caminho antigo (tamanho cheio +
// tex_reduzir) continua valendo — nada quebra, so gasta mais.
//
// POR QUE ESCALAR DENTRO DO DECODER (19/09/2026): os fundos do Xperience
// passaram a vir em 3840x2160 (~180 KB cada, 17 estilos por marca). Em
// WebPDecodeRGBA um deles e 33 MB de RGBA em tamanho cheio ANTES da reducao
// para os 1920 do heroi — na C9 de 2 GB e um pico por pasta focada, e na
// Samsung e um oitavo do heap fixo. Com use_scaling a libwebp reduz linha a
// linha e o bloco que nasce ja tem o tamanho pedido: 8 MB para 1920.
typedef struct { int width, height, has_alpha, has_animation, format; uint32_t pad[5]; } WpFeatures;
typedef struct {
  int colorspace;                // 1 = MODE_RGBA
  int width, height;
  int is_external_memory;
  union {
    struct { uint8_t *rgba; int stride; size_t size; } RGBA;
    struct { uint8_t *y, *u, *v, *a; int y_stride, u_stride, v_stride, a_stride;
             size_t y_size, u_size, v_size, a_size; } YUVA;
  } u;
  uint32_t pad[4];
  uint8_t *private_memory;
} WpBuffer;
typedef struct {
  int bypass_filtering, no_fancy_upsampling, use_cropping;
  int crop_left, crop_top, crop_width, crop_height;
  int use_scaling, scaled_width, scaled_height;
  int use_threads, dithering_strength, flip, alpha_dithering_strength;
  uint32_t pad[5];
} WpOptions;
typedef struct { WpFeatures input; WpBuffer output; WpOptions options; } WpConfig;
#define WP_ABI 0x0209
#define WP_MODE_RGBA 1
typedef int  (*FnCfgInit)(WpConfig *, int);
typedef int  (*FnDecode)(const uint8_t *, size_t, WpConfig *);
typedef void (*FnFreeBuf)(WpBuffer *);

static FnInfo pInfo; static FnRgba pRgba; static FnFree pFree;
static FnCfgInit pCfgInit; static FnDecode pDecode; static FnFreeBuf pFreeBuf;
static pthread_once_t abrirUmaVez = PTHREAD_ONCE_INIT;
static int escalaAvisada;

static void abrir(void) {
  static const char *nomes[] = {
    "libwebp.so.7", "libwebp.so", NULL };
  void *h = NULL; int i;
  for (i = 0; nomes[i] && !h; i++) h = dlopen(nomes[i], RTLD_NOW);
  if (!h) { printf("[webp] libwebp ausente; .webp nao vai decodificar\n"); return; }
  pInfo = (FnInfo)dlsym(h, "WebPGetInfo");
  pRgba = (FnRgba)dlsym(h, "WebPDecodeRGBA");
  pFree = (FnFree)dlsym(h, "WebPFree");   // ausente em libwebp antiga: free() serve
  if (!pInfo || !pRgba) { printf("[webp] libwebp sem WebPDecodeRGBA\n"); pInfo = NULL; pRgba = NULL; }
  pCfgInit = (FnCfgInit)dlsym(h, "WebPInitDecoderConfigInternal");
  pDecode  = (FnDecode)dlsym(h, "WebPDecode");
  pFreeBuf = (FnFreeBuf)dlsym(h, "WebPFreeDecBuffer");
  if (!pCfgInit || !pDecode || !pFreeBuf) {
    printf("[webp] libwebp sem API avancada: WebP grande decodifica em tamanho cheio\n");
    pCfgInit = NULL;
  }
}

// Decodifica ja reduzido a `lw` de largura. NULL quando a API avancada nao
// existe ou recusou (versao, memoria): o chamador cai no tamanho cheio.
static uint8_t *decodificarEscalado(const unsigned char *dados, size_t n, int w, int h, int lw, int *lh) {
  WpConfig cfg;
  uint8_t *px;
  size_t linha = (size_t)lw * 4, y;
  int alto = (int)(((long)h * lw + w / 2) / w);
  if (alto < 1) alto = 1;
  if (!pCfgInit) return NULL;
  memset(&cfg, 0, sizeof cfg);
  if (!pCfgInit(&cfg, WP_ABI)) {
    if (__sync_bool_compare_and_swap(&escalaAvisada, 0, 1))
      printf("[webp] libwebp recusou o ABI %#x: sem escala no decoder\n", WP_ABI);
    return NULL;
  }
  cfg.options.use_scaling   = 1;
  cfg.options.scaled_width  = lw;
  cfg.options.scaled_height = alto;
  cfg.output.colorspace     = WP_MODE_RGBA;
  if (pDecode(dados, n, &cfg) != 0 || !cfg.output.u.RGBA.rgba) { pFreeBuf(&cfg.output); return NULL; }
  // Copia para um bloco proprio, contiguo, que `soltar` libera com free():
  // o da libwebp tem stride proprio e so WebPFreeDecBuffer sabe solta-lo.
  px = malloc(linha * (size_t)cfg.output.height);
  if (px)
    for (y = 0; y < (size_t)cfg.output.height; y++)
      memcpy(px + y * linha, cfg.output.u.RGBA.rgba + y * (size_t)cfg.output.u.RGBA.stride, linha);
  *lh = cfg.output.height;
  pFreeBuf(&cfg.output);
  return px;
}

// WEBP ANIMADO: SO O PRIMEIRO QUADRO. Avatar de perfil pode vir animado
// (VP8X com ANIM/ANMF), e a libwebp simples — a unica que a LG tem, sem a
// libwebpdemux — responde WebPGetInfo com o tamanho do canvas e depois
// RECUSA o WebPDecodeRGBA (VP8_STATUS_UNSUPPORTED_FEATURE). No log de campo
// (LG 1.4.1-1.4.3) era o "[tex] decode falhou ... magica=52494646" de avatar
// pequeno: RIFF certo, tamanho certo, e cartao vazio no lugar da foto.
//
// O quadro 1 e recortado do ANMF e embrulhado num WebP parado (VP8/VP8L
// direto, ou VP8X+ALPH+VP8 quando o quadro tem alfa separado), que a libwebp
// le. Como o quadro pode ser menor que o canvas (o img2webp corta a borda
// transparente ja no primeiro), ele e colado na posicao dele num canvas
// transparente do tamanho declarado. O navegador do Tizen ja faz isto sozinho
// (createImageBitmap pega o primeiro quadro).
static unsigned le24(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16); }
static unsigned le32(const unsigned char *p) { return le24(p) | ((unsigned)p[3] << 24); }
static void pe32(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void pe24(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; }

static uint8_t *primeiroQuadro(const unsigned char *d, size_t n, int *lw, int *lh, int *ow, int *oh) {
  size_t pos = 30, fim;
  unsigned cw, ch;
  if (n < 30 || memcmp(d + 12, "VP8X", 4) || !(d[20] & 0x02)) return NULL;
  cw = le24(d + 24) + 1; ch = le24(d + 27) + 1;
  if ((size_t)cw * ch > 4096u * 4096u) return NULL;
  fim = 8 + (size_t)le32(d + 4);
  if (fim > n) fim = n;   // RIFF que declara mais do que tem: le o que veio
  while (pos + 8 <= fim) {
    size_t tam = le32(d + pos + 4), dado = pos + 8;
    if (tam > fim - dado) return NULL;
    if (!memcmp(d + pos, "ANMF", 4) && tam >= 16 + 8) {
      const unsigned char *q = d + dado;
      unsigned fx = le24(q) * 2, fy = le24(q + 3) * 2;
      unsigned fw = le24(q + 6) + 1, fh = le24(q + 9) + 1;
      const unsigned char *sub = q + 16;
      size_t nsub = tam - 16, i = 0, a = 0, nAlph = 0, img = 0, nImg = 0;
      int ehVp8l = 0, w = 0, h = 0, y;
      unsigned char *parado; size_t np;
      uint8_t *fr, *tela;
      // Subchunks do quadro: ALPH opcional, depois VP8 ou VP8L. Os tamanhos
      // guardados (nAlph, nImg) sao os do campo, sem o byte de preenchimento;
      // o embrulho repoe o preenchimento com zero.
      while (i + 8 <= nsub) {
        size_t t = le32(sub + i + 4);
        if (t > nsub - i - 8) return NULL;
        if (!memcmp(sub + i, "ALPH", 4)) { a = i; nAlph = t; }
        else if (!memcmp(sub + i, "VP8 ", 4) || !memcmp(sub + i, "VP8L", 4)) {
          img = i; nImg = t; ehVp8l = sub[i + 3] == 'L';
          break;
        }
        i += 8 + t + (t & 1);
      }
      if (!nImg) return NULL;
      if (ehVp8l) nAlph = 0;   // VP8L traz o alfa no proprio fluxo
      { size_t pA = nAlph ? 8 + nAlph + (nAlph & 1) : 0, pI = 8 + nImg + (nImg & 1);
        np = 12 + (nAlph ? 18 + pA : 0) + pI;
        parado = calloc(1, np);
        if (!parado) return NULL;
        memcpy(parado, "RIFF", 4); pe32(parado + 4, (unsigned)(np - 8)); memcpy(parado + 8, "WEBP", 4);
        if (nAlph) {
          memcpy(parado + 12, "VP8X", 4); pe32(parado + 16, 10);
          parado[20] = 0x10;   // so o bit de alfa
          pe24(parado + 24, fw - 1); pe24(parado + 27, fh - 1);
          memcpy(parado + 30, sub + a, 8 + nAlph);
          memcpy(parado + 30 + pA, sub + img, 8 + nImg);
        } else memcpy(parado + 12, sub + img, 8 + nImg); }
      fr = pRgba(parado, np, &w, &h);
      free(parado);
      if (!fr) return NULL;
      if (w == (int)cw && h == (int)ch) tela = fr;
      else {
        tela = calloc((size_t)cw * ch, 4);
        if (tela)
          for (y = 0; y < h && fy + (unsigned)y < ch; y++) {
            int cabe = fx >= cw ? 0 : ((unsigned)w > cw - fx ? (int)(cw - fx) : w);
            if (cabe > 0) memcpy(tela + ((size_t)(fy + y) * cw + fx) * 4, fr + (size_t)y * w * 4, (size_t)cabe * 4);
          }
        soltar(fr);
        if (!tela) return NULL;
      }
      *lw = (int)cw; *lh = (int)ch;
      if (ow) *ow = (int)cw;
      if (oh) *oh = (int)ch;
      return tela;
    }
    pos = dado + tam + (tam & 1);
  }
  return NULL;
}

static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh) {
  int w = 0, h = 0; uint8_t *px;
  pthread_once(&abrirUmaVez, abrir);
  if (pRgba && n >= 30 && !memcmp(dados + 12, "VP8X", 4) && (dados[20] & 0x02))
    return primeiroQuadro(dados, n, lw, lh, ow, oh);
  if (!pRgba || !pInfo(dados, n, &w, &h) || w < 1 || h < 1) return NULL;
  if (ow) *ow = w;
  if (oh) *oh = h;
  if (largMax > 0 && w > largMax) {
    px = decodificarEscalado(dados, n, w, h, largMax, lh);
    if (px) { *lw = largMax; return px; }
  }
  px = pRgba(dados, n, &w, &h);
  if (!px) return NULL;
  *lw = w; *lh = h;
  return px;
}

// O bloco da API avancada ja foi copiado para malloc; o do WebPDecodeRGBA e
// da lib. Os dois passam por aqui: WebPFree e free() sao o mesmo alocador na
// libwebp, e e por isso que a copia acima pode usar malloc.
static void soltar(uint8_t *px) { if (pFree) pFree(px); else free(px); }

SDL_Surface *webp_carregar_larg(const char *caminho, int largMax, int *ow, int *oh) {
  FILE *f; long n; unsigned char *dados; SDL_Surface *s;
  if (ow) *ow = 0;
  if (oh) *oh = 0;
  if (!caminho) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  if (n < 16 || n > 32L * 1024 * 1024) { fclose(f); return NULL; }
  dados = malloc((size_t)n);
  if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) { free(dados); fclose(f); return NULL; }
  fclose(f);
  s = webp_carregar_larg_mem(dados, (size_t)n, largMax, ow, oh);
  free(dados);
  return s;
}

/* Os bytes do download direto ao decode no LG (ver jpeg_rapido_carregar_mem). */
SDL_Surface *webp_carregar_larg_mem(const unsigned char *dados, size_t n, int largMax,
                                    int *ow, int *oh) {
  int w = 0, h = 0; uint8_t *px; SDL_Surface *s;
  if (ow) *ow = 0;
  if (oh) *oh = 0;
  if (!dados || n < 16 || n > 32L * 1024 * 1024) return NULL;
  if (memcmp(dados, "RIFF", 4) || memcmp(dados + 8, "WEBP", 4)) return NULL;
  px = decodificar(dados, n, largMax, &w, &h, ow, oh);
  if (!px) return NULL;
  // ABGR8888 no SDL = bytes R,G,B,A na memoria em little-endian, que e o que
  // os dois decodificadores entregam. Copia para uma superficie propria: a do
  // SDL_..From apontaria para memoria de fora.
  s = nv_superficie(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s) {
    int y;
    for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4);
  }
  soltar(px);
  return s;
}

SDL_Surface *webp_carregar(const char *caminho) {
  return webp_carregar_larg(caminho, 0, NULL, NULL);
}
