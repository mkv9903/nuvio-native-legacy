// Ver apoio.h.
#include "apoio.h"
#include "qr.h"
#include "gl_compat.h"
#include "idioma.h"
#include "tex_cache.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const URL[APOIO_N] = { NV_URL_PATREON, NV_URL_KOFI };
static const char *const NOME[APOIO_N] = { "Patreon", "Ko-fi" };

static GLuint tex[APOIO_N];
static int    texLado[APOIO_N];     // modulos + zona de silencio
static int    tentou[APOIO_N];
// O QR OFICIAL DO KO-FI (o do dono, com a xicara no meio): aponta para o
// endereco por ID da pagina (ko-fi.com/K0S82835VO), nao para NV_URL_KOFI.
// 410 px = 41 modulos de 10 px, zona de silencio inclusa. Sem a imagem (ou
// antes de ela carregar) vale o gerado a partir de NV_URL_KOFI.
static char selo[600] = "deploy/app/art/marcas/kofi-badge.png";
static char qrKofi[600] = "deploy/app/art/marcas/kofi-qr.png";

int apoio_n(void) {
  int i, n = 0;
  for (i = 0; i < APOIO_N; i++) if (URL[i][0]) n++;
  return n;
}

int apoio_qual(int i) {
  int q;
  for (q = 0; q < APOIO_N; q++)
    if (URL[q][0] && i-- == 0) return q;
  return -1;
}

const char *apoio_nome(int q) { return q >= 0 && q < APOIO_N ? NOME[q] : ""; }
const char *apoio_url(int q) { return q >= 0 && q < APOIO_N ? URL[q] : ""; }

const char *apoio_url_curta(int q) {
  const char *s = apoio_url(q), *p = strstr(s, "://");
  if (p) s = p + 3;
  if (!strncmp(s, "www.", 4)) s += 4;
  return s;
}

// Mesmo cuidado de login.c: textura RGB com 4 modulos de silencio, NEAREST.
static void gerar(int q) {
  Qr c;
  int lado, x, y;
  unsigned char *px;
  tentou[q] = 1;
  if (!URL[q][0] || !qr_gerar(&c, URL[q])) {
    printf("[apoio] sem QR para %s\n", NOME[q]);
    return;
  }
  lado = c.lado + 8;
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return;
  memset(px, 255, (size_t)lado * lado * 3);
  for (y = 0; y < c.lado; y++)
    for (x = 0; x < c.lado; x++)
      if (qr_modulo(&c, x, y)) {
        size_t i = ((size_t)(y + 4) * lado + (x + 4)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }
  glGenTextures(1, &tex[q]);
  glBindTexture(GL_TEXTURE_2D, tex[q]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  free(px);
  texLado[q] = lado;
}

int apoio_qr(int q, float x, float y, float lado, float a) {
  float s;
  if (q < 0 || q >= APOIO_N || !URL[q][0]) return 0;
  if (q == APOIO_KOFI && a > 0.004f) {
    GLuint t = tex_obter(qrKofi);
    if (t) {
      // Cantos claros do cartao + o simbolo oficial com folga, sem modulo
      // menor que ~5 px no menor uso (a previa de Ajustes).
      s = floorf(lado * 0.93f);
      gfx_cor((GfxRect){ x, y, lado, lado }, 0.09f, 1, 1, 1, a);
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ floorf(x + (lado - s) * 0.5f), floorf(y + (lado - s) * 0.5f), s, s },
               t, GFX_SNAP, 0, 0, 0, 0, 0, 0, 0, a);
      return 1;
    }
  }
  if (!tentou[q]) gerar(q);
  if (!tex[q] || a <= 0.004f) return tex[q] != 0;
  // A textura inclui os 4 modulos brancos de cada lado; o cartao claro em
  // volta da folga para o simbolo nao encostar nos cantos arredondados.
  // Mesmo lado do QR oficial do Ko-fi (93% do cartao), para os dois
  // cartoes lerem iguais. Fora de multiplo inteiro os modulos variam 1 px
  // (NEAREST), o que a camera tolera; conferido decodificando as capturas.
  s = floorf(lado * 0.93f);
  gfx_cor((GfxRect){ x, y, lado, lado }, 0.09f, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ floorf(x + (lado - s) * 0.5f), floorf(y + (lado - s) * 0.5f), s, s },
           tex[q], GFX_SNAP, 0, 0, 0, 0, 0, 0, 0, a);
  return 1;
}

void apoio_dir(const char *d) {
  if (!d || !d[0]) return;
  snprintf(selo, sizeof selo, "%s/marcas/kofi-badge.png", d);
  snprintf(qrKofi, sizeof qrKofi, "%s/marcas/kofi-qr.png", d);
}

// O selo do Ko-fi tem 672x356 no original (1,89:1) e cantos de ~10% da
// altura; a placa do Patreon copia altura, cantos e as duas linhas (pequena
// em cima, nome grande embaixo) para os dois lerem como par.
float apoio_rotulo(int q, float x, float y, float h, int centro, float a) {
  if (q == APOIO_KOFI) {
    GLuint t = tex_obter(selo);
    float ap = tex_aspecto(selo), w;
    if (ap <= 0.0f) ap = 672.0f / 356.0f;
    w = h * ap;
    if (centro) x -= w * 0.5f;
    if (t && a > 0.004f) {
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ x, y, w, h }, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
    }
    return w;
  }
  { TxtLinha p = txt_linha(TXT_V2_18, i18n("Apoie no"), 30, 31, 36, 255);
    TxtLinha n = txt_linha(TXT_W20_24B, NOME[q], 20, 21, 26, 255);
    float w = (float)(p.w > n.w ? p.w : n.w) + 2.0f * 0.42f * h, th = (float)p.h + (float)n.h - 2.0f;
    if (centro) x -= w * 0.5f;
    gfx_cor((GfxRect){ x, y, w, h }, 0.11f, 0.957f, 0.961f, 0.980f, a);
    txt_desenhar_alpha(p, x + (w - (float)p.w) * 0.5f, y + (h - th) * 0.5f, 0.8f * a);
    txt_desenhar_alpha(n, x + (w - (float)n.w) * 0.5f, y + (h - th) * 0.5f + (float)p.h - 2.0f, a);
    return w;
  }
}
