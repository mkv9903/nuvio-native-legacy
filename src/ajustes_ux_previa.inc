#include <math.h>
// Texto e previas geradas do inspetor de Ajustes.

// Estilo de texto mais proximo de `px` entre os que a miniatura usa. 0 = nao
// desenha (abaixo de 6 px o texto e so ruido).
static TxtEstilo ajEstiloPx(float px, int forte) {
  if (px < 6.0f) return (TxtEstilo)-1;
  if (px < 11.0f) return forte ? TXT_AJ_MINI9B : TXT_AJ_MINI9;
  if (px < 13.5f) return forte ? TXT_AJ_MINI12 : TXT_AJ_MINI13;
  if (px < 15.5f) return forte ? TXT_MINI : TXT_ILHA_APOIO;
  if (px < 18.0f) return forte ? TXT_AJ_CHIP : TXT_AJ_ESTADO;
  if (px < 21.0f) return forte ? TXT_AJ_SEG : TXT_AJ_SUB;
  if (px < 30.0f) return forte ? TXT_AJ_NOME : TXT_ILHA_CORPO;
  if (px < 50.0f) return TXT_ILHA_TITULO;
  if (px < 61.0f) return TXT_AJ_NUM58;
  if (px < 90.0f) return TXT_AJ_NUM64;
  return TXT_AJ_NUM110;
}
static void ajTextoMini(const char *s, float px, int forte, float x, float y, int r, int g, int b, float a) {
  TxtEstilo e = ajEstiloPx(px, forte);
  TxtLinha l;
  if ((int)e < 0) return;
  l = txt_linha(e, s, r, g, b, 255);
  txt_desenhar_alpha(l, x, y + (px * 1.2f - l.h) * 0.5f, a);
}

// As resolucoes que o teto de "Qualidade maxima" deixa passar.
static float ajBarrasQualidade(float x, float y, float w) {
  static const FormatoMarca F[3] = { FMT_720, FMT_1080, FMT_4K };
  static const float H[3] = { 120, 190, 260 };
  int sel = valor[AJ_QUALIDADE], i;
  float bw = (w - 12 - 36) / 3.0f, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < 3; i++) {
    int passa = sel == 0 || sel == 1 || (sel == 2 && i <= 1) || (sel == 3 && i == 0);
    float bx = x + 6 + i * (bw + 18), bh = H[i], mw = marca_formato_largura(F[i], 30);
    GfxRect r = { bx, y + 300 - 44 - bh, bw, bh };
    if (passa) { gfx_cor(r, 16.0f / bh, ar, ag, ab, 0.30f); gfx_recorte(r.x, r.y, r.w, 5); gfx_cor(r, 16.0f / bh, ar, ag, ab, 1); gfx_sem_recorte(); }
    else gfx_cor(r, 16.0f / bh, 1, 1, 1, 0.06f);
    marca_formato(F[i], bx + (bw - mw) * 0.5f, y + 300 - 30, 30, 1, 1, 1, passa ? 0.85f : 0.3f);
  }
  return 300;
}
// A previa da opcao usa a mesma ilustracao gerada do resumo da categoria.
static float ajPrevia(int op, float x, float y, float w) {
  return ajCenaGerada(uxSecaoDe(op), op, x, y, w);
}

// Previa pequena (diferencas: padrao e atual lado a lado).
static float ajPreviaCompacta(int op, float x, float y, float w) {
  return ajPrevia(op, x, y, w);
}
// Previa da coluna direita dos editores modais.
static float ajPreviaEditor(int op, float x, float y, float w) {
  if (op == AJ_QUALIDADE) {
    float h;
    ajKicker("Resoluções que passam", x, y, 1);
    h = ajBarrasQualidade(x, y + 36, w);
    return 36 + h + 16 + ajBloco(TXT_ILHA_APOIO, ajudaOpcao(op), x, y + 36 + h + 16, w, 21, 0.45f, 4);
  }
  return ajPrevia(op, x, y, w);
}

// O melhor resultado da busca: a arte do titulo em cena com o valor atual da
// opcao numa tarja, como a legenda aparece no player.
void ajustes_previa_busca(int op, float x, float y, float w, float h, float a) {
  char v[160];
  TxtLinha l;
  if (op < 0 || op >= AJ_N) return;
  // 2.0.2: a previa gerada da opcao (sem foto), no meio do quadro pedido.
  { float g = gfx_opacidade_grupo, cw = h * 16.0f / 9.0f;
    if (cw > w) cw = w;
    gfx_opacidade_grupo = g * a;
    ajCenaGerada(uxSecaoDe(op), op, x + (w - cw) * 0.5f, y + (h - cw * 9.0f / 16.0f) * 0.5f, cw);
    gfx_opacidade_grupo = g; }
  uxValorTexto(op, uxValorAtual(op), v, sizeof v);
  if (!v[0]) return;
  l = txt_linha_corta(TXT_MINI, v, 255, 255, 255, 255, w - 40);
  gfx_cor((GfxRect){ x + (w - l.w) * 0.5f - 10, y + h - 14 - l.h - 8, l.w + 20, l.h + 8 }, 6.0f / (l.h + 8), 0, 0, 0, 0.72f * a);
  txt_desenhar_alpha(l, x + (w - l.w) * 0.5f, y + h - 14 - l.h - 4, a);
}
