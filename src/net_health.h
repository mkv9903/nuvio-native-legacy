// NETWORK HEALTH: "offline" and "online" notifications for the clock island.
//
// Mockup approved by owner on 02/10 ("Offline" and "Connection restored" states,
// "new signal" badge). Prior to this there was no GLOBAL connectivity signal:
// net.c responds request-by-request and each module swallowed its own failure —
// home was left empty and user did not know if it was app, addon, or household internet.
// This module ONLY COUNTS: net.c informs every completed request (transport ok, or
// transport failure), and state emerges here.
//
// WHAT COUNTS AS FAILURE: only TRANSPORT — DNS, connection refused, timeout, TLS handshake
// failure, broken connection midway (CURLcode 6, 7, 28, 35, 52, 55, 56; in Tizen-wasm,
// XHR with status 0). HTTP 4xx/5xx is NOT a network failure: server replied. CANCELLED
// request (thread gave up, cap reached) counts for nothing. Local address (127.x, localhost,
// 10.x, 192.168.x — app internal proxy, local P2P server) does not count towards either:
// it responds even when internet is out.
//
// HYSTERESIS, so it does not flicker in TV corner:
//   - OFFLINE requires 3 CONSECUTIVE failures (no success in between), across at
//     least 2 distinct hosts, over at least 3 s. Slow addon only times out on single host;
//     single offline website doesn't bring down the others.
//   - RESTORED is first success after that.
//   - After restoration, 10 s grace period before being able to declare "offline" again.
//
// Thread-safe (net.c calls from any thread). Core (net_health_step) is pure and without
// internal clock, for unit tests.
#ifndef NV_NET_HEALTH_H
#define NV_NET_HEALTH_H
#include <stddef.h>

#define NET_HEALTH_FAILURES 3
#define NET_HEALTH_HOSTS    2
#define NET_HEALTH_WINDOW   3000u
#define NET_HEALTH_GRACE    10000u

typedef struct {
  union { int failures; int falhas; };             // consecutive transport failures
  union { unsigned first; unsigned primeira; };    // ms of first failure in sequence
  union { unsigned hosts[4]; };                    // distinct host hashes in sequence
  union { int num_hosts; int nHosts; };
  union { int offline; };                          // 1 = declared "offline"
  union { unsigned restored; unsigned voltou; };   // ms when restored (grace period)
  union { int restored_ok; int voltouOk; };
  union { unsigned seq; };                         // increments on state transition
} NetHealth;

// The core: request finished. `ok` 1 = transport ok, 0 = transport failure.
// `host` = host hash (0 = ignore). Returns 1 if state changed.
int  net_health_step(NetHealth *s, int ok, unsigned host, unsigned now_ms);

// For net.c: result of a request. `code` is CURLcode (0 = ok) or, in wasm,
// 0 for response and 6 for "no response". Non-transport codes (cancellation, cap) are ignored.
void net_health_note(int code, const char *url);
// Current state and transition counter.
int      net_health_is_offline(void);
unsigned net_health_seq(void);

// REQUESTS PER HOST (Network tab of log panel, 03/10). Each finished request
// counts for its host: requests, failures (transport or HTTP >= 400), and timing of last 16.
// Only HOST is stored: path might carry credentials. Local addresses excluded.
#define NET_HOSTS_MAX 24
typedef struct {
  char host[64];
  union { int requests; int pedidos; };
  union { int failures; int falhas; };
  union { unsigned typical_ms; unsigned tipicoMs; }; // median of recent; 0 = unmeasured
} NetHost;

void net_hosts_note(const char *url, int code, int http, unsigned ms);
// Copies up to `max` hosts, highest request count first. Returns number copied.
int  net_hosts_read(NetHost *dst, int max);

// For "offline" log send screen: consecutive transport failures and across how many hosts (up to 4),
// last host that responded, and how long ago. Returns 0 if no request answered yet in session.
int  net_health_summary(int *failures, int *hosts, char *last_host, size_t size, unsigned *ago_ms);

// Backward-compatibility shims
#define REDE_SAUDE_FALHAS   NET_HEALTH_FAILURES
#define REDE_SAUDE_HOSTS    NET_HEALTH_HOSTS
#define REDE_SAUDE_JANELA   NET_HEALTH_WINDOW
#define REDE_SAUDE_CARENCIA NET_HEALTH_GRACE
#define REDE_HOSTS_MAX      NET_HOSTS_MAX

typedef NetHealth RedeSaude;
typedef NetHost   RedeHost;

int  rede_saude_passo(RedeSaude *s, int ok, unsigned host, unsigned agoraMs);
void rede_saude_nota(int codigo, const char *url);
int  rede_saude_offline(void);
unsigned rede_saude_seq(void);
void rede_hosts_nota(const char *url, int codigo, int http, unsigned ms);
int  rede_hosts_ler(RedeHost *dst, int max);
int  rede_saude_resumo(int *falhas, int *hosts, char *ultHost, size_t tam, unsigned *haMs);

#endif
