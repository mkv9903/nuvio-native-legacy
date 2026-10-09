#include "account_pending.h"
#include "contacache.h"
#include "session.h"
#include "perfis.h"
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Same prototype used by contalib.c and app.c; defined in catalogo.c.
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

// Batch size per watched RPC. Web sends up to 5000 in one body; 200 keeps body
// below ~30 KB so a failure is cheap to retry.
#define BATCH_SIZE 200
// Pages of sync_pull_library in read-merge-write: same 500 as web.
// Without page limit NO PUSH — an incomplete list cannot replace the remote list.
#define PAGE_SIZE 500
#define MAX_PAGES 40

typedef struct {
  char surface;        // 'V' watched, 'L' list (library/saved)
  char op;             // '+' mark/save, '-' unmark/remove
  char confirmed;      // 1 = account responded 2xx (kept until pruning)
  int  profile;
  char id[24];
  char type[8];
  int  season, episode;// 0/0 = full title (null season/episode)
  long long timestamp_ms; // when the user performed the gesture
  long long confirmed_ms;
  char title[160];
  char poster[400];
} Entry;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Entry *entries;
static int num_entries, cap_entries, is_loaded;
static char current_user[80];
static long long (*custom_clock)(void);
static int threadless;

// ---------------------------------------------------------------- base

long long account_pending_now_ms(void) {
  struct timespec ts;
  if (custom_clock) return custom_clock();
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}
void account_pending_set_clock(long long (*fn)(void)) { custom_clock = fn; }
void account_pending_set_threadless(int enable) { threadless = enable; }

static int is_2xx(const char *r, int st) { return r && st >= 200 && st < 300; }

// Watched: title id is everything before first ':' (syncprog.c and vistoep.c rule).
// List: only the ":<season>:<ep>" digit suffix is removed — "tmdb:55" is intact.
static void id_watched(const char *src, char *dst, size_t size) {
  size_t k = 0;
  dst[0] = 0;
  if (!src) return;
  while (src[k] && src[k] != ':' && k + 1 < size) { dst[k] = src[k]; k++; }
  dst[k] = 0;
}

static void id_list(const char *src, char *dst, size_t size) {
  const char *a, *b;
  size_t n;
  dst[0] = 0;
  if (!src) return;
  n = strlen(src);
  b = strrchr(src, ':');
  if (b && b > src && b[1] && isdigit((unsigned char)b[1])) {
    const char *q = b + 1;
    while (*q && isdigit((unsigned char)*q)) q++;
    if (!*q) {
      a = b - 1;
      while (a > src && isdigit((unsigned char)*a)) a--;
      if (*a == ':' && a < b - 1) n = (size_t)(a - src);
    }
  }
  if (n + 1 > size) n = size - 1;
  memcpy(dst, src, n);
  dst[n] = 0;
}

static void sanitize_string(char *dst, size_t size, const char *src) {
  size_t k = 0;
  if (!src) src = "";
  while (src[k] && k + 1 < size) {
    dst[k] = (src[k] == '\t' || src[k] == '\n' || src[k] == '\r') ? ' ' : src[k];
    k++;
  }
  dst[k] = 0;
}

static void get_journal_filename(char *dst, size_t size) {
  char u[64];
  size_t k = 0, j = 0;
  while (current_user[k] && j + 1 < sizeof u) {
    char c = current_user[k++];
    if (isalnum((unsigned char)c) || c == '-') u[j++] = c;
  }
  u[j] = 0;
  snprintf(dst, size, "conta-pend-%s.txt", u[0] ? u : "anon");
}

// Lock must be held.
static void save_journal(void) {
  char filename[128], *buf, *p;
  size_t size = 64;
  int i;
  for (i = 0; i < num_entries; i++) size += 700;
  buf = (char *)malloc(size);
  if (!buf) return;
  p = buf;
  *p = 0;
  for (i = 0; i < num_entries; i++) {
    const Entry *e = &entries[i];
    int k = snprintf(p, size - (size_t)(p - buf),
                     "%c\t%c\t%d\t%d\t%s\t%s\t%d\t%d\t%lld\t%lld\t%s\t%s\n",
                     e->surface, e->op, e->confirmed, e->profile, e->id, e->type,
                     e->season, e->episode, e->timestamp_ms, e->confirmed_ms,
                     e->title, e->poster);
    if (k < 0 || (size_t)k >= size - (size_t)(p - buf)) break;
    p += k;
  }
  get_journal_filename(filename, sizeof filename);
  if (num_entries) dados_gravar(filename, buf);
  else dados_apagar(filename);
  free(buf);
}

static int ensure_capacity(int n) {
  Entry *x;
  int new_cap;
  if (n <= cap_entries) return 1;
  new_cap = cap_entries ? cap_entries * 2 : 64;
  while (new_cap < n) new_cap *= 2;
  if (new_cap > ACCOUNT_PENDING_MAX) new_cap = ACCOUNT_PENDING_MAX;
  if (new_cap < n) return 0;
  x = (Entry *)realloc(entries, sizeof *x * (size_t)new_cap);
  if (!x) return 0;
  entries = x;
  cap_entries = new_cap;
  return 1;
}

static void load_journal(void) {
  char filename[128], *buf, *line;
  get_journal_filename(filename, sizeof filename);
  buf = dados_ler(filename);
  if (!buf) return;
  for (line = buf; line && *line;) {
    char *end = strchr(line, '\n'), *cols[12];
    int k = 0;
    if (end) *end = 0;
    cols[k++] = line;
    {
      char *q;
      for (q = line; *q && k < 12; q++) if (*q == '\t') { *q = 0; cols[k++] = q + 1; }
    }
    if (k >= 10 && (cols[0][0] == 'V' || cols[0][0] == 'L') &&
        (cols[1][0] == '+' || cols[1][0] == '-') && cols[4][0] &&
        ensure_capacity(num_entries + 1)) {
      Entry *e = &entries[num_entries];
      memset(e, 0, sizeof *e);
      e->surface = cols[0][0];
      e->op = cols[1][0];
      e->confirmed = (char)(atoi(cols[2]) ? 1 : 0);
      e->profile = atoi(cols[3]);
      snprintf(e->id, sizeof e->id, "%s", cols[4]);
      snprintf(e->type, sizeof e->type, "%s", cols[5]);
      e->season = atoi(cols[6]);
      e->episode = atoi(cols[7]);
      e->timestamp_ms = atoll(cols[8]);
      e->confirmed_ms = atoll(cols[9]);
      if (k > 10) snprintf(e->title, sizeof e->title, "%s", cols[10]);
      if (k > 11) snprintf(e->poster, sizeof e->poster, "%s", cols[11]);
      if (e->profile > 0) num_entries++;
    }
    line = end ? end + 1 : NULL;
  }
  free(buf);
}

// Lock must be held. Returns 1 when user is logged in and journal is in memory.
static int ensure_loaded(void) {
  const char *u;
  if (!sessao_logada()) return 0;
  u = sessao_usuario();
  if (!u || !u[0]) return 0;
  if (is_loaded && !strcmp(current_user, u)) return 1;
  num_entries = 0;
  snprintf(current_user, sizeof current_user, "%s", u);
  is_loaded = 1;
  load_journal();
  if (num_entries) {
    int i, p = 0;
    for (i = 0; i < num_entries; i++) if (!entries[i].confirmed) p++;
    printf("[sync] account journal: %d entries, %d pending send\n", num_entries, p);
    fflush(stdout);
  }
  return 1;
}

static int find_entry(char surface, int profile, const char *id, int season, int episode) {
  int i;
  for (i = 0; i < num_entries; i++)
    if (entries[i].surface == surface && entries[i].profile == profile &&
        entries[i].season == season && entries[i].episode == episode &&
        !strcmp(entries[i].id, id)) return i;
  return -1;
}

static void remove_entry(int i) {
  memmove(&entries[i], &entries[i + 1], sizeof *entries * (size_t)(num_entries - i - 1));
  num_entries--;
}

// Lock must be held. Returns 1 if entry was updated or added.
static int record_entry(char surface, int profile, const char *id, const char *type,
                        int season, int episode, char op, const char *title,
                        const char *poster, long long timestamp_ms) {
  int i = find_entry(surface, profile, id, season, episode);
  Entry *x;
  if (i >= 0) {
    if (entries[i].op == op && !entries[i].confirmed) return 0;
    x = &entries[i];
  } else {
    if (num_entries >= ACCOUNT_PENDING_MAX || !ensure_capacity(num_entries + 1)) {
      int k, oldest = -1;
      for (k = 0; k < num_entries; k++)
        if (entries[k].confirmed && (oldest < 0 || entries[k].confirmed_ms < entries[oldest].confirmed_ms))
          oldest = k;
      if (oldest < 0) {
        printf("[sync] account journal full (%d pending): gesture on %s not saved\n",
               num_entries, id);
        fflush(stdout);
        return 0;
      }
      remove_entry(oldest);
    }
    x = &entries[num_entries++];
    memset(x, 0, sizeof *x);
    x->surface = surface;
    x->profile = profile;
    snprintf(x->id, sizeof x->id, "%s", id);
    x->season = season;
    x->episode = episode;
  }
  x->op = op;
  x->confirmed = 0;
  x->confirmed_ms = 0;
  x->timestamp_ms = timestamp_ms;
  sanitize_string(x->type, sizeof x->type,
                  type && type[0] ? type : (surface == 'V' && episode > 0 ? "series" : "movie"));
  if (strcmp(x->type, "series") && strcmp(x->type, "movie"))
    snprintf(x->type, sizeof x->type, "%s",
             (!strcmp(x->type, "show") || !strcmp(x->type, "tv")) ? "series" : "movie");
  sanitize_string(x->title, sizeof x->title, title);
  sanitize_string(x->poster, sizeof x->poster, poster);
  return 1;
}

// ---------------------------------------------------------------- logging

int account_pending_episodes(const char *imdb, const char *type,
                             const VistoPar *pairs, int n, int watched) {
  char id[24];
  int i, k = 0, profile = perfis_ativo();
  long long now = account_pending_now_ms();
  id_watched(imdb, id, sizeof id);
  if (!id[0] || !pairs || n < 1 || profile < 1) return 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded()) {
    for (i = 0; i < n; i++)
      if (pairs[i].episodio > 0 && pairs[i].temporada >= 0)
        k += record_entry('V', profile, id, type && type[0] ? type : "series",
                          pairs[i].temporada, pairs[i].episodio,
                          watched ? '+' : '-', "", "", now);
    if (k) save_journal();
  }
  pthread_mutex_unlock(&lock);
  return k;
}

int account_pending_title(const char *imdb, const char *type, int watched) {
  char id[24];
  int k = 0, profile = perfis_ativo();
  id_watched(imdb, id, sizeof id);
  if (!id[0] || profile < 1) return 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded()) {
    k = record_entry('V', profile, id, type && !strcmp(type, "series") ? "series" : "movie",
                     0, 0, watched ? '+' : '-', "", "", account_pending_now_ms());
    if (k) save_journal();
  }
  pthread_mutex_unlock(&lock);
  return k;
}

int account_pending_list(const char *imdb, const char *type, const char *name,
                         const char *poster, int saved) {
  char id[24];
  int k = 0, profile = perfis_ativo();
  id_list(imdb, id, sizeof id);
  if (!id[0] || profile < 1) return 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded()) {
    k = record_entry('L', profile, id, type, 0, 0, saved ? '+' : '-',
                     name, poster, account_pending_now_ms());
    if (k) save_journal();
  }
  pthread_mutex_unlock(&lock);
  return k;
}

// ---------------------------------------------------------------- confirmation

static void confirm_batch(const char *user, const Entry *sent, int n) {
  int i, k;
  long long now = account_pending_now_ms();
  pthread_mutex_lock(&lock);
  if (is_loaded && !strcmp(current_user, user)) {
    for (i = 0; i < n; i++) {
      k = find_entry(sent[i].surface, sent[i].profile, sent[i].id, sent[i].season, sent[i].episode);
      if (k >= 0 && entries[k].op == sent[i].op &&
          entries[k].timestamp_ms == sent[i].timestamp_ms && !entries[k].confirmed) {
        entries[k].confirmed = 1;
        entries[k].confirmed_ms = now;
      }
    }
    save_journal();
  }
  pthread_mutex_unlock(&lock);
}

static void supersede_entry(const char *user, const Entry *sent) {
  int k;
  pthread_mutex_lock(&lock);
  if (is_loaded && !strcmp(current_user, user)) {
    k = find_entry(sent->surface, sent->profile, sent->id, sent->season, sent->episode);
    if (k >= 0 && entries[k].op == sent->op && entries[k].timestamp_ms == sent->timestamp_ms) {
      remove_entry(k);
      save_journal();
    }
  }
  pthread_mutex_unlock(&lock);
}

// ---------------------------------------------------------------- watched

static int send_watched_batch(const char *user, int profile, const Entry *sent, int n, char op) {
  Jsw w;
  char *r;
  int i, st = 0, ok;
  if (n < 1) return 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", profile);
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, op == '+' ? "p_items" : "p_keys");
  jsw_arr_ini(&w);
  for (i = 0; i < n; i++) {
    const Entry *e = &sent[i];
    jsw_obj_ini(&w);
    jsw_cs(&w, "content_id", e->id);
    if (op == '+') {
      jsw_cs(&w, "content_type", e->type);
      jsw_cs(&w, "title", "");
      if (e->episode > 0) { jsw_ci(&w, "season", e->season); jsw_ci(&w, "episode", e->episode); }
      else { jsw_chave(&w, "season"); jsw_nulo(&w); jsw_chave(&w, "episode"); jsw_nulo(&w); }
      jsw_ci(&w, "watched_at", e->timestamp_ms);
    } else if (e->episode > 0) {
      jsw_ci(&w, "season", e->season);
      jsw_ci(&w, "episode", e->episode);
    }
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  r = sessao_rpc(op == '+' ? "sync_push_watched_items" : "sync_delete_watched_items",
                  jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = is_2xx(r, st);
  free(r);
  printf("[sync] watched profile %d: %s %d to account -> %s (HTTP %d)\n", profile,
         op == '+' ? "mark" : "unmark", n, ok ? "ok" : "failed", st);
  fflush(stdout);
  if (!ok) return -1;
  confirm_batch(user, sent, n);
  return n;
}

// ---------------------------------------------------------------- library

typedef struct { char *pages[MAX_PAGES]; int num_pages, total_rows; } Pages;

static void free_pages(Pages *p) {
  int i;
  for (i = 0; i < p->num_pages; i++) free(p->pages[i]);
  memset(p, 0, sizeof *p);
}

static int is_raw_array(const char *r) {
  while (r && (*r == ' ' || *r == '\n' || *r == '\r' || *r == '\t')) r++;
  return r && *r == '[';
}

static int fetch_entire_library(int profile, Pages *out) {
  int page;
  memset(out, 0, sizeof *out);
  for (page = 0; page < MAX_PAGES; page++) {
    char body[128];
    const char *p;
    char *r;
    int st = 0, count = 0;
    snprintf(body, sizeof body,
             "{\"p_profile_id\":%d,\"p_limit\":%d,\"p_offset\":%d}",
             profile, PAGE_SIZE, page * PAGE_SIZE);
    r = sessao_rpc("sync_pull_library", body, &st);
    if (!is_2xx(r, st) || !is_raw_array(r)) {
      printf("[sync] list profile %d: page %d failed (HTTP %d); nothing pushed\n",
             profile, page + 1, st);
      fflush(stdout);
      free(r);
      free_pages(out);
      return -1;
    }
    for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) count++;
    out->pages[out->num_pages++] = r;
    out->total_rows += count;
    if (count < PAGE_SIZE) return out->total_rows;
  }
  printf("[sync] list profile %d: over %d rows on account; nothing pushed\n",
         profile, PAGE_SIZE * MAX_PAGES);
  fflush(stdout);
  free_pages(out);
  return -1;
}

static long long parse_timestamp_ms(const char *start, const char *end, const char *key) {
  char t[64];
  double d;
  if (js_texto(start, end, key, t, sizeof t)) {
    if (strchr(t, '-') || strchr(t, 'T') || strchr(t, ' ')) return js_ms_iso(t);
    d = atof(t);
  } else d = js_num(start, end, key, 0.0);
  if (d <= 0.0) return 0;
  return (long long)(d < 1e12 ? d * 1000.0 : d);
}

static void emit_remote_row(Jsw *w, const char *p, const char *f, char *buf, size_t size,
                            long long added_ms) {
  static const struct { const char *key, *default_val; } fields[] = {
    { "content_id", NULL }, { "content_type", "\"movie\"" }, { "name", NULL },
    { "poster", "null" }, { "poster_shape", "\"POSTER\"" }, { "background", "null" },
    { "description", "\"\"" }, { "release_info", "\"\"" }, { "imdb_rating", "null" },
    { "genres", "[]" }, { "addon_base_url", "null" },
  };
  unsigned i;
  jsw_obj_ini(w);
  for (i = 0; i < sizeof fields / sizeof *fields; i++) {
    jsw_chave(w, fields[i].key);
    if (js_bruto(p, f, fields[i].key, buf, size)) { jsw_bruto(w, buf); continue; }
    if (!strcmp(fields[i].key, "name")) {
      if (js_bruto(p, f, "title", buf, size)) jsw_bruto(w, buf);
      else jsw_str(w, "Untitled");
      continue;
    }
    jsw_bruto(w, fields[i].default_val);
  }
  if (js_bruto(p, f, "logo", buf, size)) { jsw_chave(w, "logo"); jsw_bruto(w, buf); }
  jsw_ci(w, "added_at", added_ms > 0 ? added_ms : account_pending_now_ms());
  jsw_obj_fim(w);
}

static void emit_new_item(Jsw *w, const Entry *e) {
  jsw_obj_ini(w);
  jsw_cs(w, "content_id", e->id);
  jsw_cs(w, "content_type", e->type);
  jsw_cs(w, "name", e->title[0] ? e->title : "Untitled");
  if (e->poster[0]) jsw_cs(w, "poster", e->poster); else { jsw_chave(w, "poster"); jsw_nulo(w); }
  jsw_cs(w, "poster_shape", "POSTER");
  jsw_chave(w, "background"); jsw_nulo(w);
  jsw_cs(w, "description", "");
  jsw_cs(w, "release_info", "");
  jsw_chave(w, "imdb_rating"); jsw_nulo(w);
  jsw_chave(w, "genres"); jsw_arr_ini(w); jsw_arr_fim(w);
  jsw_chave(w, "addon_base_url"); jsw_nulo(w);
  jsw_ci(w, "added_at", e->timestamp_ms);
  jsw_obj_fim(w);
}

static int find_op_index(const Entry *ops, int n, const char *id) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(ops[i].id, id)) return i;
  return -1;
}

static int is_in_library(const Pages *pg, const char *id) {
  int i;
  for (i = 0; i < pg->num_pages; i++) {
    const char *p;
    for (p = js_raiz_array(pg->pages[i]); p; p = js_prox(js_fim(p))) {
      char cid[64], b[24];
      if (!js_texto(p, js_fim(p), "content_id", cid, sizeof cid)) continue;
      id_list(cid, b, sizeof b);
      if (!strcmp(b, id)) return 1;
    }
  }
  return 0;
}

static int send_library(const char *user, int profile, Entry *ops, int n) {
  Pages pg;
  Jsw w;
  char *buf = NULL, *r;
  size_t buf_size = 0;
  int i, remote, removed = 0, added = 0, st = 0, ok, output_count;
  char *used;

  remote = fetch_entire_library(profile, &pg);
  if (remote < 0) return -1;

  if (remote == 0) {
    long q = 0;
    char *c = contacache_ler(CC_BIBLIOTECA, profile, user, &q);
    int k = 0;
    const char *p;
    for (p = c ? js_raiz_array(c) : NULL; p; p = js_prox(js_fim(p))) k++;
    free(c);
    if (k > 0) {
      printf("[sync] list profile %d: account returned empty and local copy has %d; nothing pushed\n",
             profile, k);
      fflush(stdout);
      free_pages(&pg);
      return -1;
    }
  }

  used = (char *)calloc((size_t)n + 1, 1);
  if (!used) { free_pages(&pg); return -1; }
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", profile);
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, "p_items");
  jsw_arr_ini(&w);
  output_count = 0;
  for (i = 0; i < pg.num_pages; i++) {
    const char *p;
    for (p = js_raiz_array(pg.pages[i]); p; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      char cid[64], id[24];
      long long added_ts;
      int k;
      size_t line_len = (size_t)(f - p) + 1;
      if (!js_texto(p, f, "content_id", cid, sizeof cid) || !cid[0]) {
        printf("[sync] list profile %d: account row without content_id; nothing pushed\n", profile);
        fflush(stdout);
        jsw_livre(&w); free(buf); free(used); free_pages(&pg);
        return -1;
      }
      if (line_len > buf_size) {
        char *nb = (char *)realloc(buf, line_len);
        if (!nb) { jsw_livre(&w); free(buf); free(used); free_pages(&pg); return -1; }
        buf = nb; buf_size = line_len;
      }
      id_list(cid, id, sizeof id);
      added_ts = parse_timestamp_ms(p, f, "added_at");
      k = find_op_index(ops, n, id);
      if (k >= 0 && ops[k].op == '-') {
        if (added_ts > ops[k].timestamp_ms) {
          used[k] = 2; // re-saved on another device after local removal
        } else { used[k] = 1; removed++; continue; }
      } else if (k >= 0) used[k] = 1;
      emit_remote_row(&w, p, f, buf, buf_size, added_ts);
      output_count++;
    }
  }
  for (i = 0; i < n; i++)
    if (ops[i].op == '+' && !used[i]) {
      emit_new_item(&w, &ops[i]); used[i] = 1; added++; output_count++;
    }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  free(buf);

  for (i = 0; i < n; i++) if (used[i] == 2) supersede_entry(user, &ops[i]);

  if (!removed && !added) {
    jsw_livre(&w);
    for (i = 0; i < n; i++) if (used[i] != 2) confirm_batch(user, &ops[i], 1);
    printf("[sync] list profile %d: account %d, nothing to change\n", profile, remote);
    fflush(stdout);
    free(used); free_pages(&pg);
    return n;
  }
  if (output_count == 0) {
    static int warned;
    jsw_livre(&w);
    if (!warned) {
      warned = 1;
      printf("[sync] list profile %d: removing last title requires empty push; pending until next addition\n",
             profile);
      fflush(stdout);
    }
    free(used); free_pages(&pg);
    return 0;
  }
  r = sessao_rpc("sync_push_library", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = is_2xx(r, st);
  free(r);
  printf("[sync] list profile %d: account %d, +%d -%d -> %d -> %s (HTTP %d)\n",
         profile, remote, added, removed, output_count, ok ? "ok" : "failed", st);
  fflush(stdout);
  free_pages(&pg);
  if (!ok) { free(used); return -1; }

  {
    Pages v;
    if (fetch_entire_library(profile, &v) >= 0) {
      int present = 0, absent = 0;
      for (i = 0; i < n; i++) {
        if (used[i] == 2) continue;
        if (ops[i].op == '+') present += is_in_library(&v, ops[i].id);
        else absent += !is_in_library(&v, ops[i].id);
      }
      printf("[sync] list profile %d: verified %d rows; %d of %d saved items confirmed\n",
             profile, v.total_rows, present, added);
      fflush(stdout);
      free_pages(&v);
    }
  }
  for (i = 0; i < n; i++) if (used[i] != 2) confirm_batch(user, &ops[i], 1);
  free(used);
  return n;
}

// ---------------------------------------------------------------- send

static pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
static int send_now(void);

int account_pending_send(void) {
  int r;
  pthread_mutex_lock(&send_lock);
  r = send_now();
  pthread_mutex_unlock(&send_lock);
  return r;
}

static int send_now(void) {
  Entry *snap;
  char user[80];
  int i, n = 0, failed = 0, total = 0, profiles[16], num_profiles = 0;

  pthread_mutex_lock(&lock);
  if (!ensure_loaded()) { pthread_mutex_unlock(&lock); return 0; }
  for (i = 0; i < num_entries; i++) if (!entries[i].confirmed) n++;
  if (!n) { pthread_mutex_unlock(&lock); return 0; }
  snap = (Entry *)malloc(sizeof *snap * (size_t)n);
  if (!snap) { pthread_mutex_unlock(&lock); return -1; }
  n = 0;
  for (i = 0; i < num_entries; i++) if (!entries[i].confirmed) snap[n++] = entries[i];
  snprintf(user, sizeof user, "%s", current_user);
  pthread_mutex_unlock(&lock);

  for (i = 0; i < n; i++) {
    int k, exists = 0;
    for (k = 0; k < num_profiles; k++) if (profiles[k] == snap[i].profile) exists = 1;
    if (!exists && num_profiles < 16) profiles[num_profiles++] = snap[i].profile;
  }

  {
    Entry *group = (Entry *)malloc(sizeof *group * (size_t)n);
    int p;
    if (!group) { free(snap); return -1; }
    for (p = 0; p < num_profiles; p++) {
      static const char ops[2] = { '+', '-' };
      int o, g;
      for (o = 0; o < 2; o++) {
        g = 0;
        for (i = 0; i < n; i++)
          if (snap[i].surface == 'V' && snap[i].profile == profiles[p] && snap[i].op == ops[o]) {
            group[g++] = snap[i];
            if (g == BATCH_SIZE) {
              int r = send_watched_batch(user, profiles[p], group, g, ops[o]);
              if (r < 0) failed = 1; else total += r;
              g = 0;
            }
          }
        if (g) {
          int r = send_watched_batch(user, profiles[p], group, g, ops[o]);
          if (r < 0) failed = 1; else total += r;
        }
      }
      g = 0;
      for (i = 0; i < n; i++)
        if (snap[i].surface == 'L' && snap[i].profile == profiles[p]) group[g++] = snap[i];
      if (g) {
        int r = send_library(user, profiles[p], group, g);
        if (r < 0) failed = 1; else total += r;
      }
    }
    free(group);
  }
  free(snap);
  if (total > 0) {
    printf("[sync] account journal: %d confirmed, %d pending\n", total,
           account_pending_count());
    fflush(stdout);
  }
  return failed ? -1 : total;
}

static pthread_mutex_t worker_lock = PTHREAD_MUTEX_INITIALIZER;
static int worker_running, rerun_needed;

static void *worker_routine(void *u) {
  (void)u;
  for (;;) {
    account_pending_send();
    pthread_mutex_lock(&worker_lock);
    if (!rerun_needed) { worker_running = 0; pthread_mutex_unlock(&worker_lock); break; }
    rerun_needed = 0;
    pthread_mutex_unlock(&worker_lock);
  }
  return NULL;
}

void account_pending_kick(void) {
  pthread_t t;
  if (threadless) { account_pending_send(); return; }
  pthread_mutex_lock(&worker_lock);
  if (worker_running) { rerun_needed = 1; pthread_mutex_unlock(&worker_lock); return; }
  worker_running = 1;
  pthread_mutex_unlock(&worker_lock);
  if (pthread_create(&t, NULL, worker_routine, NULL) != 0) {
    pthread_mutex_lock(&worker_lock);
    worker_running = 0;
    pthread_mutex_unlock(&worker_lock);
    return;
  }
  pthread_detach(t);
}

// ---------------------------------------------------------------- pull

static int is_hidden(char surface, const char *id, int season, int episode, long long remote_ms) {
  int k, profile = perfis_ativo(), res = 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded()) {
    k = find_entry(surface, profile, id, season, episode);
    if (k >= 0 && entries[k].op == '-') {
      if (remote_ms > entries[k].timestamp_ms) {
        if (!entries[k].confirmed) { remove_entry(k); save_journal(); }
      } else res = 1;
    }
  }
  pthread_mutex_unlock(&lock);
  return res;
}

int account_pending_watched_hidden(const char *imdb, int season, int episode,
                                   long long remote_ms) {
  char id[24];
  id_watched(imdb, id, sizeof id);
  if (!id[0]) return 0;
  if (episode <= 0) season = episode = 0;
  return is_hidden('V', id, season, episode, remote_ms);
}

int account_pending_list_hidden(const char *imdb, long long remote_ms) {
  char id[24];
  id_list(imdb, id, sizeof id);
  if (!id[0]) return 0;
  return is_hidden('L', id, 0, 0, remote_ms);
}

int account_pending_apply_local(void) {
  int i, k = 0, profile = perfis_ativo();
  Entry *copy = NULL;
  int n = 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded() && num_entries) {
    copy = (Entry *)malloc(sizeof *copy * (size_t)num_entries);
    if (copy)
      for (i = 0; i < num_entries; i++)
        if (entries[i].surface == 'V' && entries[i].profile == profile) copy[n++] = entries[i];
  }
  pthread_mutex_unlock(&lock);
  for (i = 0; i < n; i++) {
    if (copy[i].episode > 0)
      vistoep_definir(copy[i].id, copy[i].season, copy[i].episode, copy[i].op == '+');
    else
      cat_historico_definir_id(copy[i].id, copy[i].type, copy[i].op == '+');
    k++;
  }
  free(copy);
  return k;
}

void account_pending_prune(long long since_ms) {
  int i, changed = 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded()) {
    for (i = num_entries - 1; i >= 0; i--)
      if (entries[i].confirmed && entries[i].confirmed_ms < since_ms) {
        remove_entry(i);
        changed = 1;
      }
    if (changed) save_journal();
  }
  pthread_mutex_unlock(&lock);
}

int account_pending_count(void) {
  int i, k = 0;
  pthread_mutex_lock(&lock);
  if (ensure_loaded())
    for (i = 0; i < num_entries; i++) if (!entries[i].confirmed) k++;
  pthread_mutex_unlock(&lock);
  return k;
}

void account_pending_forget(void) {
  pthread_mutex_lock(&lock);
  num_entries = 0;
  is_loaded = 0;
  current_user[0] = 0;
  pthread_mutex_unlock(&lock);
}

// Backward compatibility wrappers
int contapend_episodios(const char *imdb, const char *tipo,
                        const VistoPar *pares, int n, int visto) {
  return account_pending_episodes(imdb, tipo, pares, n, visto);
}
int contapend_titulo(const char *imdb, const char *tipo, int visto) {
  return account_pending_title(imdb, tipo, visto);
}
int contapend_lista(const char *imdb, const char *tipo, const char *nome,
                    const char *poster, int salvo) {
  return account_pending_list(imdb, tipo, nome, poster, salvo);
}
int contapend_enviar(void) {
  return account_pending_send();
}
void contapend_chutar(void) {
  account_pending_kick();
}
int contapend_visto_oculto(const char *imdb, int temporada, int episodio,
                           long long remotoMs) {
  return account_pending_watched_hidden(imdb, temporada, episodio, remotoMs);
}
int contapend_lista_oculta(const char *imdb, long long remotoMs) {
  return account_pending_list_hidden(imdb, remotoMs);
}
int contapend_aplicar_local(void) {
  return account_pending_apply_local();
}
void contapend_podar(long long desdeMs) {
  account_pending_prune(desdeMs);
}
int contapend_pendentes(void) {
  return account_pending_count();
}
void contapend_esquecer(void) {
  account_pending_forget();
}
void contapend_relogio(long long (*f)(void)) {
  account_pending_set_clock(f);
}
void contapend_sem_fio(int sim) {
  account_pending_set_threadless(sim);
}
long long contapend_agora_ms(void) {
  return account_pending_now_ms();
}
