#include "rede.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// ONLY THIS FUNCTION, in its own dedicated file on purpose.
//
// It performs credential redaction for logs (see net.h) and does not touch
// any network: no socket, no curl, no dlopen, no state. The rest of net.c
// has all of that.
//
// WHAT THIS FIXES: tests/debrid.sh mocks the transport (defines its own
// rede_postar_st and rede_baixar_st) and therefore CANNOT link net.c —
// symbols would collide. While this function lived there, the test simply
// could not link, and the obvious alternative — a stub in the test — would be
// the worst solution: the only place where debrid.c hides the /d/<key>/ segment
// of Real-Debrid links would become unexercised code, and a regression leaking
// the key in logs would pass green.
//
// Builds that glob src/*.c (Mac, ARM, Tizen — all three glob) see no difference:
// it is the same function, in the same header.
const char *net_url_public(const char *url, char *dst, unsigned size) {
  const char *e, *h, *at = NULL, *p;
  unsigned n;
  if (!dst || size == 0) return "";
  dst[0] = 0;
  if (!url || !*url) return dst;
  e = strstr(url, "://");
  if (!e) { snprintf(dst, size, "%.*s", (int)size - 1, url); return dst; }
  h = e + 3;
  while (*h && *h != '/' && *h != '?' && *h != '#') h++;
  // Authority can also carry userinfo (common in IPTV addons).
  // Path was already cut, but leaving `user:pass@` before host would
  // still leak credentials into log. Preserve scheme, host and port.
  for (p = e + 3; p < h; p++) if (*p == '@') at = p;
  {
    const char *start = at ? at + 1 : e + 3;
    n = (unsigned)(e + 3 - url);
    if (n >= size) n = size - 1;
    memcpy(dst, url, n);
    if (n < size - 1) {
      unsigned remaining = size - 1 - n;
      unsigned authority = (unsigned)(h - start);
      if (authority > remaining) authority = remaining;
      memcpy(dst + n, start, authority);
      n += authority;
    }
    dst[n] = 0;
  }
  // "/..." indicates there was a path: without it, a bare host looks like
  // a root request, which is a misleading interpretation.
  if (*h && n + 4 < size) { memcpy(dst + n, "/...", 4); dst[n + 4] = 0; }
  return dst;
}

// ---------------------------------------------------------------------------
// net_url_log: see net.h. Rules:
//   - scheme + host (userinfo stripped) always remain;
//   - query and fragment become "?<redacted>" (API key lives there);
//   - on hosts storing user configuration IN THE PATH (list below),
//     every segment becomes <redacted>, except a last one that is a title ID
//     (tt123.jpg) — which indicates WHICH poster failed without revealing whose;
//   - on any host, a "suspicious" segment becomes <redacted>: >= 24 chars,
//     UUID, JWT/JSON-base64 prefix ("eyJ"), or containing = % : { | ~ , ;
//     (serialized config). Exception only for LAST segment when it looks
//     like a media file (base [A-Za-z0-9_-] up to 40 + known extension):
//     TMDB filename has 27-32 chars and indicates WHICH image failed.
#define RL_RED "<redacted>"

static int url_host_key_in_path(const char *h, unsigned n) {
  static const char *const suffixes[] = {
    "btttr.cc", "elfhosted.com", "elfhosted.cc", "ratingposterdb.com",
    "top-streaming.stream", "top-posters.com", "toposters.com", NULL };
  static const char *const chunks[] = {
    "aiometadata", "aiopostr", "betterposter", "postersplus", "posters-plus",
    "top-poster", "toposter", "rpdb", NULL };
  int i;
  for (i = 0; suffixes[i]; i++) {
    unsigned k = (unsigned)strlen(suffixes[i]);
    if (n >= k && !strncasecmp(h + n - k, suffixes[i], k) &&
        (n == k || h[n - k - 1] == '.')) return 1;
  }
  for (i = 0; chunks[i]; i++) {
    unsigned k = (unsigned)strlen(chunks[i]), j;
    for (j = 0; j + k <= n; j++) if (!strncasecmp(h + j, chunks[i], k)) return 1;
  }
  return 0;
}

static int url_is_hex(char c) { return isxdigit((unsigned char)c) != 0; }

static int url_is_uuid(const char *s, unsigned n) {
  unsigned i;
  if (n != 36) return 0;
  for (i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return 0; }
    else if (!url_is_hex(s[i])) return 0;
  }
  return 1;
}

static int url_is_media_file(const char *s, unsigned n) {
  static const char *const ext[] = { "jpg", "jpeg", "png", "webp", "gif", "svg",
                                     "avif", "bmp", NULL };
  unsigned p, i;
  for (p = n; p > 0 && s[p - 1] != '.'; p--) {}
  if (p < 2 || p - 1 > 40) return 0;           // no dot, empty or overly long stem
  for (i = 0; i + 1 < p; i++)
    if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-') return 0;
  for (i = 0; ext[i]; i++)
    if (n - p == strlen(ext[i]) && !strncasecmp(s + p, ext[i], n - p)) return 1;
  return 0;
}

// Last segment on a host with key in path: only stays if it is a title ID
// (tt123, 12345, tmdb-123, with or without extension). Anything else might be configuration itself.
static int url_is_title_id(const char *s, unsigned n) {
  unsigned i = 0, digits = 0;
  if (n >= 2 && s[0] == 't' && s[1] == 't') i = 2;
  else if (n >= 5 && !strncasecmp(s, "tmdb", 4) && (s[4] == '-' || s[4] == '_')) i = 5;
  for (; i < n && isdigit((unsigned char)s[i]); i++) digits++;
  if (!digits || digits > 12) return 0;
  if (i == n) return 1;
  if (s[i] != '.' || n - i - 1 < 2 || n - i - 1 > 5) return 0;
  for (i++; i < n; i++) if (!isalnum((unsigned char)s[i])) return 0;
  return 1;                                     // tt123.jpg, tt123.json
}

int net_segment_is_suspicious(const char *s, unsigned n) {
  unsigned i;
  if (n >= 24) return 1;
  if (url_is_uuid(s, n)) return 1;
  if (n >= 3 && !strncmp(s, "eyJ", 3)) return 1;
  for (i = 0; i < n; i++)
    if (strchr("=%:{}|~,;", s[i])) return 1;
  return 0;
}

const char *net_url_log(const char *url, char *dst, unsigned size) {
  const char *e, *h, *a, *p, *end_host;
  unsigned n = 0;
  int key_in_path;
  if (!dst || size == 0) return "";
  dst[0] = 0;
  if (!url || !*url) return dst;
#define RL_POE(s, k) do { unsigned _k = (unsigned)(k); \
    if (_k > size - 1 - n) { _k = size - 1 - n; } \
    memcpy(dst + n, (s), _k); n += _k; dst[n] = 0; } while (0)
  e = strstr(url, "://");
  if (!e) { snprintf(dst, size, "%s", url); return dst; }
  h = e + 3;
  for (end_host = h; *end_host && *end_host != '/' && *end_host != '?' && *end_host != '#'; end_host++) {}
  for (a = h, p = h; p < end_host; p++) if (*p == '@') a = p + 1;
  RL_POE(url, h - url);
  RL_POE(a, end_host - a);
  key_in_path = url_host_key_in_path(a, (unsigned)(end_host - a));
  p = end_host;
  while (*p == '/') {
    const char *s = p + 1, *f = s;
    int is_last, redacted;
    while (*f && *f != '/' && *f != '?' && *f != '#') f++;
    is_last = *f != '/';
    RL_POE("/", 1);
    if (f > s) {
      unsigned k = (unsigned)(f - s);
      if (key_in_path) redacted = !(is_last && url_is_title_id(s, k));
      else if (is_last && url_is_media_file(s, k)) redacted = 0;
      else redacted = net_segment_is_suspicious(s, k);
      if (redacted) RL_POE(RL_RED, sizeof RL_RED - 1);
      else RL_POE(s, k);
    }
    p = f;
  }
  if (*p == '?' || *p == '#') RL_POE("?" RL_RED, sizeof RL_RED);
#undef RL_POE
  return dst;
}

// Backward-compatibility shims
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  return net_url_public(url, dst, tam);
}
const char *rede_url_log(const char *url, char *dst, unsigned tam) {
  return net_url_log(url, dst, tam);
}
