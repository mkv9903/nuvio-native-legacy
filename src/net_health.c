// Network health — see net_health.h.
#include "net_health.h"
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static NetHealth global_health;

int net_health_step(NetHealth *s, int ok, unsigned host, unsigned now_ms) {
  int i;
  if (!host) return 0;
  if (ok) {
    s->failures = 0; s->num_hosts = 0;
    if (s->offline) {
      s->offline = 0; s->restored = now_ms; s->restored_ok = 1; s->seq++;
      return 1;
    }
    return 0;
  }
  if (s->offline) return 0;
  if (!s->failures) s->first = now_ms;
  s->failures++;
  for (i = 0; i < s->num_hosts; i++) if (s->hosts[i] == host) break;
  if (i == s->num_hosts && s->num_hosts < 4) s->hosts[s->num_hosts++] = host;
  if (s->failures >= NET_HEALTH_FAILURES && s->num_hosts >= NET_HEALTH_HOSTS &&
      now_ms - s->first >= NET_HEALTH_WINDOW &&
      !(s->restored_ok && now_ms - s->restored < NET_HEALTH_GRACE)) {
    s->offline = 1; s->seq++;
    return 1;
  }
  return 0;
}

// FNV-1a of URL host ("https://a.b/c" -> "a.b"); 0 for local address.
static unsigned host_hash(const char *url) {
  const char *p = url ? strstr(url, "://") : NULL, *f;
  unsigned h = 2166136261u;
  size_t n;
  if (!p) return 0;
  p += 3;
  f = p + strcspn(p, "/:?#");
  n = (size_t)(f - p);
  if (!n) return 0;
  if ((n >= 4 && !strncmp(p, "127.", 4)) || (n == 9 && !strncmp(p, "localhost", 9)) ||
      (n >= 3 && !strncmp(p, "10.", 3)) || (n >= 8 && !strncmp(p, "192.168.", 8)))
    return 0;
  for (; p < f; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
  return h ? h : 1u;
}

static unsigned current_now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)((unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull);
}

void net_health_note(int code, const char *url) {
  int ok;
  unsigned h;
  switch (code) {
    case 0: ok = 1; break;
    case 6: case 7: case 28: case 35: case 52: case 55: case 56: ok = 0; break;
    default: return;   // cancelled (42), cap (23), improper use: says nothing about network
  }
  h = host_hash(url);
  if (!h) return;
  pthread_mutex_lock(&lock);
  net_health_step(&global_health, ok, h, current_now_ms());
  pthread_mutex_unlock(&lock);
}

int net_health_is_offline(void) {
  int o;
  pthread_mutex_lock(&lock);
  o = global_health.offline;
  pthread_mutex_unlock(&lock);
  return o;
}

unsigned net_health_seq(void) {
  unsigned s;
  pthread_mutex_lock(&lock);
  s = global_health.seq;
  pthread_mutex_unlock(&lock);
  return s;
}

// --- requests per host (see net_health.h) ------------------------------------
typedef struct { char host[64]; int requests, failures; unsigned ms[16]; int num_ms, idx_ms; } HostCount;
static HostCount hosts_count[NET_HOSTS_MAX];
static int num_hosts_count;

void net_hosts_note(const char *url, int code, int http, unsigned ms) {
  const char *p = url ? strstr(url, "://") : NULL, *f;
  char h[64];
  size_t n;
  int i;
  if (!p || code == 42 || code == 23) return;   // cancelled / cap: says nothing
  p += 3;
  f = p + strcspn(p, "/:?#");
  n = (size_t)(f - p);
  if (!n || n >= sizeof h || !host_hash(url)) return;
  memcpy(h, p, n); h[n] = 0;
  pthread_mutex_lock(&lock);
  for (i = 0; i < num_hosts_count; i++) if (!strcmp(hosts_count[i].host, h)) break;
  if (i == num_hosts_count) {
    if (num_hosts_count < NET_HOSTS_MAX) num_hosts_count++;
    else {   // full: evict one with least requests
      int m = 0, k;
      for (k = 1; k < num_hosts_count; k++) if (hosts_count[k].requests < hosts_count[m].requests) m = k;
      i = m;
    }
    memset(&hosts_count[i], 0, sizeof hosts_count[i]);
    memcpy(hosts_count[i].host, h, n + 1);
  }
  hosts_count[i].requests++;
  if (code != 0 || http >= 400) hosts_count[i].failures++;
  if (ms) {
    hosts_count[i].ms[hosts_count[i].idx_ms] = ms;
    hosts_count[i].idx_ms = (hosts_count[i].idx_ms + 1) % 16;
    if (hosts_count[i].num_ms < 16) hosts_count[i].num_ms++;
  }
  pthread_mutex_unlock(&lock);
}

static int cmp_u(const void *a, const void *b) {
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return x < y ? -1 : x > y;
}

int net_hosts_read(NetHost *dst, int max) {
  int i, j, n = 0;
  pthread_mutex_lock(&lock);
  for (i = 0; i < num_hosts_count && n < max; i++) {
    unsigned v[16];
    NetHost r;
    memcpy(r.host, hosts_count[i].host, sizeof r.host);
    r.requests = hosts_count[i].requests; r.failures = hosts_count[i].failures; r.typical_ms = 0;
    if (hosts_count[i].num_ms) {
      memcpy(v, hosts_count[i].ms, sizeof v);
      qsort(v, (size_t)hosts_count[i].num_ms, sizeof *v, cmp_u);
      r.typical_ms = v[hosts_count[i].num_ms / 2];
    }
    // insertion sort descending by request count
    for (j = n; j > 0 && dst[j - 1].requests < r.requests; j--) dst[j] = dst[j - 1];
    dst[j] = r;
    n++;
  }
  pthread_mutex_unlock(&lock);
  return n;
}


// Backward-compatibility shims
int  rede_saude_passo(RedeSaude *s, int ok, unsigned host, unsigned agoraMs) {
  return net_health_step(s, ok, host, agoraMs);
}
void rede_saude_nota(int codigo, const char *url) { net_health_note(codigo, url); }
int  rede_saude_offline(void) { return net_health_is_offline(); }
unsigned rede_saude_seq(void) { return net_health_seq(); }
void rede_hosts_nota(const char *url, int codigo, int http, unsigned ms) {
  net_hosts_note(url, codigo, http, ms);
}
int  rede_hosts_ler(RedeHost *dst, int max) { return net_hosts_read(dst, max); }
