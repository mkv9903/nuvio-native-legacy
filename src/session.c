#include "session.h"
#include "idioma.h"
#include "nuvem.h"
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define SESSION_FILE "sessao.txt"
// MEASURED: the start response itself brings `poll_interval_seconds` (3 on
// today's server). This value is just the fallback when absent — the actual
// interval comes from the server, which knows the load it can handle.
#define POLL_MS          3000
// After this time the server code expires anyway; keeping polling only drains
// battery and shows a dead code on screen.
#define LOGIN_TIMEOUT_MS 600000

static char access_token[3000];
static char refresh_token[3000];
static char sub[80];
static int  is_anonymous;          // 1 when token belongs to anonymous session
static long expires_at;            // `exp` from JWT, in seconds

static SessionState state = SESSION_LOGGED_OUT;
static char code[64];
static char login_url[400];
static char nonce[64];
static char error_msg[240];

static unsigned poll_ms = POLL_MS;

// Email login (#216). Both texts live only until request is dispatched.
static char email_pending[256], password_pending[256];
static int  email_requested;       // request stored until QR thread finishes
static char email_error[240];

static pthread_t thread;
static int thread_alive;
static int step_done;              // thread finished; next step can run
static unsigned next_poll;
static unsigned login_start_ms;

// ---------------------------------------------------------------- JWT

// base64url -> bytes. Only what is needed to read the JWT payload: neither
// validates signature nor attempts generality. An invalid JWT here becomes
// "no sub and no exp", which callers already handle.
static int b64url(const char *s, size_t n, char *dst, size_t size) {
  static const char *tab =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  unsigned acc = 0;
  int bits = 0;
  size_t i, w = 0;
  for (i = 0; i < n; i++) {
    const char *p = strchr(tab, s[i]);
    if (!p) continue;
    acc = (acc << 6) | (unsigned)(p - tab);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (w + 1 >= size) return 0;
      dst[w++] = (char)((acc >> bits) & 0xFF);
    }
  }
  dst[w] = 0;
  return 1;
}

// Reads `sub` and `exp` from payload. Without this the app does not know who
// owns the session or when it expires, discovering only upon 401 — after the
// operation has already failed once.
static void parse_jwt(const char *token) {
  const char *p1, *p2;
  char payload[2200];
  sub[0] = 0;
  expires_at = 0;
  if (!token || !*token) return;
  p1 = strchr(token, '.');
  if (!p1) return;
  p2 = strchr(p1 + 1, '.');
  if (!p2) return;
  if (!b64url(p1 + 1, (size_t)(p2 - p1 - 1), payload, sizeof payload)) return;
  {
    const char *end = payload + strlen(payload);
    js_texto(payload, end, "sub", sub, sizeof sub);
    expires_at = (long)js_num(payload, end, "exp", 0);
  }
}

// JWT format check (three non-empty parts, no spaces): format only, signature
// belongs to server. Rejects corrupted data saved by an aborted login.
static int is_valid_jwt(const char *t) {
  const char *p1, *p2;
  if (!t || !*t || strpbrk(t, " \t\r\n")) return 0;
  p1 = strchr(t, '.');
  if (!p1 || p1 == t) return 0;
  p2 = strchr(p1 + 1, '.');
  if (!p2 || p2 == p1 + 1 || !p2[1]) return 0;
  return 1;
}

// 30s margin, identical to web: a token expiring mid-request returns 401 and
// costs a full roundtrip.
static int is_expired(void) {
  if (!access_token[0]) return 1;
  if (!expires_at) return 0;   // without readable exp, only server can tell
  return expires_at <= (long)time(NULL) + 30;
}

// SERVER FAILURE, not user error (#215): no response, 429 or 5xx. In these
// cases nothing on the TV is invalid — token, refresh token, code — and
// deleting any would turn an outage into an unrecoverable logged-out state.
static int is_server_failure(int st) { return st == 0 || st == 429 || st >= 500; }

// Screen phrase for "account server did not respond". i18n on FORMAT, like
// the rest of this file.
// Without HTTP (st == 0) cause is transport; libcurl text ("curl 35: ...") is
// appended to screen and log because release builds hide stderr (#223).
static void append_transport_error(char *dst, unsigned size, int st) {
  const char *t = nuvem_ultimo_erro();
  size_t n = strlen(dst);
  if (st == 0 && t && t[0] && n + 4 < size) snprintf(dst + n, size - n, " [%s]", t);
}

static void set_server_error(int st) {
  if (st) snprintf(error_msg, sizeof error_msg,
                   i18n("O servidor da conta Nuvio não respondeu (HTTP %d). Tente de novo em alguns minutos."), st);
  else    snprintf(error_msg, sizeof error_msg, "%s",
                   i18n("O servidor da conta Nuvio não respondeu. Tente de novo em alguns minutos."));
  append_transport_error(error_msg, sizeof error_msg, st);
}

// ---------------------------------------------------------------- disk

static void save_session(void) {
  char buf[6400];
  snprintf(buf, sizeof buf, "%s\n%s\n%d\n", access_token, refresh_token, is_anonymous ? 1 : 0);
  dados_gravar(SESSION_FILE, buf);
}

static void clear_session(void) {
  access_token[0] = refresh_token[0] = sub[0] = 0;
  is_anonymous = 0;
  expires_at = 0;
}

// ---------------------------------------------------------------- responses

// Stores tokens from an auth response. Handles three formats:
// /auth/v1/signup, /auth/v1/token, and exchange function — all return
// access_token/refresh_token, either at root or nested under "session".
static int store_tokens(const char *body, int anonymous_flag) {
  const char *end, *ses;
  char a[3000], r[3000];
  if (!body) return 0;
  end = body + strlen(body);
  a[0] = r[0] = 0;
  js_texto(body, end, "access_token", a, sizeof a);
  js_texto(body, end, "refresh_token", r, sizeof r);
  if (!a[0]) {
    ses = strstr(body, "\"session\"");
    if (ses) {
      js_texto(ses, end, "access_token", a, sizeof a);
      js_texto(ses, end, "refresh_token", r, sizeof r);
    }
  }
  if (!a[0] || !is_valid_jwt(a)) return 0;
  // ATOMIC (#223): existing tokens only replaced when new response is complete.
  snprintf(access_token, sizeof access_token, "%s", a);
  snprintf(refresh_token, sizeof refresh_token, "%s", r);
  is_anonymous = anonymous_flag ? 1 : 0;
  parse_jwt(access_token);
  save_session();
  return 1;
}

// ---------------------------------------------------------------- anonymous

static int anonymous_session(void) {
  char *resp;
  int st = 0;
  // Same body as web: `data` marks session origin for server telemetry.
  resp = nuvem_post("/auth/v1/signup",
                    "{\"data\":{\"tv_client\":\"webos\"}}", NULL, &st);
  if (resp && st >= 200 && st < 300 && store_tokens(resp, 1)) { free(resp); return 1; }
  free(resp);
  // Anonymous signup may be disabled on project; dedicated grant is fallback.
  resp = nuvem_post("/auth/v1/token?grant_type=anonymous", "{}", NULL, &st);
  if (resp && st >= 200 && st < 300 && store_tokens(resp, 1)) { free(resp); return 1; }
  printf("[session] anonymous session: HTTP %d %s\n", st, nuvem_ultimo_erro());
  if (is_server_failure(st)) set_server_error(st);
  else snprintf(error_msg, sizeof error_msg, "sessao anonima recusada (HTTP %d)", st);
  free(resp);
  return 0;
}

// ---------------------------------------------------------------- refresh

// HTTP status of last refresh, so caller gets real cause (504 on refresh != 401 on RPC).
static int refresh_status;

// 1 = refreshed; 0 = rejected (session terminated); -1 = server unresponsive (session KEPT).
static int refresh_token_call(void) {
  Jsw w;
  char *resp;
  int st = 0, ok;
  refresh_status = 0;
  if (!refresh_token[0]) return 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "refresh_token", refresh_token);
  jsw_obj_fim(&w);
  resp = nuvem_post("/auth/v1/token?grant_type=refresh_token",
                    jsw_texto_final(&w), NULL, &st);
  jsw_livre(&w);
  ok = (resp && st >= 200 && st < 300 && store_tokens(resp, is_anonymous));
  free(resp);
  refresh_status = st;
  if (!ok && is_server_failure(st)) {
    // SERVER OUTAGE, NOT INVALID TOKEN (#215). Only explicit 4xx terminates session.
    printf("[session] refresh without response from server (HTTP %d): session retained\n", st);
    nuvem_falhou();
    return -1;
  }
  if (!ok) {
    // Rejected refresh terminates session permanently.
    printf("[session] refresh rejected (HTTP %d): logging out\n", st);
    clear_session();
    dados_apagar(SESSION_FILE);
    state = SESSION_LOGGED_OUT;
  }
  return ok;
}

// ---------------------------------------------------------------- RPC

static char *invoke_cloud(const char *path, const char *body, int *status) {
  char *resp;
  int st = 0;
  if (!nuvem_pronta()) { if (status) *status = 0; return NULL; }
  // Expired token and server unresponsive on refresh: don't attempt RPC.
  if (is_expired() && refresh_token[0] && refresh_token_call() < 0) {
    if (status) *status = refresh_status;
    return NULL;
  }
  resp = nuvem_post(path, body, access_token, &st);
  if (st == 401 && refresh_token[0]) {
    int rr;
    free(resp);
    rr = refresh_token_call();
    if (rr <= 0) { if (status) *status = rr < 0 ? refresh_status : 401; return NULL; }
    resp = nuvem_post(path, body, access_token, &st);
  }
  if (status) *status = st;
  // 429 also triggers backoff.
  if (!resp || st == 0 || st == 429 || st >= 500) nuvem_falhou();
  else nuvem_ok();
  return resp;
}

char *session_rpc(const char *function_name, const char *body_json, int *status) {
  char path[300];
  if (!function_name || !*function_name) return NULL;
  snprintf(path, sizeof path, "/rest/v1/rpc/%s", function_name);
  return invoke_cloud(path, body_json, status);
}

char *session_table(const char *table_name, const char *query, int *status) {
  char *resp;
  int st = 0;
  if (!nuvem_pronta()) { if (status) *status = 0; return NULL; }
  if (is_expired() && refresh_token[0] && refresh_token_call() < 0) {
    if (status) *status = refresh_status;
    return NULL;
  }
  resp = nuvem_tabela(table_name, query, access_token, &st);
  if (st == 401 && refresh_token[0]) {
    int rr;
    free(resp);
    rr = refresh_token_call();
    if (rr <= 0) { if (status) *status = rr < 0 ? refresh_status : 401; return NULL; }
    resp = nuvem_tabela(table_name, query, access_token, &st);
  }
  if (status) *status = st;
  if (!resp || st == 0 || st == 429 || st >= 500) nuvem_falhou();
  else nuvem_ok();
  return resp;
}

char *session_function(const char *name, const char *body_json, int *status) {
  char path[300];
  if (!name || !*name) return NULL;
  snprintf(path, sizeof path, "/functions/v1/%s", name);
  return invoke_cloud(path, body_json, status);
}

// ---------------------------------------------------------------- login

// Steps 1+2 on thread: anonymous session and code request.
static void *thread_request(void *u) {
  Jsw w;
  char *resp;
  int st = 0;
  (void)u;
#ifdef NV_TPK40
  nv_tpk40_etapa("note sign-in-thread started");
#endif
  error_msg[0] = 0;
  code[0] = 0;
  login_url[0] = 0;

  if (!access_token[0] || (is_anonymous && is_expired())) {
    if (!anonymous_session()) { state = SESSION_ERROR; step_done = 1; return NULL; }
  }

  // MEASURED against server: empty `p_redirect_base_url` returns 400.
  if (!nuvem_base_login()[0]) {
    snprintf(error_msg, sizeof error_msg, "pacote sem endereco de login configurado");
    state = SESSION_ERROR;
    step_done = 1;
    return NULL;
  }

  // Nonce MUST be a UUID.
  dados_uuid(nonce, sizeof nonce);

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "p_device_nonce", nonce);
  jsw_cs(&w, "p_redirect_base_url", nuvem_base_login());
  jsw_cs(&w, "p_device_name", "LG webOS (Nuvio native)");
  jsw_obj_fim(&w);
  resp = nuvem_rpc_com("start_tv_login_session", jsw_texto_final(&w), access_token, &st);

  // Older servers reject p_device_name; retry without it.
  if (resp && nuvem_erro_ausente(resp)) {
    free(resp);
    jsw_livre(&w);
    jsw_iniciar(&w);
    jsw_obj_ini(&w);
    jsw_cs(&w, "p_device_nonce", nonce);
    jsw_cs(&w, "p_redirect_base_url", nuvem_base_login());
    jsw_obj_fim(&w);
    resp = nuvem_rpc_com("start_tv_login_session", jsw_texto_final(&w), access_token, &st);
  }
  jsw_livre(&w);

  if (resp && st >= 200 && st < 300) {
    const char *end = resp + strlen(resp);
    double interval;
    js_texto(resp, end, "code", code, sizeof code);
    js_texto(resp, end, "web_url", login_url, sizeof login_url);
    interval = js_num(resp, end, "poll_interval_seconds", 0);
    if (interval >= 1.0 && interval <= 60.0) poll_ms = (unsigned)(interval * 1000.0);
  }
  if (!code[0]) {
    char msg[160];
    msg[0] = 0;
    if (resp) js_texto(resp, resp + strlen(resp), "message", msg, sizeof msg);
    printf("[session] code request: HTTP %d %s: %.160s\n", st, nuvem_ultimo_erro(), resp ? resp : "(sem corpo)");
    if (is_server_failure(st)) set_server_error(st);
    else if (msg[0]) snprintf(error_msg, sizeof error_msg, "o servidor recusou: %s", msg);
    else snprintf(error_msg, sizeof error_msg,
                  i18n("O servidor da conta Nuvio recusou o pedido do código (HTTP %d). Tente de novo em alguns minutos."), st);
    if (!email_requested && state != SESSION_EMAIL) state = SESSION_ERROR;
  } else {
    if (!login_url[0])
      snprintf(login_url, sizeof login_url, "%s?code=%s", nuvem_base_login(), code);
    if (!email_requested && state != SESSION_EMAIL) state = SESSION_WAITING;
  }
  free(resp);
  step_done = 1;
  return NULL;
}

// Step 3: poll until authorized; exchange for token.
static void *thread_poll(void *u) {
  Jsw w;
  char *resp;
  int st = 0;
  char status[48];
  (void)u;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "p_code", code);
  jsw_cs(&w, "p_device_nonce", nonce);
  jsw_obj_fim(&w);
  resp = nuvem_rpc_com("poll_tv_login_session", jsw_texto_final(&w), access_token, &st);
  jsw_livre(&w);

  status[0] = 0;
  if (resp && st >= 200 && st < 300)
    js_texto(resp, resp + strlen(resp), "status", status, sizeof status);
  free(resp);

  if (status[0] && strcmp(status, "pending") && strcmp(status, "expired") &&
      strcmp(status, "cancelled")) {
    state = SESSION_EXCHANGING;
    jsw_iniciar(&w);
    jsw_obj_ini(&w);
    jsw_cs(&w, "code", code);
    jsw_cs(&w, "device_nonce", nonce);
    jsw_obj_fim(&w);
    resp = nuvem_post("/functions/v1/tv-logins-exchange", jsw_texto_final(&w),
                      access_token, &st);
    jsw_livre(&w);
    if (resp && st >= 200 && st < 300 && store_tokens(resp, 0)) {
      state = SESSION_LOGGED_IN;
      code[0] = 0;
      printf("[session] logged in as %s\n", sub[0] ? sub : "(sem sub)");
    } else if (is_server_failure(st)) {
      printf("[session] code exchange without response from server (HTTP %d): retrying\n", st);
      state = SESSION_WAITING;
    } else {
      printf("[session] code exchange rejected (HTTP %d)\n", st);
      snprintf(error_msg, sizeof error_msg, i18n("troca de codigo recusada (HTTP %d)"), st);
      state = SESSION_ERROR;
    }
    free(resp);
  } else if (!strcmp(status, "expired") || !strcmp(status, "cancelled")) {
    snprintf(error_msg, sizeof error_msg, "o código expirou");
    state = SESSION_ERROR;
  }
  step_done = 1;
  return NULL;
}

// ---------------------------------------------------------------- email

static void zero_memory(volatile char *p, size_t n) { while (n--) *p++ = 0; }

void session_email_error_from(int st, const char *body, char *dst, unsigned size) {
  char cod[80], desc[200];
  const char *end;
  cod[0] = desc[0] = 0;
  if (body) {
    end = body + strlen(body);
    js_texto(body, end, "error_code", cod, sizeof cod);
    if (!cod[0]) js_texto(body, end, "error", cod, sizeof cod);
    js_texto(body, end, "error_description", desc, sizeof desc);
    if (!desc[0]) js_texto(body, end, "msg", desc, sizeof desc);
    if (!desc[0]) js_texto(body, end, "message", desc, sizeof desc);
  }
  if (st == 0 || st >= 500) {
    if (st) snprintf(dst, size, i18n("O servidor da conta Nuvio não respondeu (HTTP %d). Tente de novo em alguns minutos."), st);
    else    snprintf(dst, size, "%s", i18n("O servidor da conta Nuvio não respondeu. Tente de novo em alguns minutos."));
  } else if (st == 429 || !strcmp(cod, "over_request_rate_limit") ||
             !strcmp(cod, "over_email_send_rate_limit")) {
    snprintf(dst, size, "%s", i18n("Muitas tentativas. Espere um minuto e tente de novo."));
  } else if (!strcmp(cod, "email_not_confirmed") || strstr(desc, "not confirmed")) {
    snprintf(dst, size, "%s", i18n("Este e-mail ainda não foi confirmado. Abra o link que a Nuvio mandou e tente de novo."));
  } else if (!strcmp(cod, "invalid_credentials") || !strcmp(cod, "invalid_grant") ||
             strstr(desc, "Invalid login credentials") || st == 400) {
    snprintf(dst, size, "%s", i18n("E-mail ou senha incorretos."));
  } else {
    snprintf(dst, size, i18n("O servidor da conta Nuvio recusou o login (HTTP %d)."), st);
  }
}

static void spawn_thread(void *(*routine)(void *));

static void *thread_email(void *u) {
  Jsw w;
  char *resp;
  int st = 0;
  (void)u;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "email", email_pending);
  jsw_cs(&w, "password", password_pending);
  jsw_obj_fim(&w);
  zero_memory(password_pending, sizeof password_pending);
  resp = nuvem_post("/auth/v1/token?grant_type=password", jsw_texto_final(&w), NULL, &st);
  { char *t = (char *)jsw_texto_final(&w); if (t) zero_memory(t, strlen(t)); }
  jsw_livre(&w);
  zero_memory(email_pending, sizeof email_pending);
  if (resp && st >= 200 && st < 300 && store_tokens(resp, 0)) {
    email_error[0] = 0;
    code[0] = 0;
    state = SESSION_LOGGED_IN;
    printf("[session] logged in by email as %s\n", sub[0] ? sub : "(sem sub)");
  } else {
    printf("[session] email login: HTTP %d %s\n", st, nuvem_ultimo_erro());
    session_email_error_from(st == 200 ? 0 : st, resp, email_error, sizeof email_error);
    append_transport_error(email_error, sizeof email_error, st);
    if (st >= 200 && st < 300)
      snprintf(email_error, sizeof email_error, "%s", i18n("O servidor da conta Nuvio não respondeu. Tente de novo em alguns minutos."));
    state = SESSION_LOGGED_OUT;
  }
  if (resp) { zero_memory(resp, strlen(resp)); free(resp); }
  step_done = 1;
  return NULL;
}

void session_login_email(const char *email, const char *password) {
  if (!nuvem_pronta()) {
    snprintf(email_error, sizeof email_error, "%s", i18n("Este pacote foi montado sem servidor."));
    return;
  }
  if (state == SESSION_EMAIL || email_requested) return;
  snprintf(email_pending, sizeof email_pending, "%s", email ? email : "");
  snprintf(password_pending, sizeof password_pending, "%s", password ? password : "");
  email_error[0] = 0;
  code[0] = 0;
  state = SESSION_EMAIL;
  email_requested = 1;
  session_step(0);
}

const char *session_error_email(void) { return email_error; }

static void spawn_thread(void *(*routine)(void *)) {
  if (thread_alive) return;
  step_done = 0;
  if (pthread_create(&thread, NULL, routine, NULL) == 0) {
    pthread_detach(thread);
    thread_alive = 1;
  } else {
    snprintf(error_msg, sizeof error_msg, "sem fio para falar com o servidor");
    state = SESSION_ERROR;
  }
}

void session_init(void) {
  char *buf = dados_ler(SESSION_FILE);
  char *l1, *l2, *l3;
  if (!buf) return;
  l1 = buf;
  l2 = strchr(l1, '\n');
  if (l2) { *l2++ = 0; l3 = strchr(l2, '\n'); if (l3) *l3++ = 0; } else l3 = NULL;
  snprintf(access_token, sizeof access_token, "%s", l1);
  snprintf(refresh_token, sizeof refresh_token, "%s", l2 ? l2 : "");
  is_anonymous = (l3 && atoi(l3) == 1);
  free(buf);
  parse_jwt(access_token);
  if (access_token[0] && (!is_valid_jwt(access_token) || (!is_anonymous && !sub[0]))) {
    printf("[session] invalid saved session: discarded, going to login\n");
    clear_session();
    dados_apagar(SESSION_FILE);
    return;
  }
  if (access_token[0] && !is_anonymous) {
    state = SESSION_LOGGED_IN;
    printf("[session] session restored (%s)\n", sub[0] ? sub : "sem sub");
  }
}

SessionState session_state(void)        { return state; }
int          session_is_logged_in(void) { return access_token[0] != 0 && !is_anonymous; }
const char  *session_code(void)         { return code; }
const char  *session_login_url(void)    { return login_url; }
const char  *session_error(void)        { return error_msg; }
const char  *session_user(void)         { return sub; }
const char  *session_token(void)        { return session_is_logged_in() ? access_token : ""; }

void session_login_start(void) {
  if (!nuvem_pronta()) {
    snprintf(error_msg, sizeof error_msg, "app sem configuracao de servidor");
    state = SESSION_ERROR;
    return;
  }
  if (state == SESSION_REQUESTING || state == SESSION_WAITING || state == SESSION_EXCHANGING ||
      state == SESSION_EMAIL || email_requested) return;
  error_msg[0] = 0;
  code[0] = 0;
  state = SESSION_REQUESTING;
  login_start_ms = 0;
  spawn_thread(thread_request);
}

void session_step(unsigned now_ms) {
  if (thread_alive && step_done) { thread_alive = 0; step_done = 0; }
  if (thread_alive) return;
  if (email_requested) {
    email_requested = 0;
    if (session_is_logged_in() && state == SESSION_LOGGED_IN) {
      zero_memory(email_pending, sizeof email_pending);
      zero_memory(password_pending, sizeof password_pending);
      return;
    }
    state = SESSION_EMAIL;
    spawn_thread(thread_email);
    if (!thread_alive) {
      zero_memory(email_pending, sizeof email_pending);
      zero_memory(password_pending, sizeof password_pending);
      snprintf(email_error, sizeof email_error, "%s", i18n("O servidor da conta Nuvio não respondeu. Tente de novo em alguns minutos."));
      state = SESSION_LOGGED_OUT;
    }
    return;
  }

  if (state == SESSION_WAITING) {
    if (!login_start_ms) login_start_ms = now_ms;
    if (now_ms - login_start_ms > LOGIN_TIMEOUT_MS) {
      snprintf(error_msg, sizeof error_msg, "o código expirou");
      state = SESSION_ERROR;
      code[0] = 0;
      return;
    }
    if (now_ms >= next_poll) {
      next_poll = now_ms + poll_ms;
      spawn_thread(thread_poll);
    }
  }
}

void session_cancel(void) {
  if (state == SESSION_REQUESTING || state == SESSION_WAITING ||
      state == SESSION_EXCHANGING || state == SESSION_ERROR) {
    code[0] = 0;
    error_msg[0] = 0;
    state = session_is_logged_in() ? SESSION_LOGGED_IN : SESSION_LOGGED_OUT;
  }
}

void session_logout(void) {
  clear_session();
  dados_apagar(SESSION_FILE);
  code[0] = 0;
  error_msg[0] = 0;
  state = SESSION_LOGGED_OUT;
  printf("[session] session terminated and deleted from disk\n");
}

// Backward-compatibility wrappers
void sessao_iniciar(void) { session_init(); }
SesEstado sessao_estado(void) { return (SesEstado)session_state(); }
int sessao_logada(void) { return session_is_logged_in(); }
const char *sessao_codigo(void) { return session_code(); }
const char *sessao_url_login(void) { return session_login_url(); }
const char *sessao_erro(void) { return session_error(); }
const char *sessao_usuario(void) { return session_user(); }
const char *sessao_token(void) { return session_token(); }
void sessao_login_comecar(void) { session_login_start(); }
void sessao_passo(unsigned agoraMs) { session_step(agoraMs); }
void sessao_cancelar(void) { session_cancel(); }
void sessao_login_email(const char *email, const char *senha) { session_login_email(email, senha); }
const char *sessao_erro_email(void) { return session_error_email(); }
void sessao_email_erro_de(int status, const char *corpo, char *dst, unsigned tam) {
  session_email_error_from(status, corpo, dst, tam);
}
void sessao_sair(void) { session_logout(); }
char *sessao_rpc(const char *funcao, const char *corpoJson, int *status) {
  return session_rpc(funcao, corpoJson, status);
}
char *sessao_funcao(const char *nome, const char *corpoJson, int *status) {
  return session_function(nome, corpoJson, status);
}
char *sessao_tabela(const char *tabela, const char *consulta, int *status) {
  return session_table(tabela, consulta, status);
}
