/* N01: included only by the native net.c, after libcurl initialization.
 * A request owns its easy handle and callback context. No compiler TLS and
 * no implicit retries; legacy workers keep their existing connection cache.
 * CURLOPT_NOSIGNAL prevents process-wide alarms. A libcurl built with a
 * blocking DNS resolver can still block inside DNS past the deadline; the
 * application cannot interrupt that resolver. Never advertise synchronous
 * cancellation or a hard wall-clock DNS bound on those builds.
 */
typedef struct {
  const NetRequest *p;
  NetResponse *r;
  union { uint64_t start; uint64_t inicio; };
  union { uint64_t deadline; uint64_t prazo; };
  union { uint64_t first; uint64_t primeiro; };
  union { uint64_t window; uint64_t janela; };
  union { uint64_t base_wire; uint64_t baseFio; };
  union { uint64_t wire; uint64_t fio; };
  union { size_t body_cap; size_t corpoCap; };
  union { size_t hdr_cap; size_t cabCap; };
  union { size_t seen_hdrs; size_t vistosCab; };
  union { size_t body_ceiling; size_t tetoCorpo; };
  union { size_t hdr_ceiling; size_t tetoCab; };
  int head, status;
  union { uint64_t discarded; uint64_t descartado; };
} NetRequestCtx;
typedef NetRequestCtx PedidoCtx;

#define pedidoAgoraMs net_now_ms
#define pedidoCorpo req_body_cb
#define pedidoCab req_header_cb

static unsigned req_ms(uint64_t v) { return v > UINT_MAX ? UINT_MAX : (unsigned)v; }
static int req_stop(NetRequestCtx *c) {
  NetError e = net_job_state(c->p->job);
  if (e == NET_OK && c->p->stop && c->p->stop(c->p->stop_user)) e = NET_CANCELLED;
  if (e == NET_OK && net_now_ms() >= c->deadline) e = NET_TIMEOUT;
  if (e != NET_OK && c->r->error == NET_OK) c->r->error = e;
  return c->r->error != NET_OK;
}
static int req_save(char **s, size_t *n, size_t *cap, size_t ceiling,
                    const void *b, size_t len) {
  size_t needed, new_cap;
  char *new_buf;
  if (*n > ceiling || len > ceiling - *n) return 0;
  needed = *n + len + 1;
  if (needed > *cap) {
    new_cap = *cap ? *cap : 4096;
    while (new_cap < needed) new_cap *= 2;
    if (new_cap > ceiling + 1) new_cap = ceiling + 1;
    new_buf = realloc(*s, new_cap);
    if (!new_buf) return -1;
    *s = new_buf; *cap = new_cap;
  }
  if (len) memcpy(*s + *n, b, len);
  *n += len; (*s)[*n] = 0;
  return 1;
}
static void req_value(char *dst, size_t cap, const char *s, size_t n) {
  while (n && (*s == ' ' || *s == '\t')) { s++; n--; }
  while (n && isspace((unsigned char)s[n - 1])) n--;
  if (n >= cap) n = cap - 1;
  memcpy(dst, s, n); dst[n] = 0;
}
static size_t req_header_cb(void *b, size_t size, size_t count, void *u) {
  NetRequestCtx *c = u;
  const char *s = b;
  size_t n;
  int ok;
  if (size && count > SIZE_MAX / size) { c->r->error = NET_HEADER_LIMIT; return 0; }
  n = size * count;
  if (req_stop(c)) return 0;
  /* Total across informational headers and redirect hops, before realloc. */
  if (n > c->hdr_ceiling - c->seen_hdrs) { c->r->error = NET_HEADER_LIMIT; return 0; }
  c->seen_hdrs += n;
  if (n >= 5 && !strncasecmp(s, "HTTP/", 5)) {
    char line[80];
    c->r->n_headers = 0;
    c->r->mime[0] = 0; c->r->retry_after_s = 0;
    req_value(line, sizeof line, s, n);
    { const char *p = strchr(line, ' '); c->status = p ? atoi(p + 1) : 0; }
  }
  if (n > 13 && !strncasecmp(s, "Content-Type:", 13)) {
    char *end;
    req_value(c->r->mime, sizeof c->r->mime, s + 13, n - 13);
    end = strchr(c->r->mime, ';'); if (end) *end = 0;
  } else if (n > 12 && !strncasecmp(s, "Retry-After:", 12)) {
    c->r->retry_after_s = net_retry_seconds(s + 12, n - 12);
  } else if (!c->head && !c->p->body_window_ms && n > 15 && !strncasecmp(s, "Content-Length:", 15)) {
    size_t i = 15, value = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    while (i < n && isdigit((unsigned char)s[i])) {
      unsigned dig = (unsigned)(s[i++] - '0');
      if (value > c->body_ceiling / 10 || value * 10 + dig > c->body_ceiling) {
        c->r->error = NET_BODY_LIMIT; return 0;
      }
      value = value * 10 + dig;
    }
  }
  ok = req_save(&c->r->headers, &c->r->n_headers, &c->hdr_cap,
                c->hdr_ceiling, b, n);
  if (ok != 1) { c->r->error = ok ? NET_MEMORY : NET_HEADER_LIMIT; return 0; }
  return n;
}
static size_t req_body_cb(void *b, size_t size, size_t count, void *u) {
  NetRequestCtx *c = u;
  size_t n;
  int ok;
  if (size && count > SIZE_MAX / size) { c->r->error = NET_BODY_LIMIT; return 0; }
  n = size * count;
  if (req_stop(c)) return 0;
  if (c->p->body_window_ms && !(c->p->follow &&
      (c->status == 301 || c->status == 302 || c->status == 303 || c->status == 307 || c->status == 308))) {
    uint64_t now = net_now_ms();
    size_t prefix_len;
    if (c->first && now - c->first >= c->p->body_window_ms) {
      c->r->end_window = 1; return 0;
    }
    if (n > c->p->max_discarded - c->discarded) { c->r->end_cap = 1; return 0; }
    if (n && !c->first) {
      c->first = c->window = now;
      c->r->first_byte_ms = req_ms(now - c->start);
    }
    prefix_len = sizeof c->r->prefix - c->r->n_prefix;
    if (prefix_len > n) prefix_len = n;
    memcpy(c->r->prefix + c->r->n_prefix, b, prefix_len);
    c->r->n_prefix += (unsigned)prefix_len;
    c->discarded += n;
    return n;
  }
  /* Cap applies to decoded bytes, including gzip expansion, before malloc. */
  if (n > c->body_ceiling - c->r->n_body) { c->r->error = NET_BODY_LIMIT; return 0; }
  if (n && !c->first) {
    c->first = c->window = net_now_ms();
    c->r->first_byte_ms = req_ms(c->first - c->start);
  }
  ok = req_save(&c->r->body, &c->r->n_body, &c->body_cap, c->body_ceiling, b, n);
  if (ok != 1) { c->r->error = ok ? NET_MEMORY : NET_BODY_LIMIT; return 0; }
  return n;
}
static int req_is_redirect(int st) {
  return st == 301 || st == 302 || st == 303 || st == 307 || st == 308;
}
static void req_interval(NetRequestCtx *c, uint64_t now, int final) {
  NetInterval v;
  uint64_t elapsed;
  if (!c->first || (c->p->follow && req_is_redirect(c->status))) return;
  elapsed = now - c->window;
  if (!elapsed || (!final && elapsed < 1000)) return;
  v.bytes = c->wire >= c->base_wire ? c->wire - c->base_wire : 0;
  v.ms = req_ms(elapsed); v.start_ms = req_ms(c->window - c->first);
  v.complete = elapsed >= 1000;
  if (v.complete) c->r->complete_intervals++;
  if (c->p->interval) c->p->interval(&v, c->p->interval_user);
  c->window = now; c->base_wire = c->wire;
}
static int req_progress_cb(void *u, long long total, long long now_wire,
                           long long up_total, long long up_now) {
  NetRequestCtx *c = u;
  (void)total; (void)up_total; (void)up_now;
  if (req_stop(c)) return 1;
  if (now_wire >= 0 && (uint64_t)now_wire > c->wire) c->wire = (uint64_t)now_wire;
  req_interval(c, net_now_ms(), 0);
  if (c->p->body_window_ms && c->first &&
      !(c->p->follow && req_is_redirect(c->status)) &&
      net_now_ms() - c->first >= c->p->body_window_ms) {
    c->r->end_window = 1; return 1;
  }
  return req_stop(c);
}

/* Normalize scheme/host/default port without copying path/query secrets.
 * Accept explicit IPv6 and DNS ASCII/punycode. Userinfo/control characters,
 * ambiguous authorities and non-HTTP protocols are rejected before I/O. */
static int req_origin(const char *u, char *dst, size_t cap, int *tls) {
  const char *p, *end, *host, *host_end, *port_str = NULL;
  size_t n, i;
  unsigned port;
  if (!u || (n = strnlen(u, 4096)) >= 4096) return 0;
  for (i = 0; i < n; i++) if ((unsigned char)u[i] <= 32 || u[i] == '\\' || (unsigned char)u[i] == 127) return 0;
  if (!strncasecmp(u, "https://", 8)) { *tls = 1; p = u + 8; port = 443; }
  else if (!strncasecmp(u, "http://", 7)) { *tls = 0; p = u + 7; port = 80; }
  else return 0;
  end = p + strcspn(p, "/?#"); host = p;
  if (p == end || memchr(p, '@', (size_t)(end - p))) return 0;
  if (*p == '[') {
    host_end = memchr(p, ']', (size_t)(end - p));
    if (!host_end) return 0;
    host_end++;
    if (host_end != end) { if (*host_end != ':') return 0; port_str = host_end + 1; }
  } else {
    host_end = memchr(p, ':', (size_t)(end - p));
    if (host_end) port_str = host_end + 1; else host_end = end;
    for (p = host; p < host_end; p++)
      if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-' && *p != '_') return 0;
  }
  if (host == host_end) return 0;
  if (port_str) {
    if (port_str == end) return 0;
    port = 0;
    for (p = port_str; p < end; p++) {
      if (!isdigit((unsigned char)*p) || port > 6553) return 0;
      port = port * 10 + (unsigned)(*p - '0');
    }
    if (!port || port > 65535) return 0;
  }
  n = (size_t)(host_end - host);
  if (n + 16 >= cap) return 0;
  memcpy(dst, *tls ? "https://" : "http://", *tls ? 8 : 7);
  i = *tls ? 8 : 7;
  for (p = host; p < host_end; p++) dst[i++] = (char)tolower((unsigned char)*p);
  snprintf(dst + i, cap - i, ":%u", port);
  return 1;
}
static int req_method_valid(const char *m) {
  return !strcmp(m, "GET") || !strcmp(m, "HEAD") || !strcmp(m, "POST") ||
         !strcmp(m, "PUT") || !strcmp(m, "PATCH") || !strcmp(m, "DELETE");
}
static int req_headers_valid(const char *const *hdrs, size_t ceiling) {
  size_t seen = 0;
  unsigned k;
  for (k = 0; hdrs && hdrs[k] && k < 64; k++) {
    const char *s = hdrs[k], *end;
    size_t n = strnlen(s, ceiling + 1), i;
    if (n > ceiling) return 0;
    end = memchr(s, ':', n);
    if (!end || end == s || n + 2 > ceiling - seen) return 0;
    for (i = 0; i < (size_t)(end - s); i++)
      if (!isalnum((unsigned char)s[i]) && !strchr("!#$%&'*+-.^_`|~", s[i])) return 0;
    for (i = (size_t)(end - s) + 1; i < n; i++)
      if (((unsigned char)s[i] < 32 && s[i] != '\t') || (unsigned char)s[i] == 127) return 0;
    /* Avoid implicit proxy/origin routing and inconsistent body framing. */
    if ((end - s == 4 && !strncasecmp(s, "Host", 4)) ||
        (end - s == 14 && !strncasecmp(s, "Content-Length", 14)) ||
        (end - s == 17 && !strncasecmp(s, "Transfer-Encoding", 17))) return 0;
    seen += n + 2;
  }
  return !hdrs || !hdrs[k];
}
unsigned net_request_capabilities(void) {
  return NET_CAP_BODY | NET_CAP_JOB | NET_CAP_REDIRECT | NET_CAP_INTERVAL;
}
int net_request(const NetRequest *p, NetResponse *r) {
  NetRequestCtx c;
  void *curl = NULL, *list = NULL;
  const char *method;
  const void *body;
  size_t n_body;
  char url[4096], origin[300];
  int tls, headers_allowed = 1, hop, ok = 0;
  if (!r) return 0;
  memset(r, 0, sizeof *r); memset(&c, 0, sizeof c);
  c.p = p; c.r = r;
  if (!p) { r->error = NET_INPUT; return 0; }
  method = p->method ? p->method : "GET";
  c.head = !strcmp(method, "HEAD");
  c.body_ceiling = p->max_bytes ? p->max_bytes : NET_BODY_DEFAULT;
  c.hdr_ceiling = p->max_headers ? p->max_headers : NET_HDR_DEFAULT;
  if (!req_method_valid(method) || !req_origin(p->url, origin, sizeof origin, &tls) ||
      c.body_ceiling > NET_BODY_MAX || c.hdr_ceiling > NET_HDR_MAX ||
      p->timeout_ms > 300000 || (p->n_body && !p->body) ||
      p->n_body > NET_BODY_DEFAULT || ((c.head || !strcmp(method, "GET")) && p->n_body) ||
      p->body_window_ms > 60000 || (p->body_window_ms &&
        (strcmp(method, "GET") || !p->max_discarded || p->max_discarded > UINT64_C(2147483648))) ||
      !req_headers_valid(p->headers, c.hdr_ceiling)) {
    r->error = NET_INPUT; return 0;
  }
  net_job_retain(p->job);
  c.start = net_now_ms();
  c.deadline = c.start + (p->timeout_ms ? p->timeout_ms : 15000);
  if (req_stop(&c)) goto done;
  if (!net_curl_open() || !curl_cleanup || !curl_getinfo || !slist_append || !slist_free) {
    r->error = NET_UNAVAILABLE; goto done;
  }
  curl = curl_init();
  if (!curl) { r->error = NET_MEMORY; goto done; }
  strcpy(url, p->url); body = p->body; n_body = p->n_body;
  for (hop = 0; ; hop++) {
    long status = 0;
    uint64_t now;
    double wire = 0;
    char *dest = NULL;
    unsigned k;
    if (req_stop(&c)) break;
    if (list) { slist_free(list); list = NULL; }
    for (k = 0; headers_allowed && p->headers && p->headers[k]; k++) {
      void *new_item = slist_append(list, p->headers[k]);
      if (!new_item) { r->error = NET_MEMORY; break; }
      list = new_item;
    }
    if (r->error) break;
    now = net_now_ms();
    if (req_stop(&c)) break;
    /* Required options fail closed, including verification on old SDKs. */
#define REQ_OPT(op, val) do { if (curl_setopt(curl, (op), (val))) { r->error = NET_UNAVAILABLE; goto done; } } while (0)
    REQ_OPT(OPT_URL, url);
    REQ_OPT(OPT_FOLLOWLOCATION, 0L);
    REQ_OPT(OPT_PROTOCOLS, 3L); /* HTTP | HTTPS; never file/ftp/etc */
    REQ_OPT(OPT_TIMEOUT_MS, (long)(c.deadline - now));
    REQ_OPT(OPT_CONNECTTIMEOUT_MS, (long)(c.deadline - now));
    REQ_OPT(OPT_NOSIGNAL, 1L);
    REQ_OPT(OPT_SSL_VERIFYPEER, 1L);
    REQ_OPT(OPT_SSL_VERIFYHOST, 2L);
    if (p->ca_file) REQ_OPT(OPT_CAINFO, p->ca_file);
    REQ_OPT(OPT_USERAGENT, "Nuvio/1.8");
    /* Media diagnostics measure the player's wire body, without transparent
     * gzip inflation. Buffered requests retain negotiated decompression. */
    REQ_OPT(OPT_ACCEPT_ENCODING, p->body_window_ms ? (void *)NULL : "");
    REQ_OPT(OPT_HTTPHEADER, list);
    REQ_OPT(OPT_NOBODY, c.head ? 1L : 0L);
    REQ_OPT(OPT_CUSTOMREQUEST, method);
    if (body || n_body || (strcmp(method, "GET") && !c.head)) {
      REQ_OPT(OPT_POSTFIELDSIZE, (long)n_body);
      REQ_OPT(OPT_POSTFIELDS, body ? body : "");
    } else {
      /* POSTFIELDS=NULL enables POST; explicitly restore GET after a 303. */
      REQ_OPT(OPT_POSTFIELDS, (void *)NULL);
      REQ_OPT(80 /* CURLOPT_HTTPGET */, c.head ? 0L : 1L);
      REQ_OPT(OPT_NOBODY, c.head ? 1L : 0L);
    }
    REQ_OPT(OPT_WRITEFUNCTION, req_body_cb);
    REQ_OPT(OPT_WRITEDATA, &c);
    REQ_OPT(OPT_HEADERFUNCTION, req_header_cb);
    REQ_OPT(OPT_HEADERDATA, &c);
    REQ_OPT(OPT_XFERINFOFUNCTION, req_progress_cb);
    REQ_OPT(OPT_XFERINFODATA, &c);
    REQ_OPT(OPT_NOPROGRESS, 0L);
#undef REQ_OPT
    strcpy(r->final, url);
    net_url_public(url, r->host, sizeof r->host);
    r->curl_error = curl_perform(curl);
    curl_getinfo(curl, INFO_RESPONSE_CODE, &status); r->status = (int)status;
    if (!curl_getinfo(curl, INFO_SIZE_DOWNLOAD, &wire) && wire >= 0 && wire < 9007199254740992.0)
      c.wire = (uint64_t)wire;
    r->wire_bytes = c.wire;
    now = net_now_ms();
    if (c.first) r->body_ms = req_ms(now - c.first);
    if (req_stop(&c)) break;
    if (r->curl_error && !(p->body_window_ms && (r->end_window || r->end_cap) &&
        (r->curl_error == 23 || r->curl_error == 42))) {
      r->error = r->curl_error == 28 ? NET_TIMEOUT : NET_TRANSPORT; break;
    }
    if (!p->follow || !req_is_redirect(r->status)) {
      req_interval(&c, now, 1);
      if (req_stop(&c)) break;
      /* Empty 204/HEAD still owns a terminated buffer like an ordinary reply. */
      if (!r->body && !p->body_window_ms) {
        r->body = calloc(1, 1);
        if (!r->body) { r->error = NET_MEMORY; break; }
      }
      ok = 1; break;
    }
    if (hop >= 5 || curl_getinfo(curl, INFO_REDIRECT_URL, &dest) || !dest) {
      r->error = NET_REDIRECT; break;
    }
    {
      char new_origin[300]; int new_tls;
      if (!req_origin(dest, new_origin, sizeof new_origin, &new_tls) || (tls && !new_tls)) {
        r->error = NET_REDIRECT; break;
      }
      /* 303 converts to GET (except HEAD); 301/302 only convert POST. */
      if ((r->status == 303 && !c.head) || ((r->status == 301 || r->status == 302) && !strcmp(method, "POST"))) {
        method = "GET"; body = NULL; n_body = 0;
      }
      if (strcmp(origin, new_origin)) {
        if (strcmp(method, "GET") && strcmp(method, "HEAD")) { r->error = NET_REDIRECT; break; }
        /* All caller headers, including unknown API-key names, are dropped.
         * Do not restore them if a later hop returns to the original host. */
        headers_allowed = 0;
      }
      strcpy(url, dest); strcpy(origin, new_origin); tls = new_tls;
    }
    free(r->body); r->body = NULL; r->n_body = c.body_cap = 0;
    r->n_headers = 0; r->mime[0] = 0; r->retry_after_s = 0;
    r->wire_bytes = r->body_ms = r->first_byte_ms = r->complete_intervals = 0;
    c.wire = c.base_wire = c.first = c.window = 0;
    c.discarded = 0; r->n_prefix = 0; r->end_window = r->end_cap = 0;
  }
done:
  r->ms = req_ms(net_now_ms() - c.start);
  if (!ok) { free(r->body); r->body = NULL; r->n_body = 0; }
  if (list) slist_free(list);
  if (curl) curl_cleanup(curl);
  /* Cleanup may block briefly; fence generation again at the handoff. */
  if (ok) {
    NetError e = net_job_state(p->job);
    if (e != NET_OK) { r->error = e; ok = 0; free(r->body); r->body = NULL; r->n_body = 0; }
  }
  net_job_release(p->job);
  /* Consumers choose retry/status policy. No credentials, headers or URL
   * path are logged, and cancellation/staleness is not a network outage. */
  return ok;
}

// Backward-compatibility shims
unsigned rede_pedido_capacidades(void) {
  return net_request_capabilities();
}
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  return net_request((const NetRequest *)p, (NetResponse *)r);
}
