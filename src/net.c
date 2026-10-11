#include "net.h"
#include "negcache.h"
#include "negcache.inc"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

/* Jobs do not depend on TLS or legacy per-thread controls. The group is
 * retained by jobs, and a request retains its job until completion. */
struct NetGroup {
  pthread_mutex_t lock;
  unsigned refs;
  uint64_t generation;
  int cancelled;
};

struct NetJob {
  NetGroup *group;
  unsigned refs;
  uint64_t generation;
  int cancelled; /* protected by group lock */
};

NetGroup *net_group_create(void) {
  NetGroup *g = calloc(1, sizeof *g);
  if (!g) return NULL;
  if (pthread_mutex_init(&g->lock, NULL)) { free(g); return NULL; }
  g->refs = 1; g->generation = 1;
  return g;
}

void net_group_release(NetGroup *g) {
  if (g && __sync_sub_and_fetch(&g->refs, 1) == 0) {
    pthread_mutex_destroy(&g->lock); free(g);
  }
}

uint64_t net_group_advance(NetGroup *g) {
  uint64_t v;
  if (!g) return 0;
  pthread_mutex_lock(&g->lock);
  if (g->generation == UINT64_MAX) g->cancelled = 1;
  else g->generation++;
  v = g->generation;
  pthread_mutex_unlock(&g->lock);
  return v;
}

void net_group_cancel(NetGroup *g) {
  if (!g) return;
  pthread_mutex_lock(&g->lock); g->cancelled = 1;
  pthread_mutex_unlock(&g->lock);
}

NetJob *net_job_create(NetGroup *g) {
  NetJob *j;
  if (!g) return NULL;
  j = calloc(1, sizeof *j);
  if (!j) return NULL;
  __sync_add_and_fetch(&g->refs, 1);
  pthread_mutex_lock(&g->lock);
  j->generation = g->generation; j->cancelled = g->cancelled;
  pthread_mutex_unlock(&g->lock);
  j->group = g; j->refs = 1;
  return j;
}

void net_job_retain(NetJob *j) { if (j) __sync_add_and_fetch(&j->refs, 1); }

void net_job_release(NetJob *j) {
  if (j && __sync_sub_and_fetch(&j->refs, 1) == 0) {
    net_group_release(j->group); free(j);
  }
}

void net_job_cancel(NetJob *j) {
  if (!j) return;
  pthread_mutex_lock(&j->group->lock); j->cancelled = 1;
  pthread_mutex_unlock(&j->group->lock);
}

NetError net_job_state(NetJob *j) {
  NetError e = NET_OK;
  if (!j) return e;
  pthread_mutex_lock(&j->group->lock);
  if (j->cancelled || j->group->cancelled) e = NET_CANCELLED;
  else if (j->generation != j->group->generation) e = NET_GENERATION;
  pthread_mutex_unlock(&j->group->lock);
  return e;
}

static uint64_t net_now_ms(void) {
  struct timespec t;
  if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
  return (uint64_t)t.tv_sec * 1000u + (unsigned long)t.tv_nsec / 1000000u;
}

void net_response_clear(NetResponse *r) {
  if (!r) return;
  free(r->body); free(r->headers); memset(r, 0, sizeof *r);
}


/* Local control of the current request. State is never shared between probes:
 * each thread receives its cap and cancel token. */
static _Thread_local long net_limit_local;
static _Thread_local volatile int *net_cancel_local;
static _Thread_local int net_limited_local;
static _Thread_local int net_cancelled_local;
static _Thread_local long net_bytes_local;

static unsigned long net_now_ms_local(void);

static long net_current_limit(void) {
  if (net_limit_local > 0) return net_limit_local;
  return net_cap > 0 ? net_cap : 0;
}


static _Thread_local int net_curl_local;

static _Thread_local char net_err_txt[200];
static _Thread_local char net_err_buf[256]; /* CURLOPT_ERRORBUFFER (>= 256 bytes) */

static _Thread_local char *net_final_dst;
static _Thread_local unsigned net_final_size;
static _Thread_local int net_partial_ok;

// Hand-written libcurl constants: SDK does not ship curl.h.
#define OPT_URL             10002
#define OPT_WRITEFUNCTION   20011
#define OPT_WRITEDATA       10001
#define OPT_TIMEOUT            13
#define OPT_FOLLOWLOCATION     52
#define OPT_SSL_VERIFYPEER     64
#define OPT_SSL_VERIFYHOST     81
#define OPT_USERAGENT       10018
#define OPT_ACCEPT_ENCODING 10102
#define OPT_NOSIGNAL          99
#define OPT_HTTPHEADER      10023
#define OPT_NOBODY             44
#define OPT_RANGE           10007
#define INFO_URL_FINAL    1048577
#define OPT_POSTFIELDS      10015
#define OPT_POST               47
#define OPT_CUSTOMREQUEST   10036
#define INFO_RESPONSE_CODE   2097154
#define INFO_TEMPO_TOTAL     3145731
#define OPT_HEADERFUNCTION  20079
#define OPT_HEADERDATA      10029
#define OPT_MAXCONNECTS        71
#define OPT_TIMEOUT_MS        155
#define OPT_CONNECTTIMEOUT_MS 156
#define OPT_NOPROGRESS         43
#define OPT_XFERINFOFUNCTION 20219
#define OPT_XFERINFODATA     10057
#define OPT_TCP_KEEPALIVE     213
#define OPT_TCP_KEEPIDLE      214
#define OPT_TCP_KEEPINTVL     215
#define INFO_NUM_CONNECTS  2097178
#define INFO_REDIRECT_URL 1048607
#define INFO_SIZE_DOWNLOAD 3145736
#define OPT_CAINFO 10065
#define OPT_ERRORBUFFER 10010
#define OPT_POSTFIELDSIZE 60
#define OPT_PROTOCOLS 181

static void (*notify_401)(const char *url);
void net_notify_401(void (*f)(const char *url)) { notify_401 = f; }
static void (*notify_health)(int code, const char *url);
void net_notify_health(void (*f)(int code, const char *url)) { notify_health = f; }
static void (*notify_host)(const char *url, int code, int http, unsigned ms);
void net_notify_host(void (*f)(const char *url, int code, int http, unsigned ms)) { notify_host = f; }

static void *(*curl_init)(void);
static int   (*curl_setopt)(void *, int, ...);
static int   (*curl_perform)(void *);
static void  (*curl_cleanup)(void *);
static int   (*curl_global)(long);
static void *(*slist_append)(void *, const char *);
static void  (*slist_free)(void *);
static int   (*curl_getinfo)(void *, int, ...);
static void  (*curl_reset)(void *);
static time_t (*curl_getdate_fn)(const char *, const time_t *);
static int   (*curl_send)(void *, const void *, size_t, size_t *);
static int   (*curl_recv)(void *, void *, size_t, size_t *);
static int    curl_ready;

static int net_retry_seconds(const char *s, size_t n) {
  char b[128], *end;
  unsigned long long v;
  time_t until, now;
  while (n && (*s == ' ' || *s == '\t')) { s++; n--; }
  while (n && isspace((unsigned char)s[n - 1])) n--;
  if (!n || n >= sizeof b) return 0;
  memcpy(b, s, n); b[n] = 0;
  if (isdigit((unsigned char)b[0])) {
    size_t i;
    for (i = 0; i < n; i++) if (!isdigit((unsigned char)b[i])) return 0;
    v = strtoull(b, &end, 10);
    return *end ? 0 : v > INT_MAX ? INT_MAX : (int)v;
  }
  if (!curl_getdate_fn) return 0;
  until = curl_getdate_fn(b, NULL); now = time(NULL);
  if (until == (time_t)-1 || until <= now) return 0;
  return difftime(until, now) > INT_MAX ? INT_MAX : (int)difftime(until, now);
}

// Reused handle per thread via pthread_key.
static pthread_key_t handle_key;
static pthread_once_t handle_once = PTHREAD_ONCE_INIT;
static void handle_release(void *c) { if (c && curl_cleanup) curl_cleanup(c); }
static void handle_create_key(void) { pthread_key_create(&handle_key, handle_release); }
#define NET_CONNECTIONS_PER_THREAD 10L

#define NET_IDLE_DEFAULT_MS 20000UL
static unsigned long idle_max_ms = NET_IDLE_DEFAULT_MS;
#define NET_HOSTS_PER_THREAD 16
typedef struct { char h[96]; unsigned long ms; } HostUsage;
static _Thread_local HostUsage host_usage[NET_HOSTS_PER_THREAD];

static void host_from_url(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  h[0] = 0;
  if (!p || n == 0) return;
  p += 3;
  k = (size_t)(p - u) + strcspn(p, "/?#");
  if (k >= n) k = n - 1;
  memcpy(h, u, k);
  h[k] = 0;
}

static void forget_hosts(void) { memset(host_usage, 0, sizeof host_usage); }

static void record_host_used(const char *url, unsigned long now) {
  char h[96];
  int i, oldest = 0;
  host_from_url(url, h, sizeof h);
  if (!h[0]) return;
  for (i = 0; i < NET_HOSTS_PER_THREAD; i++) {
    if (!strcmp(host_usage[i].h, h)) { host_usage[i].ms = now; return; }
    if (host_usage[i].ms < host_usage[oldest].ms) oldest = i;
  }
  snprintf(host_usage[oldest].h, sizeof host_usage[oldest].h, "%s", h);
  host_usage[oldest].ms = now;
}

static int is_host_idle(const char *url, unsigned long now) {
  char h[96];
  int i;
  if (!idle_max_ms) return 0;
  host_from_url(url, h, sizeof h);
  if (!h[0]) return 0;
  for (i = 0; i < NET_HOSTS_PER_THREAD; i++)
    if (host_usage[i].ms && !strcmp(host_usage[i].h, h))
      return now - host_usage[i].ms > idle_max_ms;
  return 0;
}

#define NET_PARKED_MAX 3
#define NET_PARKED_HOSTS 6
#define NET_PARKED_VALID_MS 15000UL
typedef struct { void *c; unsigned long ms; int nh; char h[NET_PARKED_HOSTS][96]; } ParkedHandle;
static ParkedHandle parked[NET_PARKED_MAX];
static pthread_mutex_t parked_lock = PTHREAD_MUTEX_INITIALIZER;

static void clean_stale_parked(unsigned long now) {
  int i;
  for (i = 0; i < NET_PARKED_MAX; i++)
    if (parked[i].c && now - parked[i].ms > NET_PARKED_VALID_MS) {
      if (curl_cleanup) curl_cleanup(parked[i].c);
      parked[i].c = NULL;
    }
}

static void park_handle(void *c, char h[][96], int nh) {
  unsigned long now = net_now_ms_local();
  int i, free_slot = -1, oldest = 0;
  pthread_mutex_lock(&parked_lock);
  clean_stale_parked(now);
  for (i = 0; i < NET_PARKED_MAX; i++) {
    if (!parked[i].c) { free_slot = i; break; }
    if (parked[i].ms < parked[oldest].ms) oldest = i;
  }
  if (free_slot < 0) { free_slot = oldest; if (curl_cleanup) curl_cleanup(parked[free_slot].c); }
  parked[free_slot].c = c; parked[free_slot].ms = now; parked[free_slot].nh = nh;
  for (i = 0; i < nh && i < NET_PARKED_HOSTS; i++) snprintf(parked[free_slot].h[i], 96, "%.95s", h[i]);
  pthread_mutex_unlock(&parked_lock);
}

static void *take_parked_handle(const char *url) {
  char h[96];
  unsigned long now = net_now_ms_local();
  void *c = NULL;
  int i, k;
  host_from_url(url, h, sizeof h);
  if (!h[0]) return NULL;
  pthread_mutex_lock(&parked_lock);
  clean_stale_parked(now);
  for (i = 0; i < NET_PARKED_MAX && !c; i++)
    for (k = 0; parked[i].c && k < parked[i].nh; k++)
      if (!strcmp(parked[i].h[k], h)) { c = parked[i].c; parked[i].c = NULL; break; }
  pthread_mutex_unlock(&parked_lock);
  return c;
}

static void *acquire_handle(const char *url) {
  void *c;
  if (!curl_reset) return curl_init();
  pthread_once(&handle_once, handle_create_key);
  c = pthread_getspecific(handle_key);
  if (!c) {
    c = take_parked_handle(url);
    if (c) pthread_setspecific(handle_key, c);
  }
  if (c && is_host_idle(url, net_now_ms_local())) {
    curl_cleanup(c);
    pthread_setspecific(handle_key, NULL);
    forget_hosts();
    c = NULL;
  }
  if (c) curl_reset(c);
  else {
    c = curl_init();
    if (c) pthread_setspecific(handle_key, c);
  }
  if (c) curl_setopt(c, OPT_MAXCONNECTS, NET_CONNECTIONS_PER_THREAD);
  return c;
}

static void release_handle_r(void *c, int r, const char *url) {
  if (notify_health) notify_health(r, url);
  if (notify_host) {
    long http = 0;
    double sec = 0;
    if (curl_getinfo) { curl_getinfo(c, INFO_RESPONSE_CODE, &http); curl_getinfo(c, INFO_TEMPO_TOTAL, &sec); }
    notify_host(url, r, (int)http, sec > 0 ? (unsigned)(sec * 1000.0 + 0.5) : 0u);
  }
  if (!curl_reset) { curl_cleanup(c); return; }
  if (r != 0) {
    curl_cleanup(c);
    pthread_setspecific(handle_key, NULL);
    forget_hosts();
    return;
  }
  { unsigned long now = net_now_ms_local();
    char *end_url = NULL;
    record_host_used(url, now);
    if (curl_getinfo && !curl_getinfo(c, INFO_URL_FINAL, &end_url) && end_url) record_host_used(end_url, now); }
}

static unsigned long connect_timeout_ms(unsigned long timeout_ms) {
  unsigned long m = timeout_ms / 2;
  if (m < 2000) m = 2000;
  if (m > 5000) m = 5000;
  return m < timeout_ms ? m : timeout_ms;
}

static void common_options(void *c, unsigned long timeout_ms) {
  curl_setopt(c, OPT_TIMEOUT_MS, (long)timeout_ms);
  curl_setopt(c, OPT_CONNECTTIMEOUT_MS, (long)connect_timeout_ms(timeout_ms));
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  curl_setopt(c, OPT_TCP_KEEPALIVE, (long)1);
  curl_setopt(c, OPT_TCP_KEEPIDLE, (long)15);
  curl_setopt(c, OPT_TCP_KEEPINTVL, (long)5);
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
  curl_setopt(c, OPT_USERAGENT, "Nuvio/1.0 (webOS)");
  net_err_buf[0] = 0;
  net_err_txt[0] = 0;
  curl_setopt(c, OPT_ERRORBUFFER, net_err_buf);
}

static void record_error(int r) {
  if (!r) { net_err_txt[0] = 0; return; }
  snprintf(net_err_txt, sizeof net_err_txt, "curl %d%s%.150s", r,
           net_err_buf[0] ? ": " : "", net_err_buf);
}
const char *net_last_error(void) { return net_err_txt; }

typedef struct {
  unsigned long last_ms, stall_ms;
  long long seen;
  int stalled;
} Watcher;

static int watchdog_cb(void *u, long long dl_total, long long dl_now,
                       long long ul_total, long long ul_now) {
  Watcher *v = (Watcher *)u;
  unsigned long now = net_now_ms_local();
  (void)dl_total; (void)ul_total; (void)ul_now;
  if (net_cancel_local && *net_cancel_local) { net_cancelled_local = 1; return 1; }
  if (dl_now > v->seen) { v->seen = dl_now; v->last_ms = now; return 0; }
  if (v->seen > 0 && v->stall_ms && now - v->last_ms > v->stall_ms) {
    v->stalled = 1;
    return 1;
  }
  return 0;
}

static unsigned long stall_ms_for(unsigned long timeout_ms) {
  unsigned long m = timeout_ms / 3;
  if (m < 2000) m = 2000;
  if (m > 8000) m = 8000;
  return m;
}

static void attach_watchdog(void *c, Watcher *v, unsigned long timeout_ms) {
  memset(v, 0, sizeof *v);
  v->stall_ms = stall_ms_for(timeout_ms);
  v->last_ms = net_now_ms_local();
  curl_setopt(c, OPT_XFERINFOFUNCTION, watchdog_cb);
  curl_setopt(c, OPT_XFERINFODATA, v);
  curl_setopt(c, OPT_NOPROGRESS, (long)0);
}

typedef struct { char *p; size_t n, cap; } Bucket;

static char *bucket_finish(Bucket *b) {
  if (b->p && b->cap > b->n + 1) {
    char *p = realloc(b->p, b->n + 1);
    if (p) { b->p = p; b->cap = b->n + 1; }
  }
  return b->p;
}

static char *net_download_internal(const char *url, int seconds, long *size,
                                   const char *const *headers);
static char *net_download_internal2(const char *url, int seconds, long *size,
                                    const char *const *headers, int *status);
static char *net_download_internal3(const char *url, int seconds, long *size,
                                    const char *const *headers, int *status, int *retry);

static size_t receive_header(void *data, size_t size, size_t count, void *u) {
  int *retry = (int *)u;
  const char *s = (const char *)data;
  size_t bytes = size * count;
  if (retry && bytes >= 5 && !strncasecmp(s, "HTTP/", 5)) *retry = 0;
  if (retry && bytes > 12 && !strncasecmp(s, "retry-after:", 12))
    *retry = net_retry_seconds(s + 12, bytes - 12);
  return bytes;
}


_Thread_local long net_cap = 0;

static size_t receive_body(void *data, size_t size, size_t count, void *u) {
  Bucket *b = (Bucket *)u;
  size_t bytes, needed;
  long limit = net_current_limit();
  if (size && count > SIZE_MAX / size) return 0;
  bytes = size * count;
  if (!bytes) return 0;
  if (net_cancel_local && *net_cancel_local) {
    net_cancelled_local = 1;
    return 0;
  }
  if (limit > 0 && b->n >= (size_t)limit) {
    net_limited_local = 1;
    return 0;
  }
  if (limit > 0 && bytes > (size_t)limit - b->n) {
    bytes = (size_t)limit - b->n;
    net_limited_local = 1;
  }
  if (bytes > (size_t)LONG_MAX || b->n > (size_t)LONG_MAX - bytes ||
      b->n + bytes == SIZE_MAX) return 0;
  needed = b->n + bytes + 1;
  if (needed > b->cap) {
    size_t cap = b->cap ? b->cap : 4096;
    char *new_buf;
    while (cap < needed) {
      if (cap > SIZE_MAX / 2) { cap = needed; break; }
      cap *= 2;
    }
    if (limit > 0 && cap > (size_t)limit + 1) cap = (size_t)limit + 1;
    new_buf = realloc(b->p, cap);
    if (!new_buf) return 0;
    b->p = new_buf; b->cap = cap;
  }
  memcpy(b->p + b->n, data, bytes);
  b->n += bytes;
  b->p[b->n] = 0;
  return bytes;
}

static pthread_mutex_t open_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *ssl_locks;
static int n_ssl_locks;

static void ssl_lock_cb(int mode, int n, const char *file, int line) {
  (void)file; (void)line;
  if (!ssl_locks || n < 0 || n >= n_ssl_locks) return;
  if (mode & 1)  pthread_mutex_lock(&ssl_locks[n]);
  else           pthread_mutex_unlock(&ssl_locks[n]);
}

static unsigned long ssl_thread_id(void) {
  return (unsigned long)(uintptr_t)pthread_self();
}

static void prepare_openssl(void) {
  void *hc;
  int (*num_locks)(void);
  void (*set_locking)(void (*)(int, int, const char *, int));
  void (*set_id)(unsigned long (*)(void));
  int i;
  static const char *names[] = { "libcrypto.so.1.0.0", "libcrypto.so.1.0.2",
                                 "libcrypto.so.10", "libcrypto.so.1",
                                 "libcrypto.so", NULL };
  if (ssl_locks) return;
  hc = NULL;
  for (i = 0; names[i] && !hc; i++) hc = dlopen(names[i], RTLD_NOW);
  if (!hc) return;
  *(void **)(&num_locks)   = dlsym(hc, "CRYPTO_num_locks");
  *(void **)(&set_locking) = dlsym(hc, "CRYPTO_set_locking_callback");
  *(void **)(&set_id)      = dlsym(hc, "CRYPTO_set_id_callback");
  if (!num_locks || !set_locking) return;
  n_ssl_locks = num_locks();
  if (n_ssl_locks < 1) return;
  ssl_locks = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * (size_t)n_ssl_locks);
  if (!ssl_locks) { n_ssl_locks = 0; return; }
  for (i = 0; i < n_ssl_locks; i++) pthread_mutex_init(&ssl_locks[i], NULL);
  set_locking(ssl_lock_cb);
  if (set_id) set_id(ssl_thread_id);
  printf("[rede] OpenSSL 1.0 travado para %d regioes\n", n_ssl_locks);
  fflush(stdout);
}

static int net_curl_open(void) {
  void *h;
  int r = __atomic_load_n(&curl_ready, __ATOMIC_ACQUIRE);
  if (r == 1 || r == -1) return r > 0;
  pthread_mutex_lock(&open_lock);
  r = __atomic_load_n(&curl_ready, __ATOMIC_ACQUIRE);
  if (r == 1 || r == -1) { pthread_mutex_unlock(&open_lock); return r > 0; }
  __atomic_store_n(&curl_ready, -2, __ATOMIC_RELEASE);
  h = dlopen("libcurl.so.5", RTLD_NOW);
  if (!h) h = dlopen("libcurl.so.4", RTLD_NOW);
  if (!h) {
    printf("[rede] sem libcurl: %s\n", dlerror());
    __atomic_store_n(&curl_ready, -1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&open_lock);
    return 0;
  }
  *(void **)(&curl_init)    = dlsym(h, "curl_easy_init");
  *(void **)(&curl_setopt)  = dlsym(h, "curl_easy_setopt");
  *(void **)(&curl_perform) = dlsym(h, "curl_easy_perform");
  *(void **)(&curl_cleanup) = dlsym(h, "curl_easy_cleanup");
  *(void **)(&curl_global)  = dlsym(h, "curl_global_init");
  *(void **)(&slist_append) = dlsym(h, "curl_slist_append");
  *(void **)(&slist_free)   = dlsym(h, "curl_slist_free_all");
  *(void **)(&curl_getinfo) = dlsym(h, "curl_easy_getinfo");
  *(void **)(&curl_getdate_fn) = dlsym(h, "curl_getdate");
  *(void **)(&curl_send)    = dlsym(h, "curl_easy_send");
  *(void **)(&curl_recv)    = dlsym(h, "curl_easy_recv");
  { const char *reuse_env = getenv("NUVIO_REDE_REUSO");
    if (!(reuse_env && reuse_env[0] == '0')) *(void **)(&curl_reset) = dlsym(h, "curl_easy_reset"); }
  { const char *idle_env = getenv("NUVIO_REDE_OCIOSO_MS");
    if (idle_env && *idle_env) idle_max_ms = strtoul(idle_env, NULL, 10); }
  if (!curl_init || !curl_setopt || !curl_perform) {
    printf("[rede] libcurl sem os simbolos esperados\n");
    __atomic_store_n(&curl_ready, -1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&open_lock);
    return 0;
  }
  if (curl_global) curl_global(3 /* CURL_GLOBAL_DEFAULT */);
  prepare_openssl();
  __atomic_store_n(&curl_ready, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&open_lock);
  return 1;
}

void net_prepare(void) { net_curl_open(); }

#include "net_request.inc"


char *net_download_bin(const char *url, int seconds, long *size) {
  return net_download_internal(url, seconds, size, NULL);
}

char *net_download(const char *url, int seconds) {
  return net_download_internal(url, seconds, NULL, NULL);
}

static char *chunk_once(const char *url, int seconds, long long start, long long end,
                        long *size, int *status, int *error,
                        char *final, unsigned final_size) {
  char range_hdr[80];
  const char *hdrs[2];
  char *r;
  int st = 0;
  if (final && final_size) final[0] = 0;
  snprintf(range_hdr, sizeof range_hdr, "Range: bytes=%lld-%lld", start, end);
  hdrs[0] = range_hdr; hdrs[1] = NULL;
  net_curl_local = 0;
  net_cap = (long)(end - start + 1);
  net_final_dst = final; net_final_size = final ? final_size : 0;
  net_partial_ok = 1;
  r = net_download_internal2(url, seconds, size, hdrs, &st);
  net_partial_ok = 0;
  net_final_dst = NULL; net_final_size = 0;
  net_cap = 0;
  if (status) *status = st;
  if (error) *error = net_curl_local;
  if (r && (st < 200 || st >= 300)) {
    char safe[120];
    printf("[rede] HTTP %d em %s\n", st, net_url_public(url, safe, sizeof safe));
    fflush(stdout);
    free(r); r = NULL;
    if (size) *size = 0;
  }
  return r;
}

char *net_download_with(const char *url, int seconds, const char *const *headers) {
  return net_download_internal(url, seconds, NULL, headers);
}

char *net_download_st(const char *url, int seconds, const char *const *headers,
                      int *status) {
  return net_download_internal2(url, seconds, NULL, headers, status);
}

char *net_download_st_retry(const char *url, int seconds, const char *const *headers,
                            int *status, int *retry_after) {
  return net_download_internal3(url, seconds, NULL, headers, status, retry_after);
}


static char *net_download_internal(const char *url, int seconds, long *size,
                                   const char *const *headers) {
  return net_download_internal2(url, seconds, size, headers, NULL);
}

static int should_retry(int r, const Watcher *v, size_t bytes) {
  if (net_cancelled_local) return 0;
  if (r == 42) return v->stalled;
  if (bytes > 0) return 0;
  return r == 7 || r == 28 || r == 35 || r == 52 || r == 55 || r == 56;
}

static char *net_download_internal2(const char *url, int seconds, long *size,
                                    const char *const *headers, int *status) {
  return net_download_internal3(url, seconds, size, headers, status, NULL);
}

static char *net_download_internal3(const char *url, int seconds, long *size,
                                    const char *const *headers, int *status, int *retry) {
  Bucket b = {0};
  Watcher watcher;
  void *c, *list = NULL;
  int r = 0, attempt, reused = 0;
  unsigned long start, timeout_ms, elapsed = 0;
  if (retry) *retry = 0;
  if (status) *status = 0;
  if (!url || !*url) return NULL;
  if (!status && negcache_barra(url, (long)time(NULL))) return NULL;
  if (!net_curl_open()) return NULL;
  start = net_now_ms_local();
  timeout_ms = (unsigned long)(seconds > 0 ? seconds : 30) * 1000UL;
  for (attempt = 0; ; attempt++) {
    unsigned long remaining = timeout_ms - elapsed;
    c = acquire_handle(url);
    if (!c) return NULL;
    curl_setopt(c, OPT_URL, url);
    if (retry) {
      curl_setopt(c, OPT_HEADERFUNCTION, receive_header);
      curl_setopt(c, OPT_HEADERDATA, retry);
    }
    curl_setopt(c, OPT_WRITEFUNCTION, receive_body);
    curl_setopt(c, OPT_WRITEDATA, &b);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
    common_options(c, remaining);
    attach_watchdog(c, &watcher, timeout_ms);
    curl_setopt(c, OPT_ACCEPT_ENCODING, "");
    if (headers && slist_append) {
      int k;
      for (k = 0; headers[k]; k++) list = slist_append(list, headers[k]);
      if (list) curl_setopt(c, OPT_HTTPHEADER, list);
    }
    r = curl_perform(c);
    { long new_conn = -1;
      if (curl_getinfo) curl_getinfo(c, INFO_NUM_CONNECTS, &new_conn);
      reused = new_conn == 0; }
    elapsed = net_now_ms_local() - start;
    if (attempt == 0 && r != 0 && should_retry(r, &watcher, b.n) &&
        elapsed + 1000 < timeout_ms) {
      char safe[120];
      printf("[rede] curl %d em %s (conexao %s, %ld bytes, %lu ms): de novo em conexao nova\n",
             r, net_url_public(url, safe, sizeof safe), reused ? "reusada" : "nova",
             (long)b.n, elapsed);
      fflush(stdout);
      if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(list); list = NULL; }
      release_handle_r(c, r, url);
      free(b.p);
      b.p = NULL; b.n = b.cap = 0;
      continue;
    }
    break;
  }
  if (r == 42 && watcher.stalled) r = 28;
  net_curl_local = r;
  record_error(r);
  if (net_final_dst && net_final_size && curl_getinfo) {
    char *end_url = NULL;
    curl_getinfo(c, INFO_URL_FINAL, &end_url);
    snprintf(net_final_dst, net_final_size, "%s", end_url ? end_url : "");
  }
  { long http = 0;
    if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
    if (status) *status = (int)http;
    if (http == 401 && notify_401) notify_401(url);
    negcache_nota(url, r, (int)http, (long)time(NULL));
    if (!r && http >= 400 && !status) {
      if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(list); }
      release_handle_r(c, r, url);
      free(b.p);
      { char safe[120];
        printf("[rede] HTTP %ld em %s\n", http, net_url_public(url, safe, sizeof safe)); }
      fflush(stdout);
      return NULL;
    } }
  if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(list); }
  release_handle_r(c, r, url);
  if (net_cancelled_local) {
    free(b.p);
    return NULL;
  }
  if (r == 23 && net_current_limit() > 0 && b.n > 0) r = 0;
  if ((r == 18 || r == 56) && net_partial_ok && b.n > 0 && status && *status == 206) {
    char safe[120];
    printf("[rede] corte %d em %s (conexao %s, %ld bytes, %lu ms): fica o que veio\n", r,
           net_url_public(url, safe, sizeof safe), reused ? "reusada" : "nova",
           (long)b.n, elapsed);
    fflush(stdout);
    net_bytes_local = (long)b.n;
    if (size) *size = (long)b.n;
    return bucket_finish(&b);
  }
  if (r != 0) {
    char safe[120]; free(b.p);
    printf("[rede] falha %d em %s (conexao %s, %ld bytes, %lu ms%s)%s%s\n", r,
           net_url_public(url, safe, sizeof safe), reused ? "reusada" : "nova",
           (long)b.n, elapsed, attempt ? ", 2a tentativa" : "",
           net_err_buf[0] ? ": " : "", net_err_buf);
    fflush(stdout);
    return NULL;
  }
  net_bytes_local = (long)b.n;
  if (size) *size = (long)b.n;
  return bucket_finish(&b);
}

typedef struct { size_t n; int limited; } UrlProbe;
#define INFO_CONTENT_TYPE 1048594

static size_t receive_probe(void *data, size_t size, size_t count, void *u) {
  UrlProbe *s = u;
  size_t bytes;
  (void)data;
  if (size && count > SIZE_MAX / size) return 0;
  if (net_cancel_local && *net_cancel_local) { net_cancelled_local = 1; return 0; }
  bytes = size * count;
  if (bytes > 64 - s->n) { s->n = 64; s->limited = 1; return 0; }
  s->n += bytes;
  return bytes;
}

int net_url_final_headers(const char *url, int seconds, const char *const *headers,
                          char *dst, unsigned size, int *status) {
  return net_url_final_type(url, seconds, headers, dst, size, status, NULL, 0, NULL);
}

int net_url_final_type(const char *url, int seconds, const char *const *headers,
                       char *dst, unsigned size, int *status,
                       char *mime, unsigned mime_size, long *body) {
  UrlProbe s = {0};
  Watcher watcher;
  void *c, *list = NULL;
  char *final_url = NULL;
  long http = 0;
  unsigned long timeout_ms;
  int r, ok, k;
  if (status) *status = 0;
  if (mime && mime_size) mime[0] = 0;
  if (body) *body = -1;
  if (dst && size) dst[0] = 0;
  if (!url || !*url || !dst || !size || size > INT_MAX || !net_curl_open() || !curl_getinfo) return 0;
  if (headers && headers[0] && (!slist_append || !slist_free)) return 0;
  c = acquire_handle(url);
  if (!c) return 0;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receive_probe);
  curl_setopt(c, OPT_WRITEDATA, &s);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  timeout_ms = (unsigned long)(seconds > 0 ? seconds : 20) * 1000UL;
  common_options(c, timeout_ms);
  attach_watchdog(c, &watcher, timeout_ms);
  curl_setopt(c, OPT_RANGE, "0-63");
  for (k = 0; headers && headers[k]; k++) {
    void *new_item = slist_append(list, headers[k]);
    if (!new_item) { if (list) slist_free(list); release_handle_r(c, 1, url); return 0; }
    list = new_item;
  }
  if (list) curl_setopt(c, OPT_HTTPHEADER, list);
  r = curl_perform(c);
  curl_getinfo(c, INFO_RESPONSE_CODE, &http);
  curl_getinfo(c, INFO_URL_FINAL, &final_url);
  if (status) *status = (int)http;
  if (mime && mime_size) {
    char *ct = NULL, *pv;
    unsigned k2 = 0;
    curl_getinfo(c, INFO_CONTENT_TYPE, &ct);
    for (pv = ct; pv && *pv && *pv != ';' && k2 + 1 < mime_size; pv++)
      if (*pv != ' ' && *pv != '\t') mime[k2++] = (char)tolower((unsigned char)*pv);
    mime[k2] = 0;
  }
  if (body && http == 200 && !s.limited && !r) *body = (long)s.n;
  ok = (!r || (r == 23 && s.limited)) && !net_cancelled_local &&
       http >= 200 && http < 300 && final_url && *final_url && strlen(final_url) < size;
  if (ok) memcpy(dst, final_url, strlen(final_url) + 1);
  if (!ok) {
    char safe[120];
    printf("[rede] url final falhou: curl %d, HTTP %ld em %s\n", r, http,
           net_url_public(url, safe, sizeof safe));
    fflush(stdout);
  }
  if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); slist_free(list); }
  release_handle_r(c, r, url);
  return ok;
}

int net_url_final(const char *url, int seconds, char *dst, unsigned size) {
  return net_url_final_headers(url, seconds, NULL, dst, size, NULL);
}

static size_t discard_body(void *d, size_t t, size_t q, void *u) { (void)d; (void)u; return t * q; }

int rede_aquecer_lote(const char *const *origens, int n, unsigned *ms) {
  char ok[NET_PARKED_HOSTS][96];
  int i, opened = 0, n_ok = 0;
  void *c;
  if (!origens || n < 1 || !net_curl_open() || !curl_reset || !curl_getinfo) return 0;
  for (i = 0; i < n; i++) {
    char u[200], h[96];
    unsigned long t0;
    int r;
    if (ms) ms[i] = 0;
    host_from_url(origens[i], h, sizeof h);
    if (!h[0] || strlen(h) + 2 > sizeof u) continue;
    snprintf(u, sizeof u, "%s/", h);
    c = acquire_handle(u);
    if (!c) continue;
    curl_setopt(c, OPT_URL, u);
    curl_setopt(c, OPT_NOBODY, (long)1);
    curl_setopt(c, OPT_WRITEFUNCTION, discard_body);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
    common_options(c, 4000UL);
    t0 = net_now_ms_local();
    r = curl_perform(c);
    if (ms) { unsigned long dt = net_now_ms_local() - t0; ms[i] = r ? 0 : (unsigned)(dt ? dt : 1); }
    release_handle_r(c, r, u);
    if (!r) {
      opened++;
      if (n_ok < NET_PARKED_HOSTS) snprintf(ok[n_ok++], 96, "%s", h);
    }
  }
  c = pthread_getspecific(handle_key);
  if (c && n_ok > 0) {
    pthread_setspecific(handle_key, NULL);
    forget_hosts();
    park_handle(c, ok, n_ok);
  }
  return opened;
}


char *net_post(const char *url, int seconds, const char *const *headers,
               const char *body) {
  return net_post_st(url, seconds, headers, body, NULL);
}

char *net_delete(const char *url, int seconds, const char *const *headers,
                 int *status) {
  Bucket b = {0};
  void *c, *list = NULL;
  int r;
  if (status) *status = 0;
  if (!url || !*url || !net_curl_open()) return NULL;
  c = acquire_handle(url);
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receive_body);
  curl_setopt(c, OPT_WRITEDATA, &b);
  common_options(c, (unsigned long)(seconds > 0 ? seconds : 20) * 1000UL);
  curl_setopt(c, OPT_CUSTOMREQUEST, "DELETE");
  if (slist_append) {
    int k;
    for (k = 0; headers && headers[k]; k++) list = slist_append(list, headers[k]);
    if (list) curl_setopt(c, OPT_HTTPHEADER, list);
  }
  r = curl_perform(c);
  { long h = 0;
    if (!r && curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &h);
    if (status) *status = (int)h;
    if (h == 401 && notify_401) notify_401(url); }
  if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(list); }
  release_handle_r(c, r, url);
  if (r != 0) { free(b.p); return NULL; }
  if (!b.p) return strdup("");
  return bucket_finish(&b);
}

static char *post_native(const char *url, int seconds, const char *const *headers,
                         const char *body, int *status, int secure) {
  Bucket b = {0};
  void *c, *list = NULL;
  int r;
  if (status) *status = 0;
  if (!url || !*url || !net_curl_open()) return NULL;
  c = secure ? curl_init() : acquire_handle(url);
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receive_body);
  curl_setopt(c, OPT_WRITEDATA, &b);
  common_options(c, (unsigned long)(seconds > 0 ? seconds : 20) * 1000UL);
  if (secure) {
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
  }
  curl_setopt(c, OPT_POST, (long)1);
  curl_setopt(c, OPT_POSTFIELDS, body ? body : "");
  if (slist_append) {
    int k;
    int has_ct = 0;
    for (k = 0; headers && headers[k]; k++) if (!strncasecmp(headers[k], "Content-Type:", 13)) has_ct = 1;
    if (!has_ct) list = slist_append(list, "Content-Type: application/json");
    for (k = 0; headers && headers[k]; k++) list = slist_append(list, headers[k]);
    if (list) curl_setopt(c, OPT_HTTPHEADER, list);
  }
  r = curl_perform(c);
  record_error(r);
  if (r != 0) {
    char safe[120];
    printf("[rede] POST falhou em %s: %s\n", net_url_public(url, safe, sizeof safe), net_err_txt);
    fflush(stdout);
  }
  { long code = 0;
    if (!r && curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &code);
    if (status) *status = (int)code;
    if (code == 401 && notify_401) notify_401(url); }
  if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(list); }
  if (secure) curl_cleanup(c); else release_handle_r(c, r, url);
  if (r != 0) { free(b.p); return NULL; }
  return b.p ? bucket_finish(&b) : strdup("");
}

char *net_post_st(const char *url, int seconds, const char *const *headers,
                  const char *body, int *status) {
  return post_native(url, seconds, headers, body, status, 0);
}



#define VAZ_SEG_BALDES 64
#define VAZ_ESPERA_PADRAO_MS 8000UL
static unsigned long throughput_wait_ms = VAZ_ESPERA_PADRAO_MS;
void net_throughput_wait(unsigned long ms) { throughput_wait_ms = ms ? ms : VAZ_ESPERA_PADRAO_MS; }

typedef struct {
  unsigned long req_time, t0, window_ms, wait_time, last_time;
  long long bytes, max_bytes;
  long long buckets[VAZ_SEG_BALDES];
  int status, by_window, by_cap, cancelled, by_wait;
  volatile int *cancel;
} ThroughputCounter;

static size_t counter_header(void *data, size_t size, size_t count, void *u) {
  ThroughputCounter *c = (ThroughputCounter *)u;
  const char *s = (const char *)data;
  size_t b = size * count;
  if (b > 9 && !strncmp(s, "HTTP/", 5)) {
    const char *p = memchr(s, ' ', b);
    if (p) c->status = atoi(p + 1);
  }
  return b;
}

static size_t counter_body(void *data, size_t size, size_t count, void *u) {
  ThroughputCounter *c = (ThroughputCounter *)u;
  size_t b = size * count;
  unsigned long now = net_now_ms_local(), elapsed;
  (void)data;
  if (c->status >= 400) return 0;
  if (c->cancel && *c->cancel) { c->cancelled = 1; return 0; }
  if (!c->t0) c->t0 = now;
  elapsed = now - c->t0;
  if (elapsed >= c->window_ms) { c->by_window = 1; return 0; }
  if (elapsed / 1000UL < VAZ_SEG_BALDES) c->buckets[elapsed / 1000UL] += (long long)b;
  c->bytes += (long long)b;
  c->last_time = now;
  if (c->max_bytes > 0 && c->bytes >= c->max_bytes) { c->by_cap = 1; return 0; }
  return b;
}

static int counter_watchdog(void *u, long long dt, long long dn, long long ut, long long un) {
  ThroughputCounter *c = (ThroughputCounter *)u;
  (void)dt; (void)dn; (void)ut; (void)un;
  if (c->cancel && *c->cancel) { c->cancelled = 1; return 1; }
  if (c->t0 && net_now_ms_local() - c->t0 >= c->window_ms) { c->by_window = 1; return 1; }
  if (!c->t0 && c->wait_time && net_now_ms_local() - c->req_time >= c->wait_time) { c->by_wait = 1; return 1; }
  return 0;
}

int net_measure_throughput(const char *url, const char *const *headers, int seconds,
                           long start, long long max_bytes, volatile int *cancelled,
                           int *kbps, int n_max, NetThroughput *res,
                           char *final, unsigned final_size) {
  static const ThroughputCounter empty_counter;
  ThroughputCounter ct = empty_counter;
  char range_buf[40];
  void *c, *list = NULL;
  int r, k, n_sec = 0;
  unsigned long elapsed = 0;
  if (res) memset(res, 0, sizeof *res);
  if (final && final_size) final[0] = 0;
  if (!url || !*url || !net_curl_open()) { if (res) res->error = 2; return 0; }
  if (seconds < 1) seconds = 1;
  ct.window_ms = (unsigned long)(seconds * 1000UL);
  ct.max_bytes = max_bytes;
  ct.cancel = cancelled;
  ct.req_time = net_now_ms_local();
  ct.wait_time = throughput_wait_ms;
  c = acquire_handle(url);
  if (!c) { if (res) res->error = 2; return 0; }
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, counter_body);
  curl_setopt(c, OPT_WRITEDATA, &ct);
  curl_setopt(c, OPT_HEADERFUNCTION, counter_header);
  curl_setopt(c, OPT_HEADERDATA, &ct);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  common_options(c, ct.window_ms + throughput_wait_ms);
  curl_setopt(c, OPT_XFERINFOFUNCTION, counter_watchdog);
  curl_setopt(c, OPT_XFERINFODATA, &ct);
  curl_setopt(c, OPT_NOPROGRESS, (long)0);
  if (start > 0) {
    snprintf(range_buf, sizeof range_buf, "%ld-", start);
    curl_setopt(c, OPT_RANGE, range_buf);
  }
  if (headers && slist_append) {
    for (k = 0; headers[k]; k++) list = slist_append(list, headers[k]);
    if (list) curl_setopt(c, OPT_HTTPHEADER, list);
  }
  r = curl_perform(c);
  { long http = 0;
    if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
    if (http > 0) ct.status = (int)http; }
  if (final && final_size && curl_getinfo) {
    char *end_url = NULL;
    curl_getinfo(c, INFO_URL_FINAL, &end_url);
    snprintf(final, final_size, "%s", end_url ? end_url : "");
  }
  if (list) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(list); }
  release_handle_r(c, r, url);
  if (ct.by_window || ct.by_cap || ((r == 28 || r == 18) && ct.bytes > 0)) r = 0;
  if (ct.t0) elapsed = ct.by_window ? ct.window_ms : (ct.last_time ? ct.last_time : ct.t0) - ct.t0;
  if (kbps && n_max > 0 && ct.bytes > 0 && !ct.cancelled && ct.status < 400 && r == 0) {
    n_sec = (int)(elapsed / 1000UL);
    if (n_sec > n_max) n_sec = n_max;
    if (n_sec > VAZ_SEG_BALDES) n_sec = VAZ_SEG_BALDES;
    for (k = 0; k < n_sec; k++) kbps[k] = (int)(ct.buckets[k] * 8 / 1000);
    if (n_sec == 0) {
      kbps[0] = (int)(ct.bytes * 8 / (long long)(elapsed > 0 ? elapsed : 1));
      n_sec = 1;
    }
  }
  if (res) {
    res->status = ct.status;
    res->error = ct.cancelled ? 0 : r;
    res->bytes = ct.bytes;
    res->ms = elapsed;
    res->wait_ms = ct.t0 ? ct.t0 - ct.req_time : 0;
    res->cancelled = ct.cancelled;
  }
  return n_sec;
}

#define OPT_CONNECT_ONLY      141
#define OPT_HTTP_VERSION       84
#define INFO_LASTSOCKET  2097181
#define CURLE_AGAIN_ 81

#include <sys/select.h>



static unsigned long net_now_ms_local(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}

#define NET_CUTOFF_MIN    (32L * 1024)
#define NET_CUTOFF_HOSTS  8
#define NET_CHUNKS_MAX    32

static _Thread_local int net_tail_refused_st;
int net_tail_refused(void) { return net_tail_refused_st; }

typedef struct { char h[96]; long cap; } HostCutoff;
static HostCutoff host_cutoffs[NET_CUTOFF_HOSTS];
static int cutoff_next;
static pthread_mutex_t cutoff_lock = PTHREAD_MUTEX_INITIALIZER;

static void host_cutoff_from_url(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  h[0] = 0;
  if (!p || n == 0) return;
  k = (size_t)(p + 3 - u) + strcspn(p + 3, "/?#");
  if (k >= n) k = n - 1;
  memcpy(h, u, k);
  h[k] = 0;
}

long net_host_cutoff(const char *url) {
  char h[96];
  long t = 0;
  int i;
  host_cutoff_from_url(url, h, sizeof h);
  if (!h[0]) return 0;
  pthread_mutex_lock(&cutoff_lock);
  for (i = 0; i < NET_CUTOFF_HOSTS; i++)
    if (host_cutoffs[i].cap && !strcmp(host_cutoffs[i].h, h)) { t = host_cutoffs[i].cap; break; }
  pthread_mutex_unlock(&cutoff_lock);
  return t;
}

static void learn_host_cutoff(const char *url, long received) {
  char h[96], safe[120];
  long cap = (received / (16L * 1024)) * (16L * 1024);
  int i, found = -1, changed = 0;
  if (cap < NET_CUTOFF_MIN) cap = NET_CUTOFF_MIN;
  host_cutoff_from_url(url, h, sizeof h);
  if (!h[0]) return;
  pthread_mutex_lock(&cutoff_lock);
  for (i = 0; i < NET_CUTOFF_HOSTS; i++)
    if (host_cutoffs[i].cap && !strcmp(host_cutoffs[i].h, h)) { found = i; break; }
  if (found < 0) {
    found = cutoff_next; cutoff_next = (cutoff_next + 1) % NET_CUTOFF_HOSTS;
    snprintf(host_cutoffs[found].h, sizeof host_cutoffs[found].h, "%s", h);
    host_cutoffs[found].cap = cap; changed = 1;
  } else if (cap < host_cutoffs[found].cap) { host_cutoffs[found].cap = cap; changed = 1; }
  pthread_mutex_unlock(&cutoff_lock);
  if (changed) {
    printf("[rede] %s corta Range em %ld bytes: pedidos de ate %ld KB neste host daqui em diante\n",
           net_url_public(url, safe, sizeof safe), received, cap / 1024);
    fflush(stdout);
  }
}

char *net_download_range_st(const char *url, int seconds, long long start, long long end,
                            long *size, int *status, int *error,
                            char *final, unsigned final_size) {
  unsigned long t0 = net_now_ms_local(),
                timeout = (unsigned long)(seconds > 0 ? seconds : 30) * 1000UL;
  long requested = (long)(end - start + 1), received = 0;
  char *buf = NULL;
  int st = 0, e = 0, chunks = 0, cuts = 0;
  char current[4096], fin[4096];
  if (size) *size = 0;
  if (status) *status = 0;
  if (error) *error = 0;
  if (final && final_size) final[0] = 0;
  net_tail_refused_st = 0;
  if (!url || requested <= 0) return chunk_once(url, seconds, start, end, size, status, error, final, final_size);
  snprintf(current, sizeof current, "%s", url);
  for (;;) {
    long long a = start + received, b = end; long n = 0, cap = net_host_cutoff(current);
    int seg = seconds, truncated;
    char *r;
    if (cap <= 0) cap = net_host_cutoff(url);
    if (cap > 0 && b - a + 1 > cap) b = a + cap - 1;
    if (chunks > 0) {
      unsigned long elapsed = net_now_ms_local() - t0;
      if (elapsed + 1000UL > timeout) { e = e ? e : 28; goto failed; }
      seg = (int)((timeout - elapsed + 999UL) / 1000UL);
    }
    r = chunk_once(current, seg, a, b, &n, &st, &e, fin, sizeof fin);
    chunks++;
    if (chunks == 1 && final && final_size) snprintf(final, final_size, "%s", fin);
    if (!r && received > 0 && n == 0 && e != 28) net_tail_refused_st = 1;
    if (!r) goto failed;
    if (st != 206) {
      if (chunks == 1) { if (size) *size = n; if (status) *status = st; return r; }
      free(r); goto failed;
    }
    truncated = e != 0;
    { char *new_buf = realloc(buf, (size_t)(received + n + 1));
      if (!new_buf) { free(r); goto failed; }
      buf = new_buf; memcpy(buf + received, r, (size_t)n); received += n; buf[received] = 0; free(r); }
    if (truncated) {
      cuts++;
      learn_host_cutoff(current, n);
      if (strcmp(current, url)) learn_host_cutoff(url, n);
      printf("[rede] Range %lld+%ld: %ld de %ld bytes ate aqui, pedindo o resto (pedaco %d)\n",
             start, requested, received, requested, chunks);
      fflush(stdout);
    }
    if (fin[0]) snprintf(current, sizeof current, "%s", fin);
    if (received >= requested) break;
    if (!truncated && n < b - a + 1) break;
    if (cuts >= NET_CHUNKS_MAX) goto failed;
  }
  if (size) *size = received;
  if (status) *status = 206;
  return buf;
failed:
  if (received > 0) {
    printf("[rede] Range %lld+%ld sem progresso depois de %ld bytes em %d pedaco(s) (HTTP %d, curl %d%s)\n",
           start, requested, received, chunks, st, e,
           net_tail_refused_st ? ", o servidor recusou o resto" : "");
    fflush(stdout);
  }
  free(buf);
  if (size) *size = 0;
  if (status) *status = st;
  if (error) *error = e ? e : (received > 0 ? 18 : 0);
  return NULL;
}

char *net_download_range(const char *url, int seconds, long start, long end,
                         long *size) {
  return net_download_range_st(url, seconds, start, end, size, NULL, NULL, NULL, 0);
}


char *net_download_measured_control(const char *url, int seconds,
                                    const char *const *headers,
                                    const NetControl *control,
                                    NetMeasurement *measurement) {
  int status = 0;
  unsigned long start = net_now_ms_local();
  char *body;
  long limit = control ? control->max_bytes : 0;
  volatile int *cancelled = control ? control->cancelled : NULL;
  net_limit_local = limit > 0 ? limit : 0;
  net_cancel_local = cancelled;
  net_limited_local = 0;
  net_cancelled_local = 0;
  net_bytes_local = 0;
  body = net_download_st(url, seconds, headers, &status);
  if (measurement) {
    measurement->status = status;
    measurement->bytes = net_bytes_local;
    measurement->ms = net_now_ms_local() - start;
    measurement->limited = net_limited_local;
    measurement->cancelled = net_cancelled_local;
  }
  net_limit_local = 0;
  net_cancel_local = NULL;
  net_limited_local = 0;
  net_cancelled_local = 0;
  net_bytes_local = 0;
  return body;
}

char *net_download_bin_measured_control(const char *url, int seconds,
                                        const char *const *headers,
                                        const NetControl *control,
                                        long *size, NetMeasurement *measurement) {
  int status = 0;
  unsigned long start = net_now_ms_local();
  char *body;
  long limit = control ? control->max_bytes : 0;
  volatile int *cancelled = control ? control->cancelled : NULL;
  net_limit_local = limit > 0 ? limit : 0;
  net_cancel_local = cancelled;
  net_limited_local = 0;
  net_cancelled_local = 0;
  net_bytes_local = 0;
  body = net_download_st(url, seconds, headers, &status);
  if (size) *size = net_bytes_local;
  if (measurement) {
    measurement->status = status;
    measurement->bytes = net_bytes_local;
    measurement->ms = net_now_ms_local() - start;
    measurement->limited = net_limited_local;
    measurement->cancelled = net_cancelled_local;
  }
  net_limit_local = 0;
  net_cancel_local = NULL;
  net_limited_local = 0;
  net_cancelled_local = 0;
  net_bytes_local = 0;
  return body;
}

// ===========================================================================
// Backward-compatibility symbol shims
// ===========================================================================

void rede_resposta_limpar(RedeResposta *r) { net_response_clear(r); }
RedeGrupo *rede_grupo_criar(void) { return net_group_create(); }
void rede_grupo_soltar(RedeGrupo *g) { net_group_release(g); }
uint64_t rede_grupo_avancar(RedeGrupo *g) { return net_group_advance(g); }
void rede_grupo_cancelar(RedeGrupo *g) { net_group_cancel(g); }
RedeJob *rede_job_criar(RedeGrupo *g) { return net_job_create(g); }
void rede_job_reter(RedeJob *j) { net_job_retain(j); }
void rede_job_soltar(RedeJob *j) { net_job_release(j); }
void rede_job_cancelar(RedeJob *j) { net_job_cancel(j); }
RedeErro rede_job_estado(RedeJob *j) { return net_job_state(j); }

char *rede_baixar(const char *url, int s) { return net_download(url, s); }
char *rede_baixar_bin(const char *url, int s, long *n) { return net_download_bin(url, s, n); }
char *rede_baixar_com(const char *u, int s, const char *const *h) { return net_download_with(u, s, h); }
char *rede_baixar_trecho(const char *u, int s, long ini, long fim, long *tam) {
  return net_download_range(u, s, ini, fim, tam);
}
char *rede_baixar_trecho_st(const char *u, int s, long long ini, long long fim,
                            long *tam, int *st, int *err, char *fin, unsigned tf) {
  return net_download_range_st(u, s, ini, fim, tam, st, err, fin, tf);
}
long rede_corte_host(const char *url) { return net_host_cutoff(url); }
int rede_resto_recusado(void) { return net_tail_refused(); }
int rede_url_final(const char *u, int s, char *d, unsigned t) { return net_url_final(u, s, d, t); }
int rede_url_final_cab(const char *u, int s, const char *const *h, char *d, unsigned t, int *st) {
  return net_url_final_headers(u, s, h, d, t, st);
}
int rede_url_final_tipo(const char *u, int s, const char *const *h, char *d, unsigned t, int *st,
                        char *m, unsigned mt, long *c) {
  return net_url_final_type(u, s, h, d, t, st, m, mt, c);
}
char *rede_postar(const char *u, int s, const char *const *h, const char *c) {
  return net_post(u, s, h, c);
}
char *rede_apagar(const char *u, int s, const char *const *h, int *st) {
  return net_delete(u, s, h, st);
}
char *rede_postar_st(const char *u, int s, const char *const *h, const char *c, int *st) {
  return net_post_st(u, s, h, c, st);
}
char *rede_baixar_st(const char *u, int s, const char *const *h, int *st) {
  return net_download_st(u, s, h, st);
}
char *rede_baixar_st_retry(const char *u, int s, const char *const *h, int *st, int *ra) {
  return net_download_st_retry(u, s, h, st, ra);
}
char *rede_baixar_medido_controle(const char *u, int s, const char *const *h,
                                  const RedeControle *c, RedeMedida *m) {
  return net_download_measured_control(u, s, h, c, m);
}
char *rede_baixar_bin_medido_controle(const char *u, int s, const char *const *h,
                                      const RedeControle *c, long *t, RedeMedida *m) {
  return net_download_bin_measured_control(u, s, h, c, t, m);
}
int rede_medir_vazao(const char *u, const char *const *h, int s, long ini, long long mb,
                     volatile int *c, int *kps, int nm, RedeVazao *r, char *f, unsigned tf) {
  return net_measure_throughput(u, h, s, ini, mb, c, kps, nm, r, f, tf);
}
void rede_vazao_espera(unsigned long ms) { net_throughput_wait(ms); }
void rede_avisar_401(void (*f)(const char *url)) { net_notify_401(f); }
void rede_avisar_saude(void (*f)(int codigo, const char *url)) { net_notify_health(f); }
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms)) {
  net_notify_host(f);
}
void rede_preparar(void) { net_prepare(); }
const char *rede_ultimo_erro(void) { return net_last_error(); }
