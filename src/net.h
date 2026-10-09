// Simple HTTP(S) fetch, into memory.
//
// Uses the DEVICE'S libcurl via dlopen. Writing TLS by hand was out of the
// question and the SDK does not provide libcurl to link against — but
// /usr/lib/libcurl.so.5 exists on the TV, and addons only speak HTTPS. On Mac
// it uses system libcurl.
#ifndef NV_NET_H
#define NV_NET_H

#include <stddef.h>
#include <stdint.h>

/* Additive API for workers/plugins. Does not use the per-thread state of wrappers.
 * The request BLOCKS; execute it outside drawing. Strings, body, and callback
 * remain valid until return. net_response_clear frees the two allocations.
 * Zero max_bytes/max_headers use safe limits, never "unlimited".
 * TLS is verified; ca_file overrides the trusted bundle registered at startup
 * (net_discord_ca), or uses system trust if there is no bundle.
 * Redirects HTTP(S) only, without HTTPS downgrade. Owner headers and body do
 * not cross origins; cross-origin authenticated 307/308 are rejected.
 * WGT synchronous XHR does not provide this contract: returns UNAVAILABLE without
 * starting I/O. Legacy wrappers remain available with their limits.
 */
typedef struct NetGroup NetGroup;
typedef struct NetJob NetJob;
typedef enum {
  NET_OK = 0, NET_INPUT, NET_UNAVAILABLE, NET_MEMORY,
  NET_TRANSPORT, NET_TIMEOUT, NET_CANCELLED, NET_GENERATION,
  NET_BODY_LIMIT, NET_HEADER_LIMIT, NET_REDIRECT
} NetError;

#define NET_BODY_DEFAULT (8u * 1024u * 1024u)
#define NET_BODY_MAX     (32u * 1024u * 1024u)
#define NET_HDR_DEFAULT  16384u
#define NET_HDR_MAX      65536u
#define NET_CAP_BODY     1u
#define NET_CAP_JOB      2u
#define NET_CAP_REDIRECT 4u
#define NET_CAP_INTERVAL 8u

typedef struct NetInterval {
  union { uint64_t bytes; };         /* wire payload, before decompression */
  union { unsigned ms; };           /* real duration; never invents 1s windows */
  union { unsigned start_ms; unsigned inicio_ms; }; /* relative to first body byte */
  union { int complete; int completa; };            /* >= 1s; short final sample is 0 */
} NetInterval;

typedef struct NetRequest {
  union { const char *method; const char *metodo; }; /* NULL = GET; GET/HEAD/POST/PUT/PATCH/DELETE */
  const char *url;
  union { const char *const *headers; const char *const *cabecalhos; };
  union { const void *body; const void *corpo; };
  union { size_t n_body; size_t n_corpo; };          /* binary length, not strlen */
  union { unsigned timeout_ms; unsigned prazo_ms; }; /* total, including redirects; 0 = 15s, max 300s */
  size_t max_bytes;
  union { size_t max_headers; size_t max_cabecalhos; };
  union { int follow; int seguir; };                 /* 0 = return 3xx; 1 = up to 5 redirects */
  union { const char *ca_file; const char *ca_arquivo; };
  /* #290: 1 = PERSONAL server configured by user (Jellyfin/Emby/Plex),
   * which often has a self-signed or local-network certificate. The certificate
   * is only unverified while the request stays on the SAME origin
   * (scheme+host+port) as `url`; a redirect to another origin verifies again.
   * Everything else in the app continues verifying. */
  union { int tls_personal; int tls_pessoal; };
  union { NetJob *job; };
  union { void (*interval)(const NetInterval *, void *); void (*intervalo)(const NetInterval *, void *); };
  union { void *interval_user; void *intervalo_usuario; };
  /* Optional streamed/discarded GET: keeps only a 512-byte prefix. Stops at
   * window or cap, never allocates the media body. A byte cap does not prove
   * a completed time window. Zero keeps the normal buffered contract. */
  union { unsigned body_window_ms; unsigned janela_corpo_ms; }; /* from first body byte; max 60 s */
  union { uint64_t max_discarded; uint64_t max_descartado; };   /* required with window; max 2 GiB */
  union { int (*stop)(void *); int (*parar)(void *); };         /* additional caller cancellation, no UI work */
  union { void *stop_user; void *parar_usuario; };
} NetRequest;

typedef struct NetResponse {
  int status;
  union { int curl_error; int curl_erro; };
  union { NetError error; NetError erro; }; /* HTTP 4xx/5xx is a response, not a transport error */
  union { char *body; char *corpo; };
  union { char *headers; char *cabecalhos; };
  union { size_t n_body; size_t n_corpo; };
  union { size_t n_headers; size_t n_cabecalhos; };
  char final[4096], host[256], mime[128];  /* final may be private: never log */
  int retry_after_s;
  unsigned ms;
  union { unsigned first_byte_ms; unsigned primeiro_byte_ms; };
  union { unsigned body_ms; unsigned corpo_ms; };
  union { unsigned complete_intervals; unsigned intervalos_completos; };
  union { uint64_t wire_bytes; uint64_t bytes_fio; }; /* separate from n_body (decompressed) */
  union { unsigned char prefix[512]; unsigned char prefixo[512]; };
  union { unsigned n_prefix; unsigned n_prefixo; };
  union { int end_window; int fim_janela; }; /* intentional discard stops, not transport EOF */
  union { int end_cap; int fim_teto; };
} NetResponse;

unsigned net_request_capabilities(void);
int net_request(const NetRequest *request, NetResponse *response);
void net_response_clear(NetResponse *response);

/* Group lives outside request: advancing when switching account/profile/title
 * invalidates all previous jobs. Retained jobs prevent UAF; no generation
 * becomes valid again. Cancelling a job does not cancel its siblings. Releasing
 * the owner group does not cancel it; cancel explicitly upon shutdown. The caller
 * must check net_job_state also when publishing to a UI queue: changing generation
 * AFTER return cannot recall an already delivered result.
 * No scheduler/pool: integrates existing workers. No compiler TLS.
 */
NetGroup *net_group_create(void);
void net_group_release(NetGroup *group);
uint64_t net_group_advance(NetGroup *group);
void net_group_cancel(NetGroup *group);
NetJob *net_job_create(NetGroup *group);
void net_job_retain(NetJob *job);
void net_job_release(NetJob *job);
void net_job_cancel(NetJob *job);
NetError net_job_state(NetJob *job);

/* Bounded, validated byte ranges for the local DTS demuxer. Unlike the old
 * long-offset API this works past 2 GiB on ARM. Only matching HTTP 206 bodies
 * are accepted; total is -1 when unknown. Caller frees the returned buffer.
 * Raw headers use one Name: value per line. Credentials are stripped when a
 * redirect changes origin. Cancelling also interrupts an idle transfer. */
char *net_download_range64_final(const char *url, const char *headers,
                                 int64_t start, int64_t end, long *size,
                                 int64_t *total, int *status,
                                 volatile int *cancelled, char *final, int *cross);
char *net_download_range64_headers(const char *url, const char *headers,
                                   int64_t start, int64_t end, long *size,
                                   int64_t *total, int *status,
                                   volatile int *cancelled);

typedef struct NetMeasurement {
  int status;                              // 0 = transport without response
  long bytes;                              // received body; -1 when not measured
  unsigned long ms;                        // total request time
  union { int limited; int limitado; };    // 1 = per-request ceiling reached
  union { int cancelled; int cancelado; }; // 1 = cancellation interrupted transfer
} NetMeasurement;

typedef struct NetControl {
  long max_bytes;                                               // 0 = no additional ceiling
  union { volatile int *cancelled; volatile int *cancelado; }; // optional; checked during transfer
} NetControl;

// Downloads entire `url` into a new buffer (NUL-terminated) and returns it;
// caller frees with free(). NULL on any failure. BLOCKS — call from own thread,
// never from render loop.
char *net_download(const char *url, int seconds);

// Same, but for BINARY content: returns size in *n. Above version ends in NUL
// and is suitable for JSON; images contain zeroes in the middle and strlen would lie.
char *net_download_bin(const char *url, int seconds, long *n);

#ifdef __EMSCRIPTEN__
// POST of text/plain body with BINARY response (size in *n; 4xx/5xx become NULL,
// as in net_download_bin). Only exists on Tizen, for Xtream proxy (#112): icon URL
// goes in BODY, which worker platform does not log, and returns image with zeroes
// inside — net_post_st does not provide size. See tex_cache.c and servidor/recomendacoes/src/xtream.js.
char *net_post_bin(const char *url, int seconds, const char *body, long *n);
#endif

// With headers. `headers` is a NULL-terminated array of preformatted lines
// ("Authorization: Bearer x"). Exists because Trakt requires token and app key
// in headers — impossible to pass via URL.
char *net_download_with(const char *url, int seconds, const char *const *headers);

// Downloads ONLY A RANGE via HTTP Range header. Returns size in *size.
//
// Exists to read MKV headers without pulling the entire file: LG's pipeline
// returns "language":"(null)" on EVERY subtitle track (measured on a 43-sub file —
// audio has language, subtitles do not), and the only way to know language is
// reading the container itself, as web app does in browser.
//
// Server ignoring Range returns entire file; caller must be prepared to receive
// MORE than requested and stop reading when desired data is found.
char *net_download_range(const char *url, int seconds, long start, long end,
                         long *size);

// Same, returning WHY it failed (#92). `*status` receives final HTTP response
// code after redirects (0 = no response), and `*error` receives libcurl code
// (28 = timeout, 7 = connection refused; 0 on Tizen without libcurl). Body only
// returned on 2xx: 403/429/416 is NULL as before, but caller can distinguish
// "CDN refused connection" from "network down" — mkvass treats one as rate limit
// and the other as temporary failure. `final` (optional, `final_size` bytes)
// receives address after redirects — same as net_url_final would give, without
// extra request. Any pointer may be NULL.
//
// CHUNKED/CUT BODY (#92): a 206 closing before end (curl 18) is no longer a failure —
// received data remains and remainder is requested from next byte until complete
// or chunk returns nothing (then NULL with code). Host that cuts gets remembered
// request ceiling for the whole session (net_host_cutoff), and future ranges
// start in chunks of that size. `seconds` is timeout for ENTIRE range, not each chunk.
// net_download_range uses the same loop. `start`/`end` in 64 bits: Range past 2 GiB
// on 32-bit ARM (#269).
char *net_download_range_st(const char *url, int seconds, long long start, long long end,
                            long *size, int *status, int *error,
                            char *final, unsigned final_size);

// Learned request ceiling for host of `url` (scheme, host, port), in bytes;
// 0 = host never truncated Range in this session. Truncating host is also candidate
// for dropping extra connections: mkvass restricts to single connection.
long net_host_cutoff(const char *url);

// 1 when LAST net_download_range_st ON THIS THREAD failed because subsequent chunk
// (remainder after cut or ceiling chunk) returned 0 bytes without timing out:
// server refused to continue (#92, v1.4.7: Real-Debrid while playing).
// Read right after call, on the same thread.
int net_tail_refused(void);

// Wire byte cap of current transfer (0 = uncapped). Internal to module;
// exposed only because net_download_range uses it. Do not touch from outside.
#if defined(NV_TPK40)
long *net_cap_ptr(void);
#define net_cap (*net_cap_ptr())
#elif defined(__GNUC__)
extern _Thread_local long net_cap;
#else
extern long net_cap;
#endif

// Follows redirects with 64-byte GET Range and returns FINAL address only on HTTP 2xx,
// without truncating destination. Native discards body and aborts past 64 bytes;
// synchronous XHR on WGT receives entire response but does not copy to WASM heap
// (cannot abort during receive). Determines whether debrid link leads to file or
// placeholder video ("downloading.mp4", "slate.mp4") — which PLAYS NORMALLY without error.
// Returns 1 if resolved successfully.
int net_url_final(const char *url, int seconds, char *dst, unsigned size);

// Same, with headers required by addon and final HTTP status (0 when no response).
// Transport failure, non-2xx HTTP, or small destination leave dst empty. Caller in WGT
// must distinguish browser-controlled header refusal from dead source: AVPlay can send it.
int net_url_final_headers(const char *url, int seconds, const char *const *headers,
                          char *dst, unsigned size, int *status);

// SAME REQUEST, also returning what the final response reports: `mime`
// (Content-Type without parameters, lowercase; empty when omitted) and `body`
// (body bytes when response was complete 200 of up to 64 B, or -1 when undetermined:
// 206, truncated body). Any output pointer may be NULL. No extra request: data from
// same probe. Used by naovideo.h (warning link returning HTML/JSON instead of video).
int net_url_final_type(const char *url, int seconds, const char *const *headers,
                       char *dst, unsigned size, int *status,
                       char *mime, unsigned mime_size, long *body);

// JSON POST. Exists for Trakt, which only accepts writes via POST.
char *net_post(const char *url, int seconds, const char *const *headers,
               const char *body);

// Same, returning HTTP CODE in *status (0 when request never left).
// Exists for Supabase: 401 is not a failure, but instruction to refresh token and retry.
// Without code in hand, "expired session" and "server down" appear identical — NULL —
// and app either loses session needlessly or loops retrying an error that will not pass.
// Error body is also returned: PostgREST explains in body which function or table is
// missing, distinguishing "old server" from "bad parameter".
// DELETE with response body (often empty: 204). Returns NULL only on TRANSPORT failure;
// 4xx returns body and status in `*status`, as in net_post_st. Exists because Trakt
// deletes resume items via DELETE /sync/playback/:id and responds 404 to same path on POST.
char *net_delete(const char *url, int seconds, const char *const *headers,
                 int *status);
char *net_post_st(const char *url, int seconds, const char *const *headers,
                  const char *body, int *status);

// GET with headers AND HTTP code. For same reason as POST above: Supabase table reading
// must distinguish "table does not exist" (404 with PGRST205 in body) from "no network",
// as former means falling back to next table while latter means doing nothing.
//
// NOTE: unlike net_download_with, this DOES NOT convert 4xx into NULL.
// Error body is precisely what caller wants to read.
char *net_download_st(const char *url, int seconds, const char *const *headers,
                      int *status);

// Same, returning Retry-After in seconds (delta or HTTP-date; 0 = absent, invalid,
// expired, or hidden by CORS on WGT). For provider 429 responses.
char *net_download_st_retry(const char *url, int seconds, const char *const *headers,
                            int *status, int *retry_after);

// GET with per-request measurement. Does not share cap, state, or accumulators with
// other requests; diagnostic probe can call in series or on different threads without mixing numbers.
char *net_download_measured(const char *url, int seconds,
                            const char *const *headers, NetMeasurement *measurement);
char *net_download_measured_control(const char *url, int seconds,
                                    const char *const *headers,
                                    const NetControl *control,
                                    NetMeasurement *measurement);
char *net_download_bin_measured_control(const char *url, int seconds,
                                        const char *const *headers,
                                        const NetControl *control,
                                        long *size, NetMeasurement *measurement);

// Same, and COPIES RESPONSE ETag to `etag` (empty when server sent none).
//
// WHY SEPARATE FUNCTION: No other app path needed RESPONSE headers — status was enough.
// Recommendations service polls once per minute while app is open, and without ETag each
// probe would retrieve entire list only to find nothing changed. With ETag, common case
// is 304 without body: returned `etag` goes into `If-None-Match` of next request; value
// is OPAQUE to client — simply returns what was received.
//
// On 304 body is NULL and `*status` is 304; caller distinguishes this from transport failure (*status == 0).
char *net_download_etag(const char *url, int seconds,
                        const char *const *headers, int *status,
                        char *etag, unsigned etag_size);

// STREAM THROUGHPUT: downloads `url` from byte `start` for up to `seconds`
// (counted from FIRST BYTE of body, not connection) or `max_bytes`, and DISCARDS BYTES —
// nothing accumulated in memory, which TV lacks. Exists for diagnostic speed test (vazao.h).
//
// `kbps_per_second` receives sample per FULL SECOND of body (up to `n_max`); a second where
// nothing arrived is 0, which is the exact stall calculation needs to see. Measurement
// shorter than 1s (file ended) becomes single sample. Returns number of samples written.
//
// `headers` as in net_download_with (addon proxyHeaders); `final` (optional) receives
// address after redirects. `cancelled` is checked during reception.
//
// ON TIZEN cannot count bytes while arriving (synchronous XHR delivers whole body at once):
// uses increasing Range chunks, each discarded in JavaScript and timed; each chunk becomes
// as many samples as seconds it took. CDN without CORS for app origin (debrid) appears
// as status 0 error -1: caller reports "could not measure via browser" without guessing.
typedef struct NetThroughput {
  int status;                               // final response HTTP; 0 = no response
  union { int error; int erro; };           // libcurl (0 = ok); -1 on Tizen: browser refused
  long long bytes;                          // received (and discarded)
  unsigned long ms;                         // from first byte to end of measurement
  union { unsigned long wait_ms; unsigned long esperaMs; }; // from request to first byte
  union { int cancelled; int cancelado; };
} NetThroughput;

int net_measure_throughput(const char *url, const char *const *headers, int seconds,
                           long start, long long max_bytes, volatile int *cancelled,
                           int *kbps_per_second, int n_max, NetThroughput *res,
                           char *final, unsigned final_size);

// Timeout in ms for FIRST body byte (DNS, TLS, redirects, and server wait) in upcoming
// measurements; measurement gives up once exceeded. 0 reverts to default (8s).
// Full cycle downloads multiple sources in queue and stalled source must not block queue.
// Only libcurl honors this (Tizen sync XHR lacks timeout).
void net_throughput_wait(unsigned long ms);

// Registers listener for 401. Without this, expired session token was only a log line —
// Trakt remained "connected" on screen while every response returned 401. Callback receives
// URL and decides if refusal belongs to it.
void net_notify_401(void (*fn)(const char *url));

// Registers listener for completion of each request for network health (net_health.h:
// "no internet" / "internet back" on island). Callback receives CURLcode (0 = ok;
// on Tizen-wasm 0 = answered, 6 = no response) and URL, running on request thread.
// Function pointer instead of direct call so tests compiling net.c standalone do not need module.
void net_notify_health(void (*fn)(int code, const char *url));

// Each completed request, with host, CURLcode (0 = transport ok), HTTP, and ms elapsed
// (0 = not measured on wasm). Used by Network tab of log panel for requests, failures,
// and typical time per host (net_health.h: net_hosts_*). Single listener; called from any thread.
void net_notify_host(void (*fn)(const char *url, int code, int http, unsigned ms));

// Loads libcurl NOW on calling thread. Exists so startup can execute on main thread
// before any network thread is spawned: `curl_global_init` is not thread-safe, and
// internal lock is secondary defense, not primary. Calling multiple times has no overhead.
void net_prepare(void);

// SAFE URL FOR LOG: scheme and host only, path stripped.
//
// WHY THIS EXISTS: Debrid key travels in PATH of addon URLs ("https://host/manifest/<id>/<jwt>",
// "https://host/d/<key>/file.mkv"), and app PRINTS these URLs at various points.
// Destination is no longer just development file:
//   - on webOS goes to /tmp/nuvio.log, readable by any process;
//   - on Tizen goes to browser console and, with NUVIO_LOG_URL, exits device via network;
//   - and appears ON TV SCREEN via red key log panel.
// Stripping at display time protects room, not log file. Stripping at SOURCE protects both,
// and host — useful for diagnosing which addon responded — remains in log.
// Writes to `dst` and returns `dst`. Text without "://" copied as is.
// Transport failure string from LAST request on this thread ("curl 35: ..."), or "" when passed.
// No URL, body or secret; used for screen and log (#223).
const char *net_last_error(void);

// CONNECTION WARMUP (2.0.2). Opens DNS + TCP + TLS for each origin ("https://host")
// with a HEAD on "/" (no redirect follow, no body, never a stream path) and keeps handle
// pooled for next thread requesting these hosts. ms[i] receives time per origin (0 = failed).
// Returns number opened. Call from disposable thread; no-op on .wgt (returns 0).
int net_warmup_batch(const char *const *origins, int n, unsigned *ms);
const char *net_url_public(const char *url, char *dst, unsigned size);

// IMAGE/META URL FOR LOG: keeps scheme, host, and path segments that cannot be credentials
// (e.g., /t/p/w500/abc.jpg from TMDB), replacing suspicious segments (>= 24 chars, UUID,
// JWT, serialized config) and query with <redacted>. Hosts storing user config/key in PATH
// retain only host and title ID.
const char *net_url_log(const char *url, char *dst, unsigned size);
int net_segment_is_suspicious(const char *s, unsigned n);

// RAW TLS CONNECTION, without HTTP. For Discord websocket (discordws.c):
// TV libcurl (7.53.1) lacks websocket support, but opens TLS and delivers channel
// with CONNECT_ONLY. `url` is "https://host:port/" — only host and port matter.
// Blocks until connected or `seconds` expires. NULL = no connection. Always NULL in browser.
// Startup only: optional trusted CA bundle for Discord OAuth and Gateway.
// Empty uses libcurl system trust; never disables certificate verification.
void net_discord_ca(const char *pem_path);
// OAuth POST: verified TLS, dedicated handle, no native redirects.
char *net_post_secure_st(const char *url, int seconds, const char *const *headers,
                         const char *body, int *status);

typedef struct NetTls NetTls;
NetTls *net_tls_open(const char *url, int seconds);
int net_tls_send(NetTls *t, const void *buf, size_t n);
int net_tls_try_send(NetTls *t, const void *buf, size_t n, size_t *sent);
int net_tls_recv(NetTls *t, void *buf, size_t n, int wait_ms);
void net_tls_close(NetTls *t);

// ===========================================================================
// Backward-compatibility wrappers & aliases for Portuguese names
// ===========================================================================

typedef NetGroup RedeGrupo;
typedef NetJob RedeJob;
typedef NetError RedeErro;

#define REDE_OK NET_OK
#define REDE_ENTRADA NET_INPUT
#define REDE_INDISPONIVEL NET_UNAVAILABLE
#define REDE_MEMORIA NET_MEMORY
#define REDE_TRANSPORTE NET_TRANSPORT
#define REDE_PRAZO NET_TIMEOUT
#define REDE_CANCELADO NET_CANCELLED
#define REDE_GERACAO NET_GENERATION
#define REDE_LIMITE_CORPO NET_BODY_LIMIT
#define REDE_LIMITE_CABECALHOS NET_HEADER_LIMIT
#define REDE_REDIRECT NET_REDIRECT

#define REDE_CORPO_PADRAO NET_BODY_DEFAULT
#define REDE_CORPO_MAXIMO NET_BODY_MAX
#define REDE_CAB_PADRAO NET_HDR_DEFAULT
#define REDE_CAB_MAXIMO NET_HDR_MAX
#define REDE_CAP_CORPO NET_CAP_BODY
#define REDE_CAP_JOB NET_CAP_JOB
#define REDE_CAP_REDIRECT NET_CAP_REDIRECT
#define REDE_CAP_INTERVALO NET_CAP_INTERVAL

typedef NetInterval RedeIntervalo;
typedef NetRequest RedePedido;
typedef NetResponse RedeResposta;
typedef NetMeasurement RedeMedida;
typedef NetControl RedeControle;
typedef NetThroughput RedeVazao;
typedef NetTls RedeTls;

#if defined(NV_TPK40)
#define rede_teto_ptr net_cap_ptr
#define rede_teto net_cap
#elif defined(__GNUC__)
#define rede_teto net_cap
#else
#define rede_teto net_cap
#endif

unsigned rede_pedido_capacidades(void);
int rede_pedir(const RedePedido *p, RedeResposta *r);
void rede_resposta_limpar(RedeResposta *r);

RedeGrupo *rede_grupo_criar(void);
void rede_grupo_soltar(RedeGrupo *g);
uint64_t rede_grupo_avancar(RedeGrupo *g);
void rede_grupo_cancelar(RedeGrupo *g);
RedeJob *rede_job_criar(RedeGrupo *g);
void rede_job_reter(RedeJob *j);
void rede_job_soltar(RedeJob *j);
void rede_job_cancelar(RedeJob *j);
RedeErro rede_job_estado(RedeJob *j);

char *rede_baixar_trecho64_final(const char *url, const char *headers,
                                 int64_t start, int64_t end, long *size,
                                 int64_t *total, int *status,
                                 volatile int *cancelled, char *final, int *cross);
char *rede_baixar_trecho64_cab(const char *url, const char *headers,
                               int64_t start, int64_t end, long *size,
                               int64_t *total, int *status,
                               volatile int *cancelled);
char *rede_baixar(const char *url, int s);
char *rede_baixar_bin(const char *url, int s, long *n);
#ifdef __EMSCRIPTEN__
char *rede_postar_bin(const char *u, int s, const char *c, long *n);
#endif
char *rede_baixar_com(const char *u, int s, const char *const *h);
char *rede_baixar_trecho(const char *u, int s, long ini, long fim, long *tam);
char *rede_baixar_trecho_st(const char *u, int s, long long ini, long long fim,
                            long *tam, int *st, int *err, char *fin, unsigned tf);
long rede_corte_host(const char *url);
int rede_resto_recusado(void);
int rede_url_final(const char *u, int s, char *d, unsigned t);
int rede_url_final_cab(const char *u, int s, const char *const *h, char *d, unsigned t, int *st);
int rede_url_final_tipo(const char *u, int s, const char *const *h, char *d, unsigned t, int *st,
                        char *m, unsigned mt, long *c);
char *rede_postar(const char *u, int s, const char *const *h, const char *c);
char *rede_apagar(const char *u, int s, const char *const *h, int *st);
char *rede_postar_st(const char *u, int s, const char *const *h, const char *c, int *st);
char *rede_baixar_st(const char *u, int s, const char *const *h, int *st);
char *rede_baixar_st_retry(const char *u, int s, const char *const *h, int *st, int *ra);
char *rede_baixar_medido(const char *u, int s, const char *const *h, RedeMedida *m);
char *rede_baixar_medido_controle(const char *u, int s, const char *const *h,
                                  const RedeControle *c, RedeMedida *m);
char *rede_baixar_bin_medido_controle(const char *u, int s, const char *const *h,
                                      const RedeControle *c, long *t, RedeMedida *m);
char *rede_baixar_etag(const char *u, int s, const char *const *h, int *st, char *e, unsigned te);
int rede_medir_vazao(const char *u, const char *const *h, int s, long ini, long long mb,
                     volatile int *c, int *kps, int nm, RedeVazao *r, char *f, unsigned tf);
void rede_vazao_espera(unsigned long ms);
void rede_avisar_401(void (*f)(const char *url));
void rede_avisar_saude(void (*f)(int codigo, const char *url));
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms));
void rede_preparar(void);
const char *rede_ultimo_erro(void);
int rede_aquecer_lote(const char *const *origens, int n, unsigned *ms);
const char *rede_url_publica(const char *url, char *dst, unsigned tam);
const char *rede_url_log(const char *url, char *dst, unsigned tam);
int rede_segmento_suspeito(const char *s, unsigned n);
void rede_discord_ca(const char *p);
char *rede_postar_seguro_st(const char *u, int s, const char *const *h, const char *c, int *st);
RedeTls *rede_tls_abrir(const char *u, int s);
int rede_tls_enviar(RedeTls *t, const void *b, size_t n);
int rede_tls_tentar_enviar(RedeTls *t, const void *b, size_t n, size_t *f);
int rede_tls_receber(RedeTls *t, void *b, size_t n, int e);
void rede_tls_fechar(RedeTls *t);

#endif
