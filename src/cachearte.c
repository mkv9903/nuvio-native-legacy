#include "cachearte.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

/* NOME DO ARQUIVO CALCULADO UMA VEZ, NA CRIACAO DO PINO (22/09/2026).
 *
 * A versao anterior recalculava o FNV da URL de CADA pino para CADA arquivo
 * da pasta: atualizarInventarioNativo (tex_cache.c) varria o diretorio e
 * chamava cachearte_nativo_essencial por arquivo, que andava a lista de pinos
 * inteira fazendo hash. Com 3309 arquivos e ~500 pinos da home isso e 1,6
 * milhao de hashes de ~100 bytes, duas vezes por download e sob discoMtx.
 * MEDIDO no Mac (tests/texlento.c, 3000 arquivos, 500 pinos, disco
 * instantaneo): 447 ms por download so disso; na C9, ~10x mais lenta, e o
 * piso de ~3,9 s de persist_ms que o log mostrava em todo fetch http.
 * Aqui o nome fica no pino e a comparacao e um strcmp de 13 bytes. */
typedef struct NativePin {
  char *url; char name[32]; unsigned long h;
  int group, essential, inUse; struct NativePin *next;
} NativePin;
static pthread_mutex_t s_native_mtx = PTHREAD_MUTEX_INITIALIZER;
static NativePin *s_pins;
static _Atomic long s_hits, s_misses, s_writes, s_errors, s_items, s_bytes;
static _Atomic long s_essential_present;
static _Atomic int s_inventory_running;
static char s_native_dir[512];

static unsigned long native_hash(const char *url) {
  unsigned long h = 2166136261UL; const char *p = url;
  for (; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
  return h;
}
/* Mesmo nome que nomeDeCache (tex_cache.c) da: hash de 8 hex + extensao. */
static void native_name(const char *url, char *name, size_t cap) {
  unsigned long h = native_hash(url); const char *dot = strrchr(url, '.'); char ext[8] = ".jpg";
  if (dot && strlen(dot) <= 5 && !strchr(dot, '/')) snprintf(ext, sizeof ext, "%s", dot);
  snprintf(name, cap, "%08lx%s", h, ext);
}
static const char *base_name(const char *path) {
  const char *b = strrchr(path, '/'); return b ? b + 1 : path;
}

static NativePin *native_find(const char *url, int group, int create) {
  NativePin *p;
  for (p = s_pins; p; p = p->next)
    if (p->group == group && !strcmp(p->url, url)) return p;
  if (!create || !(p = (NativePin *)calloc(1, sizeof *p))) return NULL;
  p->url = strdup(url); if (!p->url) { free(p); return NULL; } p->group = group;
  p->h = native_hash(url); native_name(url, p->name, sizeof p->name);
  p->next = s_pins; s_pins = p; return p;
}

/* INDICE EM MEMORIA DA PASTA DE CACHE (22/09/2026).
 *
 * Antes cada download relia a pasta inteira: prepararDisco -> readdir+lstat de
 * todos os arquivos, e de novo depois da gravacao, sempre sob discoMtx. Com
 * 3309 arquivos no eMMC da C9 isso segurava a trava por segundos, e o acerto de
 * disco dos OUTROS fios (garantirLocal, inclusive a chamada que o fio de decode
 * faz) esperava na mesma trava: `phase=disk-cache cache_ms=4568` e `ler 4479`.
 * Agora a pasta e lida UMA vez por sessao, num fio proprio, e daqui em diante
 * so o fio de gravacao, a poda e a invalidacao mexem no indice, em O(1).
 * A trava do indice so cobre operacoes de memoria: nenhum I/O e feito com ela. */
typedef struct NvEnt { char nome[24]; long bytes; long uso; struct NvEnt *prox; } NvEnt;
#define NV_IDX_BALDES 4096
static pthread_mutex_t s_idx_mtx = PTHREAD_MUTEX_INITIALIZER;
static NvEnt *s_idx[NV_IDX_BALDES];
static long s_idx_n, s_idx_bytes;
static _Atomic long s_idx_bytes_pub;
/* 0 = nao lido, 1 = lendo, 2 = pronto. */
static _Atomic int s_idx_estado;

static unsigned idx_balde(const char *nome) {
  unsigned long h = 5381; const unsigned char *p = (const unsigned char *)nome;
  for (; *p; p++) h = h * 33 + *p;
  return (unsigned)(h % NV_IDX_BALDES);
}
/* Chamador segura s_idx_mtx. */
static NvEnt **idx_achar(const char *nome) {
  NvEnt **e = &s_idx[idx_balde(nome)];
  while (*e && strcmp((*e)->nome, nome)) e = &(*e)->prox;
  return e;
}
static void idx_por(const char *nome, long bytes, long uso, int substituir) {
  NvEnt **e;
  if (strlen(nome) >= sizeof((NvEnt *)0)->nome) return;
  pthread_mutex_lock(&s_idx_mtx);
  e = idx_achar(nome);
  if (*e) {
    if (substituir) { s_idx_bytes += bytes - (*e)->bytes; (*e)->bytes = bytes; (*e)->uso = uso; }
  } else {
    NvEnt *n = (NvEnt *)calloc(1, sizeof *n);
    if (n) {
      snprintf(n->nome, sizeof n->nome, "%s", nome); n->bytes = bytes; n->uso = uso;
      *e = n; s_idx_n++; s_idx_bytes += bytes;
    }
  }
  atomic_store(&s_idx_bytes_pub, s_idx_bytes);
  pthread_mutex_unlock(&s_idx_mtx);
}
static int native_image_name(const char *name) {
  const char *p = name;
  while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
         (*p >= 'A' && *p <= 'F')) p++;
  if (p - name < 8 || p - name > 16) return 0;
  return !strcmp(p, ".jpg") || !strcmp(p, ".jpeg") || !strcmp(p, ".png") ||
         !strcmp(p, ".webp") || !strcmp(p, ".gif");
}
/* A UNICA VARREDURA DA SESSAO. Roda sem trava nenhuma durante o readdir/lstat;
 * so a insercao pega s_idx_mtx, e com "nao substituir": o que o fio de
 * gravacao registrou enquanto isto rodava e mais novo que o lstat daqui. */
void cachearte_nativo_indice_construir(void) {
  char dir[512]; DIR *d; struct dirent *e; int esperado = 0; long n = 0, b = 0;
  if (!atomic_compare_exchange_strong(&s_idx_estado, &esperado, 1)) return;
  pthread_mutex_lock(&s_native_mtx);
  snprintf(dir, sizeof dir, "%s", s_native_dir);
  pthread_mutex_unlock(&s_native_mtx);
  d = dir[0] ? opendir(dir) : NULL;
  if (d) {
    while ((e = readdir(d)) != NULL) {
      char path[1024]; struct stat st;
      if (!native_image_name(e->d_name)) continue;
      snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
      if (lstat(path, &st) || !S_ISREG(st.st_mode)) continue;
      idx_por(e->d_name, (long)st.st_size, (long)st.st_mtime, 0);
      n++; b += (long)st.st_size;
    }
    closedir(d);
  }
  atomic_store(&s_idx_estado, 2);
  if (n) { printf("[tex] cache de disco ja tinha %ld arquivo(s), %.1f MB\n", n, b / 1048576.0); fflush(stdout); }
}
int cachearte_nativo_indice_pronto(void) { return atomic_load(&s_idx_estado) == 2; }
void cachearte_nativo_indice_registrar(const char *path, long bytes) {
  if (path && native_image_name(base_name(path))) idx_por(base_name(path), bytes, (long)time(NULL), 1);
}
void cachearte_nativo_indice_remover(const char *path) {
  NvEnt **e, *x;
  if (!path) return;
  pthread_mutex_lock(&s_idx_mtx);
  e = idx_achar(base_name(path));
  if ((x = *e) != NULL) { *e = x->prox; s_idx_n--; s_idx_bytes -= x->bytes; free(x); }
  atomic_store(&s_idx_bytes_pub, s_idx_bytes);
  pthread_mutex_unlock(&s_idx_mtx);
}
/* Acerto de disco: o LRU passa a ver o uso sem nenhuma escrita no eMMC. */
void cachearte_nativo_indice_tocar(const char *path) {
  NvEnt *x;
  if (!path) return;
  pthread_mutex_lock(&s_idx_mtx);
  if ((x = *idx_achar(base_name(path))) != NULL) x->uso = (long)time(NULL);
  pthread_mutex_unlock(&s_idx_mtx);
}
int cachearte_nativo_indice_tem(const char *path) {
  int r;
  if (!path || atomic_load(&s_idx_estado) != 2) return 0;
  pthread_mutex_lock(&s_idx_mtx);
  r = *idx_achar(base_name(path)) != NULL;
  pthread_mutex_unlock(&s_idx_mtx);
  return r;
}
long cachearte_nativo_indice_bytes(void) { return atomic_load(&s_idx_bytes_pub); }

typedef struct { char nome[24]; long bytes; long uso; } NvCand;
static int cand_ordem(const void *a, const void *b) {
  const NvCand *x = a, *y = b;
  if (x->uso != y->uso) return x->uso < y->uso ? -1 : 1;
  return strcmp(x->nome, y->nome);
}
/* PODA PELO INDICE, com a mesma politica de nv_cache_podar (mais velho
 * primeiro, folga de 25%, reserva de espaco livre, protegidos ficam). A copia
 * dos candidatos e feita sob s_idx_mtx; o unlink e o callback de protecao,
 * que pega a trava das texturas, rodam sem ela. So o fio de gravacao chama
 * isto no caminho normal. */
long cachearte_nativo_podar(long entrada, long teto, uint64_t reserva, int forcar,
                            NvCacheProtegido protegido, void *ctx) {
  static pthread_mutex_t so_uma = PTHREAD_MUTEX_INITIALIZER;
  char dir[512]; struct statvfs fs; uint64_t livre = 0, falta = 0;
  long total, alvo = teto, apagados = 0, liberados = 0;
  NvCand *lista = NULL; size_t n = 0, i;
  if (!cachearte_nativo_indice_pronto()) return 0;
  if (pthread_mutex_trylock(&so_uma)) return 0;   /* outra poda ja esta nisso */
  pthread_mutex_lock(&s_native_mtx);
  snprintf(dir, sizeof dir, "%s", s_native_dir);
  pthread_mutex_unlock(&s_native_mtx);
  if (dir[0] && statvfs(dir, &fs) == 0) {
    livre = (uint64_t)fs.f_bavail * fs.f_frsize;
    if (livre < reserva + (uint64_t)entrada) falta = reserva + (uint64_t)entrada - livre;
  }
  total = cachearte_nativo_indice_bytes();
  if (!forcar && total <= teto - entrada && !falta) { pthread_mutex_unlock(&so_uma); return 0; }
  if (total > teto - entrada) alvo = teto - teto / 4 - entrada;
  if (alvo < 0) alvo = 0;
  if (forcar && falta < 32UL * 1024 * 1024) falta = 32UL * 1024 * 1024;
  pthread_mutex_lock(&s_idx_mtx);
  lista = s_idx_n > 0 ? (NvCand *)malloc((size_t)s_idx_n * sizeof *lista) : NULL;
  if (lista) {
    for (i = 0; i < NV_IDX_BALDES; i++) {
      NvEnt *e;
      for (e = s_idx[i]; e && n < (size_t)s_idx_n; e = e->prox) {
        memcpy(lista[n].nome, e->nome, sizeof lista[n].nome);
        lista[n].bytes = e->bytes; lista[n].uso = e->uso; n++;
      }
    }
  }
  pthread_mutex_unlock(&s_idx_mtx);
  if (n > 1) qsort(lista, n, sizeof *lista, cand_ordem);
  for (i = 0; i < n && (total > alvo || (uint64_t)liberados < falta); i++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, lista[i].nome);
    if (protegido && protegido(path, ctx)) continue;
    if (unlink(path) && errno != ENOENT) continue;
    cachearte_nativo_indice_remover(path);
    total -= lista[i].bytes; liberados += lista[i].bytes; apagados++;
  }
  free(lista);
  pthread_mutex_unlock(&so_uma);
  if (apagados) {
    printf("[tex] cache de disco podado: %ld arquivo(s), %.1f MB liberados, agora %.1f MB (teto %.0f MB)\n",
           apagados, liberados / 1048576.0, cachearte_nativo_indice_bytes() / 1048576.0,
           teto / 1048576.0);
    fflush(stdout);
  }
  return apagados;
}

void cachearte_marcar_grupo(int grupo, const char *url, int variante,
                            int essencial, int emUso) {
  NativePin *p, **link; (void)variante; if (!url || !*url) return;
  pthread_mutex_lock(&s_native_mtx); p = native_find(url, grupo, 1);
  if (p) { p->essential = !!essencial; p->inUse = !!emUso; }
  for (link = &s_pins; *link;) {
    p = *link;
    if (!p->essential && !p->inUse) { *link = p->next; free(p->url); free(p); }
    else link = &p->next;
  }
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_limpar_uso(void) {
  NativePin *p, **link; pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next) p->inUse = 0;
  for (link = &s_pins; *link;) {
    p = *link;
    if (!p->essential) { *link = p->next; free(p->url); free(p); }
    else link = &p->next;
  }
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_limpar_referencias(void) {
  NativePin *p, **link; pthread_mutex_lock(&s_native_mtx);
  for (link = &s_pins; *link;) {
    p = *link; *link = p->next; free(p->url); free(p);
  }
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_limpar_referencias_grupo(int grupo) {
  NativePin *p, **link; pthread_mutex_lock(&s_native_mtx);
  for (link = &s_pins; *link;) {
    p = *link;
    if (p->group == grupo) { *link = p->next; free(p->url); free(p); }
    else link = &p->next;
  }
  pthread_mutex_unlock(&s_native_mtx);
}
/* INVENTARIO SEM VARRER A PASTA. Itens e bytes vem do indice; "essenciais
 * presentes" e uma consulta ao indice por pino essencial, O(pinos). A pasta so
 * e lida se o indice ainda nao existe, e ai num fio proprio, uma vez. Antes
 * main.c pedia isto a cada 3 s e cada pedido criava um fio que relia 3309
 * arquivos no eMMC e fazia o hash de todos os pinos por arquivo. */
static void publicar_inventario(void) {
  NativePin *p; long essential = 0;
  pthread_mutex_lock(&s_native_mtx);
  pthread_mutex_lock(&s_idx_mtx);
  for (p = s_pins; p; p = p->next) if (p->essential) {
    NativePin *q; int first = 1;
    for (q = s_pins; q != p; q = q->next)
      if (q->essential && q->h == p->h && !strcmp(q->name, p->name)) { first = 0; break; }
    if (first && *idx_achar(p->name)) essential++;
  }
  atomic_store(&s_items, s_idx_n); atomic_store(&s_bytes, s_idx_bytes);
  pthread_mutex_unlock(&s_idx_mtx);
  pthread_mutex_unlock(&s_native_mtx);
  atomic_store(&s_essential_present, essential);
}
static void *native_inventory_thread(void *unused) {
  (void)unused;
  cachearte_nativo_indice_construir();
  publicar_inventario();
  atomic_store(&s_inventory_running, 0);
  return NULL;
}
void cachearte_nativo_configurar_diretorio(const char *dir) {
  pthread_mutex_lock(&s_native_mtx);
  snprintf(s_native_dir, sizeof s_native_dir, "%s", dir ? dir : "");
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_estatisticas_pedir(void) {
  pthread_t thread; int expected = 0;
  if (cachearte_nativo_indice_pronto()) { publicar_inventario(); return; }
  if (!atomic_compare_exchange_strong(&s_inventory_running, &expected, 1)) return;
  if (pthread_create(&thread, NULL, native_inventory_thread, NULL) != 0) {
    atomic_store(&s_inventory_running, 0); return;
  }
  pthread_detach(thread);
}
void cachearte_estatisticas(NvCacheArteStats *out) {
  NativePin *p;
  long expected = 0;
  if (!out) return;
  /* Deduplica por nome de arquivo (o hash compara primeiro): o mesmo arquivo
   * pinado pela home e pelo perfil conta uma vez, como antes. */
  pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next) if (p->essential) {
    NativePin *prior; int first = 1;
    for (prior = s_pins; prior != p; prior = prior->next)
      if (prior->essential && prior->h == p->h && !strcmp(prior->url, p->url)) { first = 0; break; }
    if (first) expected++;
  }
  pthread_mutex_unlock(&s_native_mtx);
  out->itens = atomic_load(&s_items); out->bytes = atomic_load(&s_bytes);
  out->essenciais = atomic_load(&s_essential_present); out->essenciais_esperados = expected;
  out->hits = atomic_load(&s_hits);
  out->misses = atomic_load(&s_misses); out->gravacoes = atomic_load(&s_writes);
  out->falhas = atomic_load(&s_errors);
}
void cachearte_nativo_hit(void) { atomic_fetch_add(&s_hits, 1); }
void cachearte_nativo_miss(void) { atomic_fetch_add(&s_misses, 1); }
void cachearte_nativo_gravacao(int ok) { if (ok) atomic_fetch_add(&s_writes, 1); else atomic_fetch_add(&s_errors, 1); }
int cachearte_nativo_protegido(const char *path) {
  NativePin *p; const char *base; int protected = 0;
  if (!path) return 0; base = base_name(path);
  pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next)
    if ((p->essential || p->inUse) && !strcmp(p->name, base)) { protected = 1; break; }
  pthread_mutex_unlock(&s_native_mtx); return protected;
}
int cachearte_nativo_essencial(const char *path) {
  NativePin *p; const char *base; int essential = 0;
  if (!path) return 0; base = base_name(path);
  pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next)
    if (p->essential && !strcmp(p->name, base)) { essential = 1; break; }
  pthread_mutex_unlock(&s_native_mtx); return essential;
}
