#include "progress.h"
#include "idbase.h"
#include "dados.h"
#include "perfis.h"
#include <pthread.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define FILE_NAME "progresso.txt"
#define FILE_HEADER "#nvprog2"

// Sync converts seconds to integer milliseconds. NaN, infinity and out-of-range
// values are not valid progress and must not reach file or casts.
static int is_valid_time(double pos, double dur) {
  const double limit = (double)LLONG_MAX / 1000.0;
  return isfinite(pos) && isfinite(dur) && dur > 1.0 &&
         pos < limit && dur < limit;
}

// Two threads touch this: main (player closing, watch toggle, post-play, catalog
// re-applying) and sync (pending for push, mark pushed). One mutex per public
// call; internal helpers assume mutex held.
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define ACQUIRE_LOCK() pthread_mutex_lock(&lock)
#define RELEASE_LOCK() pthread_mutex_unlock(&lock)

static ProgressRecord records[PROGRESS_MAX];
static int num_records;
static int is_loaded;
static long long (*custom_clock)(void);

static long long default_clock(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

long long progress_now_ms(void) { return custom_clock ? custom_clock() : default_clock(); }
void progress_set_clock(long long (*c)(void)) { custom_clock = c; }
void progress_invalidate(void) { ACQUIRE_LOCK(); is_loaded = 0; num_records = 0; RELEASE_LOCK(); }

// ------------------------------------------------------------ identity (stateless)

void progress_content_id(char *dst, unsigned n, const char *imdb, int *season, int *episode) {
  size_t len;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!imdb) return;
  len = idbase_len(imdb);
  if (len >= n) len = n - 1;
  memcpy(dst, imdb, len);
  dst[len] = 0;
  if (season && episode && imdb[idbase_len(imdb)] == ':') {
    int s = 0, e = 0;
    if (sscanf(imdb + idbase_len(imdb) + 1, "%d:%d", &s, &e) == 2 && s >= 0 && e > 0) {
      *season = s;
      *episode = e;
    }
  }
}

void progress_key(char *dst, unsigned n, const char *content_id, int season, int episode) {
  char id[64];
  if (!dst || !n) return;
  progress_content_id(id, sizeof id, content_id, NULL, NULL);
  if (id[0] && season >= 0 && episode > 0)
    snprintf(dst, n, "%s_s%de%d", id, season, episode);
  else
    snprintf(dst, n, "%s", id);
}

// ------------------------------------------------------------ disk (mutex held)

// LEGACY format line: "imdb pos dur [season ep]", written by catalogo.c.
static int parse_legacy_line(const char *line, ProgressRecord *r) {
  char id[40];
  double pos, dur;
  int season = 0, ep = 0, n;
  n = sscanf(line, "%39s %lf %lf %d %d", id, &pos, &dur, &season, &ep);
  if (n < 3 || !is_valid_time(pos, dur)) return 0;
  memset(r, 0, sizeof *r);
  {
    int s_id = 0, e_id = 0;
    progress_content_id(r->content_id, sizeof r->content_id, id, &s_id, &e_id);
    if (!(season >= 0 && ep > 0)) { season = s_id; ep = e_id; }
  }
  if (!r->content_id[0]) return 0;
  r->season = ep > 0 ? season : 0;
  r->episode = ep > 0 ? ep : 0;
  snprintf(r->type, sizeof r->type, "%s", ep > 0 ? "series" : "movie");
  progress_key(r->key, sizeof r->key, r->content_id, r->season, r->episode);
  r->pos_sec = pos < 0 ? 0 : pos;
  r->dur_sec = dur;
  r->last_watched_ms = 0;
  r->pending = 1;        // never pushed with correct key
  r->profile = perfis_ativo();
  return 1;
}

// NEW format line (10 tab-separated columns):
// profile key content_id type season episode pos_sec dur_sec last_ms pending
static int parse_new_line(const char *line, ProgressRecord *r) {
  int n;
  memset(r, 0, sizeof *r);
  n = sscanf(line, "%d\t%47s\t%23s\t%7s\t%d\t%d\t%lf\t%lf\t%lld\t%d",
             &r->profile, r->key, r->content_id, r->type,
             &r->season, &r->episode, &r->pos_sec, &r->dur_sec,
             &r->last_watched_ms, &r->pending);
  if (n != 10 || !r->key[0] || !r->content_id[0] ||
      !is_valid_time(r->pos_sec, r->dur_sec)) return 0;
  if (r->pos_sec < 0) r->pos_sec = 0;
  return 1;
}

static void load_file(void) {
  char *buf, *line, *ctx;
  int is_new_format = 0;
  if (is_loaded) return;
  is_loaded = 1;
  num_records = 0;
  buf = dados_ler(FILE_NAME);
  if (!buf) return;
  for (line = strtok_r(buf, "\n", &ctx); line && num_records < PROGRESS_MAX;
       line = strtok_r(NULL, "\n", &ctx)) {
    ProgressRecord r;
    if (!line[0] || line[0] == '\r') continue;
    if (!strncmp(line, FILE_HEADER, strlen(FILE_HEADER))) { is_new_format = 1; continue; }
    if (is_new_format ? parse_new_line(line, &r) : parse_legacy_line(line, &r)) {
      // Duplicate (same profile and key) keeps newer timestamp.
      int i, dup = -1;
      for (i = 0; i < num_records; i++)
        if (records[i].profile == r.profile && !strcmp(records[i].key, r.key)) { dup = i; break; }
      if (dup >= 0) {
        if (r.last_watched_ms >= records[dup].last_watched_ms) records[dup] = r;
      } else {
        records[num_records++] = r;
      }
    }
  }
  free(buf);
  if (!is_new_format && num_records)
    printf("[progress] %d rows migrated from legacy format\n", num_records);
}

static int save_file(void) {
  size_t size = (size_t)num_records * 256 + 64;
  char *buf = malloc(size), *p;
  int i, ok, written;
  if (!buf) return 0;
  p = buf;
  p += snprintf(p, size, "%s\n", FILE_HEADER);
  for (i = 0; i < num_records; i++) {
    const ProgressRecord *r = &records[i];
    size_t available = size - (size_t)(p - buf);
    written = snprintf(p, available, "%d\t%s\t%s\t%s\t%d\t%d\t%.0f\t%.0f\t%lld\t%d\n",
                       r->profile, r->key, r->content_id, r->type,
                       r->season, r->episode, r->pos_sec, r->dur_sec,
                       r->last_watched_ms, r->pending ? 1 : 0);
    if (written < 0 || (size_t)written >= available) { free(buf); return 0; }
    p += written;
  }
  ok = dados_gravar(FILE_NAME, buf);
  free(buf);
  return ok;
}

// When file is full, remove oldest NON-pending row.
static int find_slot(void) {
  int i, target = -1;
  if (num_records < PROGRESS_MAX) return num_records;
  for (i = 0; i < num_records; i++) {
    if (records[i].pending) continue;
    if (target < 0 || records[i].last_watched_ms < records[target].last_watched_ms)
      target = i;
  }
  return target;
}

static int find_record(int profile, const char *key) {
  int i;
  for (i = 0; i < num_records; i++)
    if (records[i].profile == profile && !strcmp(records[i].key, key)) return i;
  return -1;
}

static int newest_first(const void *a, const void *b) {
  const ProgressRecord *x = a, *y = b;
  if (x->last_watched_ms != y->last_watched_ms)
    return x->last_watched_ms < y->last_watched_ms ? 1 : -1;
  return strcmp(x->key, y->key);
}

// ------------------------------------------------------------ read

int progress_read(ProgressRecord *out, int max) {
  int i, k = 0, oldest = -1, profile = perfis_ativo();
  if (!out || max < 1) return 0;
  ACQUIRE_LOCK();
  load_file();
  for (i = 0; i < num_records; i++) {
    int j;
    if (records[i].profile != profile) continue;
    if (k < max) { out[k++] = records[i]; continue; }
    if (oldest < 0) {
      oldest = 0;
      for (j = 1; j < k; j++)
        if (newest_first(&out[j], &out[oldest]) > 0) oldest = j;
    }
    if (newest_first(&records[i], &out[oldest]) < 0) {
      out[oldest] = records[i];
      oldest = -1;
    }
  }
  RELEASE_LOCK();
  qsort(out, (size_t)k, sizeof *out, newest_first);
  return k;
}

static int removed_wins_from(int profile, const char *imdb, long long timestamp_ms);

int progress_continue_from_profile(int profile, int completed_pct, ProgressRecord *out) {
  int i, best = -1;
  if (!out) return 0;
  ACQUIRE_LOCK();
  load_file();
  for (i = 0; i < num_records; i++) {
    const ProgressRecord *r = &records[i];
    double p;
    if (r->profile != profile || r->dur_sec < 60.0) continue;
    p = r->pos_sec / r->dur_sec;
    if (p < 0.01 || p * 100.0 >= completed_pct) continue;
    if (removed_wins_from(profile, r->content_id, r->last_watched_ms)) continue;
    if (best < 0 || newest_first(r, &records[best]) < 0) best = i;
  }
  if (best >= 0) *out = records[best];
  RELEASE_LOCK();
  return best >= 0;
}

int progress_by_key(const char *key, ProgressRecord *out) {
  int i;
  if (!key || !*key) return 0;
  ACQUIRE_LOCK();
  load_file();
  i = find_record(perfis_ativo(), key);
  if (i >= 0 && out) *out = records[i];
  RELEASE_LOCK();
  return i >= 0;
}

int progress_pending(ProgressRecord *out, int max) {
  int i, k = 0, profile = perfis_ativo();
  if (!out || max < 1) return 0;
  ACQUIRE_LOCK();
  load_file();
  for (i = 0; i < num_records && k < max; i++)
    if (records[i].profile == profile && records[i].pending) out[k++] = records[i];
  RELEASE_LOCK();
  return k;
}

// ------------------------------------------------------------ write

int progress_write_local(const char *imdb, int season, int episode,
                         double pos_sec, double dur_sec) {
  ProgressRecord r;
  int i, ok;
  if (!imdb || !*imdb || !is_valid_time(pos_sec, dur_sec)) return 0;
  memset(&r, 0, sizeof r);
  {
    int s_id = 0, e_id = 0;
    progress_content_id(r.content_id, sizeof r.content_id, imdb, &s_id, &e_id);
    if (!(episode > 0)) { season = s_id; episode = e_id; }
  }
  if (!r.content_id[0]) return 0;
  r.profile = perfis_ativo();
  r.season = episode > 0 ? season : 0;
  r.episode = episode > 0 ? episode : 0;
  snprintf(r.type, sizeof r.type, "%s", episode > 0 ? "series" : "movie");
  progress_key(r.key, sizeof r.key, r.content_id, r.season, r.episode);
  r.pos_sec = pos_sec < 0 ? 0 : pos_sec;
  r.dur_sec = dur_sec;
  r.last_watched_ms = progress_now_ms();
  r.pending = 1;
  ACQUIRE_LOCK();
  load_file();
  i = find_record(r.profile, r.key);
  if (i < 0) { i = find_slot(); if (i >= 0 && i == num_records) num_records++; }
  if (i < 0) { RELEASE_LOCK(); return 0; }
  records[i] = r;
  ok = save_file();
  RELEASE_LOCK();
  return ok;
}

int progress_apply_remote(const ProgressRecord *rem) {
  ProgressRecord r;
  int i;
  if (!rem || !rem->content_id[0] || !is_valid_time(rem->pos_sec, rem->dur_sec) ||
      !memchr(rem->content_id, 0, sizeof rem->content_id) ||
      !memchr(rem->key, 0, sizeof rem->key) ||
      !memchr(rem->type, 0, sizeof rem->type)) return 0;
  r = *rem;
  r.profile = perfis_ativo();
  r.pending = 0;
  if (r.pos_sec < 0) r.pos_sec = 0;
  if (!r.key[0]) progress_key(r.key, sizeof r.key, r.content_id, r.season, r.episode);
  if (!r.type[0]) snprintf(r.type, sizeof r.type, "%s", r.episode > 0 ? "series" : "movie");
  ACQUIRE_LOCK();
  load_file();
  i = find_record(r.profile, r.key);
  if (i >= 0) {
    if (records[i].pending || r.last_watched_ms <= records[i].last_watched_ms) {
      RELEASE_LOCK();
      return 0;
    }
  } else {
    i = find_slot();
    if (i < 0) { RELEASE_LOCK(); return 0; }
    if (i == num_records) num_records++;
  }
  records[i] = r;
  save_file();
  RELEASE_LOCK();
  return 1;
}

void progress_confirm_pushed(const ProgressRecord *sent, int n) {
  int i, k, changed = 0;
  if (!sent || n < 1) return;
  ACQUIRE_LOCK();
  load_file();
  for (k = 0; k < n; k++) {
    const ProgressRecord *e = &sent[k];
    if (!memchr(e->key, 0, sizeof e->key)) continue;
    i = find_record(e->profile, e->key);
    if (i >= 0 && records[i].pending && records[i].last_watched_ms == e->last_watched_ms &&
        records[i].pos_sec == e->pos_sec && records[i].dur_sec == e->dur_sec) {
      records[i].pending = 0;
      changed = 1;
    }
  }
  if (changed) save_file();
  RELEASE_LOCK();
}


void progress_remove(const char *key) {
  int i;
  if (!key || !*key) return;
  ACQUIRE_LOCK();
  load_file();
  i = find_record(perfis_ativo(), key);
  if (i >= 0) { records[i] = records[--num_records]; save_file(); }
  RELEASE_LOCK();
}

// --- DISMISSALS FROM "CONTINUE WATCHING" --------------------------------------
#define PROGRESS_REMOVED_MAX 32
static struct { int profile; char item_id[24]; long long timestamp_ms; } removed_items[PROGRESS_REMOVED_MAX];
static int num_removed;

void progress_mark_removed(const char *imdb) {
  char item_id[24];
  int i, target = -1, profile = perfis_ativo();
  long long now = progress_now_ms();
  progress_content_id(item_id, sizeof item_id, imdb, NULL, NULL);
  if (!item_id[0]) return;
  ACQUIRE_LOCK();
  for (i = 0; i < num_removed && target < 0; i++)
    if (removed_items[i].profile == profile && !strcmp(removed_items[i].item_id, item_id))
      target = i;
  if (target < 0 && num_removed < PROGRESS_REMOVED_MAX) target = num_removed++;
  if (target < 0) {
    target = 0;
    for (i = 1; i < num_removed; i++)
      if (removed_items[i].timestamp_ms < removed_items[target].timestamp_ms)
        target = i;
  }
  removed_items[target].profile = profile;
  snprintf(removed_items[target].item_id, sizeof removed_items[target].item_id, "%s", item_id);
  removed_items[target].timestamp_ms = now;
  RELEASE_LOCK();
}

int progress_removed_wins(const char *imdb, long long timestamp_ms) {
  int res;
  ACQUIRE_LOCK();
  res = removed_wins_from(perfis_ativo(), imdb, timestamp_ms);
  RELEASE_LOCK();
  return res;
}

static int removed_wins_from(int profile, const char *imdb, long long timestamp_ms) {
  char item_id[24];
  int i;
  long long ms = 0;
  progress_content_id(item_id, sizeof item_id, imdb, NULL, NULL);
  if (!item_id[0]) return 0;
  for (i = 0; i < num_removed; i++)
    if (removed_items[i].profile == profile && !strcmp(removed_items[i].item_id, item_id)) {
      ms = removed_items[i].timestamp_ms;
      break;
    }
  if (!ms) return 0;
  if (timestamp_ms > ms) return 0;
  load_file();
  for (i = 0; i < num_records; i++)
    if (records[i].profile == profile && !strcmp(records[i].content_id, item_id) &&
        records[i].last_watched_ms > ms) return 0;
  return 1;
}

void progress_forget_all(void) {
  ACQUIRE_LOCK();
  dados_apagar(FILE_NAME);
  is_loaded = 0;
  num_records = 0;
  num_removed = 0;
  RELEASE_LOCK();
}

// Backward compatibility wrappers
void prog_chave(char *dst, unsigned n, const char *contentId, int temporada, int episodio) {
  progress_key(dst, n, contentId, temporada, episodio);
}
void prog_content_id(char *dst, unsigned n, const char *imdb, int *temporada, int *episodio) {
  progress_content_id(dst, n, imdb, temporada, episodio);
}
int prog_ler(ProgRegistro *saida, int max) {
  return progress_read(saida, max);
}
int prog_continuar_de_perfil(int perfil, int pctConcluido, ProgRegistro *saida) {
  return progress_continue_from_profile(perfil, pctConcluido, saida);
}
int prog_por_chave(const char *chave, ProgRegistro *saida) {
  return progress_by_key(chave, saida);
}
int prog_gravar_local(const char *imdb, int temporada, int episodio, double posSeg, double durSeg) {
  return progress_write_local(imdb, temporada, episodio, posSeg, durSeg);
}
int prog_aplicar_remoto(const ProgRegistro *r) {
  return progress_apply_remote(r);
}
int prog_pendentes(ProgRegistro *saida, int max) {
  return progress_pending(saida, max);
}
void prog_confirmar_empurrados(const ProgRegistro *enviados, int n) {
  progress_confirm_pushed(enviados, n);
}
void prog_remover(const char *chave) {
  progress_remove(chave);
}
void prog_marcar_removido(const char *imdb) {
  progress_mark_removed(imdb);
}
int prog_removido_vence(const char *imdb, long long instanteMs) {
  return progress_removed_wins(imdb, instanteMs);
}
void prog_esquecer_tudo(void) {
  progress_forget_all();
}
void prog_invalidar(void) {
  progress_invalidate();
}
long long prog_agora_ms(void) {
  return progress_now_ms();
}
void prog_definir_relogio(long long (*relogio)(void)) {
  progress_set_clock(relogio);
}
