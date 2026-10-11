/* Shared add-on URL limits and the staged C/Rust adapter.
 *
 * Provider configuration can be embedded in long paths or queries. Keep the
 * entire URL or reject it; never send a silently truncated configuration.
 * 2048 covers the measured ~870-byte Comet configuration with room to grow.
 * Requests have another 1024 bytes for the resource path, below rede.c's 4096.
 *
 * Primary base storage uses NV_ADDON_URL_MAX. Secondary collection/channel
 * copies remain smaller to avoid multiplying TV RAM use; empty copies fall
 * back to the primary base rather than retaining a truncated URL.
 *
 * Logs contain an add-on name and lengths only. Never log a configured URL.
 * Rust owns the URL rules in NV_RUST_CORE builds. C keeps variadic formatting
 * and log integration; existing callers and saved/external fields keep names.
 * Normal C builds retain the reference implementation during migration.
 */
#ifndef NV_ADDONURL_H
#define NV_ADDONURL_H

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define NV_ADDON_URL_MAX    2048
#define NV_ADDON_PEDIDO_MAX (NV_ADDON_URL_MAX + 1024)

/* Check snprintf's required length against a fixed-size destination. */
#define NV_COUBE(w, buf) ((w) >= 0 && (size_t)(w) < sizeof(buf))

#ifdef NV_RUST_CORE
#include "rust_core.h"

/* Reject an oversized installed URL without exposing its configuration. */
static inline int nv_addon_url_cabe(const char *name, size_t length) {
  if (nuvio_rust_addon_url_fits(length)) return 1;
  printf("[addons] %s: URL length %lu exceeds maximum %d: add-on skipped\n",
         name && *name ? name : "Addon", (unsigned long)length, NV_ADDON_URL_MAX - 1);
  fflush(stdout);
  return 0;
}

/* A failed or truncated request must never reach the network. */
static inline int nv_addon_pedido_coube(const char *name, int length, size_t capacity) {
  if (nuvio_rust_addon_request_fits(length, capacity)) return 1;
  printf("[addons] %s: request length %d exceeds maximum %lu: skipped\n",
         name && *name ? name : "Addon", length, (unsigned long)(capacity ? capacity - 1 : 0));
  fflush(stdout);
  return 0;
}

/* Stable Rust cannot accept C variadic arguments. Format only the resource
 * path here, then let Rust join it before the unchanged base query. */
static inline int nv_addon_url(char *output, size_t capacity, const char *base,
                               const char *format, ...) {
  char path[NV_ADDON_PEDIDO_MAX];
  va_list args;
  va_start(args, format);
  int length = vsnprintf(path, sizeof path, format, args);
  va_end(args);
  return nuvio_rust_addon_request(output, capacity, base, path, length);
}

/* UTF-8 and other non-ASCII bytes are percent-encoded individually. */
static inline int nv_addon_id(char *output, size_t capacity, const char *id) {
  return nuvio_rust_addon_id(output, capacity, id);
}

/* Remove outer C whitespace, stremio scheme, trailing slashes and manifest
 * suffix. Keep a nonempty query verbatim. Overflow produces an empty string.
 * Inputs and outputs are caller-owned, NUL-terminated, non-overlapping bytes. */
static inline void nv_addon_base(const char *url, char *output, size_t capacity) {
  nuvio_rust_addon_base(url, output, capacity);
}

#else
/* Legacy C reference: leave behavior unchanged until all callers migrate. */
static inline int nv_addon_url_cabe(const char *nome, size_t len) {
  if (len < NV_ADDON_URL_MAX) return 1;
  printf("[addons] %s: URL de %lu caracteres nao cabe (maximo %d): addon ignorado\n",
         nome && *nome ? nome : "Addon", (unsigned long)len, NV_ADDON_URL_MAX - 1);
  fflush(stdout);
  return 0;
}

static inline int nv_addon_pedido_coube(const char *nome, int w, size_t tam) {
  if (w >= 0 && (size_t)w < tam) return 1;
  printf("[addons] %s: pedido de %d caracteres nao cabe (maximo %lu): pulado\n",
         nome && *nome ? nome : "Addon", w, (unsigned long)tam - 1);
  fflush(stdout);
  return 0;
}

static inline int nv_addon_url(char *dst, size_t n, const char *base, const char *fmt, ...) {
  char caminho[NV_ADDON_PEDIDO_MAX];
  const char *q;
  int lb, wp, w;
  va_list ap;
  if (!base) base = "";
  q = strchr(base, '?');
  lb = (int)(q ? (size_t)(q - base) : strlen(base));
  while (lb > 0 && base[lb - 1] == '/') lb--;
  va_start(ap, fmt);
  wp = vsnprintf(caminho, sizeof caminho, fmt, ap);
  va_end(ap);
  if (wp < 0 || (size_t)wp >= sizeof caminho) { if (dst && n) dst[0] = 0; return wp < 0 ? -1 : lb + wp; }
  if (!dst || !n) return -1;
  w = snprintf(dst, n, "%.*s%s%s", lb, base, caminho, q ? q : "");
  if (w < 0 || (size_t)w >= n) dst[0] = 0;
  return w;
}

static inline int nv_addon_id(char *dst, size_t n, const char *id) {
  static const char HEX[] = "0123456789ABCDEF";
  const unsigned char *p = (const unsigned char *)(id ? id : "");
  size_t k = 0;
  if (!dst || !n) return 0;
  for (; *p; p++) {
    int livre = isalnum(*p) || (*p && strchr("-._~:@!$&'()*+,;=", *p));
    if (*p >= 0x80) livre = 0;
    if (k + (livre ? 1 : 3) >= n) { dst[0] = 0; return 0; }
    if (livre) dst[k++] = (char)*p;
    else { dst[k++] = '%'; dst[k++] = HEX[*p >> 4]; dst[k++] = HEX[*p & 15]; }
  }
  dst[k] = 0;
  return 1;
}

static inline void nv_addon_base(const char *url, char *dst, size_t tam) {
  const char *ini, *fim, *q;
  size_t k;
  int w;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!url) return;
  ini = url;
  while (*ini && isspace((unsigned char)*ini)) ini++;
  fim = ini + strlen(ini);
  while (fim > ini && isspace((unsigned char)fim[-1])) fim--;
  q = memchr(ini, '?', (size_t)(fim - ini));
  if (!q) q = fim;
  w = !strncasecmp(ini, "stremio://", 10)
      ? snprintf(dst, tam, "https://%.*s", (int)(q - ini - 10), ini + 10)
      : snprintf(dst, tam, "%.*s", (int)(q - ini), ini);
  if (w < 0 || (size_t)w >= tam) { dst[0] = 0; return; }
  k = (size_t)w;
  while (k && dst[k - 1] == '/') dst[--k] = 0;
  if (k > 14 && !strcasecmp(dst + k - 14, "/manifest.json")) { k -= 14; dst[k] = 0; }
  while (k && dst[k - 1] == '/') dst[--k] = 0;
  if (fim - q > 1) {
    w = snprintf(dst + k, tam - k, "%.*s", (int)(fim - q), q);
    if (w < 0 || (size_t)w >= tam - k) dst[0] = 0;
  }
}

#endif /* NV_RUST_CORE */
#endif /* NV_ADDONURL_H */
