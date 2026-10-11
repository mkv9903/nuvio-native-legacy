#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NV_RUST_CORE
#include "rust_core.h"

/* Shared storage layout: Rust emits into a native buffer owned by this Jsw. */
_Static_assert(JSW_PROF == 16, "writer depth limit");
_Static_assert(sizeof(Jsw) == (sizeof(void *) == 8 ? 48 : 36), "writer size");
_Static_assert(_Alignof(Jsw) == _Alignof(void *), "writer alignment");
_Static_assert(offsetof(Jsw, p) == 0, "writer buffer offset");
_Static_assert(offsetof(Jsw, n) == sizeof(void *), "writer length offset");
_Static_assert(offsetof(Jsw, cap) == sizeof(void *) * 2, "writer capacity offset");
_Static_assert(offsetof(Jsw, erro) == sizeof(void *) * 3, "writer error offset");
_Static_assert(offsetof(Jsw, prof) == sizeof(void *) * 3 + 4, "writer depth offset");
_Static_assert(offsetof(Jsw, primeiro) == sizeof(void *) * 3 + 8, "writer flags offset");

void jsw_iniciar(Jsw *writer) { nuvio_rust_json_writer_init(writer); }
void jsw_livre(Jsw *writer) { nuvio_rust_json_writer_free(writer); }
const char *jsw_texto_final(const Jsw *writer) { return nuvio_rust_json_writer_finish(writer); }
void jsw_obj_ini(Jsw *writer) { nuvio_rust_json_writer_open(writer, '{'); }
void jsw_obj_fim(Jsw *writer) { nuvio_rust_json_writer_close(writer, '}'); }
void jsw_arr_ini(Jsw *writer) { nuvio_rust_json_writer_open(writer, '['); }
void jsw_arr_fim(Jsw *writer) { nuvio_rust_json_writer_close(writer, ']'); }
void jsw_chave(Jsw *writer, const char *key) { nuvio_rust_json_writer_key(writer, key); }
void jsw_str(Jsw *writer, const char *input) { nuvio_rust_json_writer_string(writer, input); }
void jsw_int(Jsw *writer, long long value) { nuvio_rust_json_writer_integer(writer, value); }
void jsw_bool(Jsw *writer, int value) { nuvio_rust_json_writer_boolean(writer, value); }
void jsw_nulo(Jsw *writer) { nuvio_rust_json_writer_null(writer); }
void jsw_bruto(Jsw *writer, const char *input) { nuvio_rust_json_writer_raw(writer, input); }
void jsw_cs(Jsw *writer, const char *key, const char *value) {
  jsw_chave(writer, key); jsw_str(writer, value);
}
void jsw_ci(Jsw *writer, const char *key, long long value) {
  jsw_chave(writer, key); jsw_int(writer, value);
}
void jsw_cb(Jsw *writer, const char *key, int value) {
  jsw_chave(writer, key); jsw_bool(writer, value);
}

#else

static void por(Jsw *w, const char *s, size_t n) {
  if (w->erro || !s) return;
  if (w->n + n + 1 > w->cap) {
    size_t nova = w->cap ? w->cap * 2 : 256;
    char *q;
    while (nova < w->n + n + 1) nova *= 2;
    q = (char *)realloc(w->p, nova);
    if (!q) { w->erro = 1; return; }
    w->p = q;
    w->cap = nova;
  }
  memcpy(w->p + w->n, s, n);
  w->n += n;
  w->p[w->n] = 0;
}

static void porc(Jsw *w, char c) { por(w, &c, 1); }

// Chamado ANTES de todo valor: escreve a virgula que separa do anterior. E a
// unica virgula do arquivo, e por isso ela nunca sobra no fim de uma lista.
static void separar(Jsw *w) {
  if (w->erro) return;
  if (w->prof > 0 && w->prof <= JSW_PROF) {
    if (w->primeiro[w->prof - 1]) w->primeiro[w->prof - 1] = 0;
    else porc(w, ',');
  }
}

static void entrar(Jsw *w, char abre) {
  separar(w);
  porc(w, abre);
  if (w->prof >= JSW_PROF) { w->erro = 1; return; }
  w->primeiro[w->prof] = 1;
  w->prof++;
}

static void sairNivel(Jsw *w, char fecha) {
  if (w->prof > 0) w->prof--;
  porc(w, fecha);
}

void jsw_iniciar(Jsw *w) {
  memset(w, 0, sizeof *w);
}

void jsw_livre(Jsw *w) {
  free(w->p);
  memset(w, 0, sizeof *w);
}

const char *jsw_texto_final(const Jsw *w) {
  return (w->erro || !w->p) ? NULL : w->p;
}

void jsw_obj_ini(Jsw *w) { entrar(w, '{'); }
void jsw_obj_fim(Jsw *w) { sairNivel(w, '}'); }
void jsw_arr_ini(Jsw *w) { entrar(w, '['); }
void jsw_arr_fim(Jsw *w) { sairNivel(w, ']'); }

// A string em si, SEM passar pelo separador. Existe por causa de um defeito
// real: jsw_chave separava e depois chamava jsw_str, que separava de novo — e
// como o primeiro separar ja tinha consumido a marca de "primeiro do nivel", o
// segundo escrevia uma virgula. O corpo saia como {,"a":1} e o servidor
// respondia "Empty or invalid json", sem dizer onde.
static void escreverStr(Jsw *w, const char *s) {
  const unsigned char *p;
  porc(w, '"');
  for (p = (const unsigned char *)s; *p; p++) {
    switch (*p) {
      case '"':  por(w, "\\\"", 2); break;
      case '\\': por(w, "\\\\", 2); break;
      case '\n': por(w, "\\n", 2);  break;
      case '\r': por(w, "\\r", 2);  break;
      case '\t': por(w, "\\t", 2);  break;
      default:
        // Controle abaixo de 0x20 TEM de virar \u00XX; solto, ele torna o
        // documento invalido e o servidor recusa o corpo inteiro. Acima de
        // 0x7F o byte passa cru: o conteudo daqui e UTF-8, que e o que o JSON
        // espera, e escapar byte a byte quebraria os acentos.
        if (*p < 0x20) {
          char u[7];
          snprintf(u, sizeof u, "\\u%04x", (unsigned)*p);
          por(w, u, 6);
        } else {
          porc(w, (char)*p);
        }
    }
  }
  porc(w, '"');
}

void jsw_str(Jsw *w, const char *s) {
  if (!s) { jsw_nulo(w); return; }
  separar(w);
  escreverStr(w, s);
}

// A chave leva a virgula do nivel; o valor que vem depois dela nao leva outra,
// e por isso a marca de "primeiro" e reposta logo em seguida.
void jsw_chave(Jsw *w, const char *nome) {
  separar(w);
  escreverStr(w, nome ? nome : "");
  porc(w, ':');
  if (w->prof > 0 && w->prof <= JSW_PROF) w->primeiro[w->prof - 1] = 1;
}


void jsw_int(Jsw *w, long long v) {
  char b[32];
  separar(w);
  snprintf(b, sizeof b, "%lld", v);
  por(w, b, strlen(b));
}

void jsw_bool(Jsw *w, int v) {
  separar(w);
  if (v) por(w, "true", 4); else por(w, "false", 5);
}

void jsw_nulo(Jsw *w) {
  separar(w);
  por(w, "null", 4);
}

void jsw_bruto(Jsw *w, const char *json) {
  if (!json || !*json) { jsw_nulo(w); return; }
  separar(w);
  por(w, json, strlen(json));
}

void jsw_cs(Jsw *w, const char *chave, const char *valor) {
  jsw_chave(w, chave);
  jsw_str(w, valor);
}

void jsw_ci(Jsw *w, const char *chave, long long valor) {
  jsw_chave(w, chave);
  jsw_int(w, valor);
}

void jsw_cb(Jsw *w, const char *chave, int valor) {
  jsw_chave(w, chave);
  jsw_bool(w, valor);
}
#endif
