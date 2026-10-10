// Still-image GIF decoding: real LZW fixtures, independent pixel expectations,
// palettes, transparency, interlacing, scaling and malformed input.
#include "../src/gif.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static size_t nbuf;
static unsigned char *gBuf;
static size_t gCap;
static void b1(unsigned v) { assert(nbuf < gCap); gBuf[nbuf++] = (unsigned char)v; }
static void b2(unsigned v) { b1(v & 255); b1((v >> 8) & 255); }
static void bn(const char *s, size_t n) { for (size_t i=0; i<n; i++) b1((unsigned char)s[i]); }
static void extensaoNetscape(void) {
  b1(0x21); b1(0xFF); b1(11); bn("NETSCAPE2.0", 11);
  b1(3); b1(1); b2(0); b1(0);
}
static uint16_t filho[4096][256];   // codigo de (prefixo, simbolo), 0 = nao ha

static unsigned char lzwSaida[1 << 20];
static size_t lzwN;
static uint32_t lzwBits;
static int lzwNb;
static void lzwEmitir(int cod, int larg) {
  lzwBits |= (uint32_t)cod << lzwNb;
  lzwNb += larg;
  while (lzwNb >= 8) { assert(lzwN < sizeof lzwSaida); lzwSaida[lzwN++] = (unsigned char)(lzwBits & 0xFF); lzwBits >>= 8; lzwNb -= 8; }
}
// Codifica `n` indices e escreve no buf: tamanho minimo + sub-blocos.
static void lzwBloco(const unsigned char *idx, size_t n, int minimo) {
  int clear = 1 << minimo, eoi = clear + 1, prox = eoi + 1, larg = minimo + 1, atual;
  size_t i, p;
  lzwN = 0; lzwBits = 0; lzwNb = 0;
  memset(filho, 0, sizeof filho);
  lzwEmitir(clear, larg);
  atual = idx[0];
  for (i = 1; i < n; i++) {
    int s = idx[i];
    if (filho[atual][s]) { atual = filho[atual][s]; continue; }
    lzwEmitir(atual, larg);
    if (prox < 4096) {
      filho[atual][s] = (uint16_t)prox++;
      if (prox > (1 << larg) && larg < 12) larg++;
    } else {
      lzwEmitir(clear, larg);
      memset(filho, 0, sizeof filho);
      prox = eoi + 1; larg = minimo + 1;
    }
    atual = s;
  }
  lzwEmitir(atual, larg);
  // O decodificador cria uma entrada ao ler o ultimo codigo; se ela chega a
  // 2^larg, ele ja espera o END um bit mais largo.
  if (prox < 4096 && prox + 1 > (1 << larg) && larg < 12) larg++;
  lzwEmitir(eoi, larg);
  if (lzwNb) { lzwSaida[lzwN++] = (unsigned char)(lzwBits & 0xFF); lzwBits = 0; lzwNb = 0; }
  b1((unsigned)minimo);
  for (p = 0; p < lzwN; p += 255) {
    size_t k = lzwN - p < 255 ? lzwN - p : 255;
    b1((unsigned)k);
    for (i = 0; i < k; i++) b1(lzwSaida[p + i]);
  }
  b1(0);
}

static unsigned char grande[1 << 21];   // buffer dos GIFs com pixel
// O montador de cima escreve em `buf` (8 KB); os GIFs daqui sao maiores.
#define USAR_GRANDE() do { gBuf = grande; gCap = sizeof grande; } while (0)

typedef struct { int esq, topo, larg, alt, descarte, transp, entrel, palLocal; } Q;
#define MAXW 320
#define MAXH 200
static unsigned char idxDe[8][MAXW * MAXH];      // indices por quadro (ordem de linha real)
static uint32_t palG[256], palL[256];

static uint32_t rgba(int r, int g, int b) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u; }

// Monta um GIF de `nq` quadros em `grande`. Devolve o tamanho.
static size_t montarGif(int W, int H, int bitsPal, const Q *q, int nq) {
  int k, i, nPal = 1 << bitsPal;
  nbuf = 0;
  bn("GIF89a", 6); b2((unsigned)W); b2((unsigned)H);
  b1(0x80 | (unsigned)(bitsPal - 1)); b1(0); b1(0);
  for (i = 0; i < nPal; i++) { b1(palG[i] & 0xFF); b1((palG[i] >> 8) & 0xFF); b1((palG[i] >> 16) & 0xFF); }
  extensaoNetscape();
  for (k = 0; k < nq; k++) {
    unsigned char seq[MAXW * MAXH];
    int n = 0, y, x, linhas = q[k].alt;
    b1(0x21); b1(0xF9); b1(4);
    b1((unsigned)((q[k].descarte << 2) | (q[k].transp >= 0 ? 1 : 0)));
    b2(4); b1(q[k].transp >= 0 ? (unsigned)q[k].transp : 0); b1(0);
    b1(0x2C); b2((unsigned)q[k].esq); b2((unsigned)q[k].topo); b2((unsigned)q[k].larg); b2((unsigned)q[k].alt);
    b1((q[k].palLocal ? 0x80 | (unsigned)(bitsPal - 1) : 0) | (q[k].entrel ? 0x40 : 0));
    if (q[k].palLocal)
      for (i = 0; i < nPal; i++) { b1(palL[i] & 0xFF); b1((palL[i] >> 8) & 0xFF); b1((palL[i] >> 16) & 0xFF); }
    // Entrelacado: as linhas vao ao arquivo na ordem das quatro passadas.
    { static const int ini[4] = { 0, 4, 2, 1 }, passo[4] = { 8, 8, 4, 2 };
      int pas;
      if (q[k].entrel) {
        for (pas = 0; pas < 4; pas++)
          for (y = ini[pas]; y < linhas; y += passo[pas])
            for (x = 0; x < q[k].larg; x++) seq[n++] = idxDe[k][y * q[k].larg + x];
      } else {
        for (y = 0; y < linhas; y++)
          for (x = 0; x < q[k].larg; x++) seq[n++] = idxDe[k][y * q[k].larg + x];
      } }
    lzwBloco(seq, (size_t)n, bitsPal < 2 ? 2 : bitsPal);
  }
  b1(0x3B);
  return nbuf;
}


static uint32_t expected[MAXW * MAXH];
static void firstFrame(int W, int H, const Q *q) {
  memset(expected, 0, (size_t)W * H * 4);
  for (int y=0; y<q->alt; y++) for (int x=0; x<q->larg; x++) {
    int c=idxDe[0][y*q->larg+x], X=q->esq+x, Y=q->topo+y;
    if (X<W && Y<H && c!=q->transp)
      expected[Y*W+X]=(q->palLocal ? palL : palG)[c];
  }
}
static void verify(int W, int H, int bits, Q q, int width, int twoFrames) {
  Q frames[2]={q, {0,0,W,H,1,-1,0,0}};
  int w,h,ow,oh;
  memset(idxDe[1], 1, (size_t)W*H);
  size_t n=montarGif(W,H,bits,frames,twoFrames ? 2 : 1);
  unsigned char *out=gif_primeiro_rgba(grande,n,width,&w,&h,&ow,&oh);
  int sw=width<=0 || width>W ? W : width;
  int sh=(int)((double)H*sw/W+.5);
  if (sh<1) sh=1;
  assert(out && w==sw && h==sh && ow==W && oh==H);
  firstFrame(W,H,&q);
  for (int y=0;y<h;y++) for(int x=0;x<w;x++) {
    int x0=x*W/w,x1=(x+1)*W/w,y0=y*H/h,y1=(y+1)*H/h;
    unsigned r=0,g=0,b=0,count=0,area=(x1-x0)*(y1-y0);
    for(int Y=y0;Y<y1;Y++) for(int X=x0;X<x1;X++) {
      uint32_t c=expected[Y*W+X];
      if(c>>24){r+=c&255;g+=(c>>8)&255;b+=(c>>16)&255;count++;}
    }
    uint32_t pixel=count ? r/count | ((g/count)<<8) | ((b/count)<<16) |
      (((count==area ? 255 : count*255/area))<<24) : 0;
    assert(!memcmp(out+((size_t)y*w+x)*4,&pixel,4));
  }
  free(out);
}
int main(void) {
  USAR_GRANDE();
  for(int i=0;i<256;i++){
    palG[i]=rgba(i,255-i,(i*7)&255);
    palL[i]=rgba((i*3)&255,i,128);
  }
  unsigned random=12345;
  for(int i=0;i<MAXW*MAXH;i++){
    random=random*1103515245u+12345u;
    idxDe[0][i]=(unsigned char)(random>>16);
  }
  verify(MAXW,MAXH,8,(Q){0,0,MAXW,MAXH,1,-1,0,0},0,0);
  verify(MAXW,MAXH,8,(Q){0,0,MAXW,MAXH,1,-1,0,0},80,1);
  memset(idxDe[0],3,sizeof idxDe[0]);
  verify(64,40,4,(Q){0,0,64,40,1,-1,0,0},0,0);
  for(int i=0;i<64*40;i++)idxDe[0][i]=(unsigned char)(i%16);
  verify(64,40,4,(Q){0,0,64,40,3,5,1,1},16,1);
  verify(64,40,4,(Q){50,30,30,20,2,0,0,0},16,0);
  verify(64,40,4,(Q){100,100,20,20,1,-1,0,0},1000,0);
  Q small={0,0,8,8,1,-1,1,1};
  size_t n=montarGif(8,8,4,&small,1);
  for(size_t cut=0;cut<n;cut++){
    int w,h,ow,oh;
    unsigned char *out=gif_primeiro_rgba(grande,cut,0,&w,&h,&ow,&oh);
    free(out);
  }
  int w=7,h=7,ow=7,oh=7;
  assert(!gif_primeiro_rgba(NULL,0,0,&w,&h,&ow,&oh));
  assert(!w && !h && !ow && !oh);
  unsigned char saved[4];
  memcpy(saved,grande+6,4);memset(grande+6,255,4);
  assert(!gif_primeiro_rgba(grande,n,16,&w,&h,&ow,&oh));
  memcpy(grande+6,saved,4);
  assert(!gif_primeiro_rgba(grande,32u*1024u*1024u+1,0,NULL,NULL,NULL,NULL));
  unsigned char original[4096];
  assert(n<sizeof original);memcpy(original,grande,n);
  for(size_t i=13;i<n;i++){
    memcpy(grande,original,n);grande[i]^=255;
    unsigned char *out=gif_primeiro_rgba(grande,n,4,NULL,NULL,NULL,NULL);
    free(out);
  }
  puts("GIF still images: LZW, palettes, transparency, interlace, scaling and malformed input passed");
  return 0;
}
