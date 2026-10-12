// Transitional C callers for the Rust-owned account capability.
// QR and email login, session refresh, cancellation and versioned JSON
// persistence are implemented and tested in backend/. The UI reads immutable
// snapshots; blocking authenticated requests are for existing C workers only.
#ifndef NV_SESSION_H
#define NV_SESSION_H

#include <stddef.h>

typedef enum {
  SESSION_LOGGED_OUT = 0,
  SESSION_REQUESTING,      // requesting code
  SESSION_WAITING,         // code on screen, waiting for user authorization
  SESSION_EXCHANGING,      // authorized, exchanging for token
  SESSION_LOGGED_IN,
  SESSION_ERROR,
  SESSION_EMAIL            // login by email and password in progress (#216)
} SessionState;

// Backward-compatibility aliases
typedef SessionState SesEstado;
#define SES_DESLOGADO SESSION_LOGGED_OUT
#define SES_PEDINDO   SESSION_REQUESTING
#define SES_AGUARDANDO SESSION_WAITING
#define SES_TROCANDO  SESSION_EXCHANGING
#define SES_LOGADO    SESSION_LOGGED_IN
#define SES_ERRO      SESSION_ERROR
#define SES_EMAIL     SESSION_EMAIL

// Loads the saved session, if any. Call after data_init and nuvem_configurar.
void session_init(void);

SessionState session_state(void);
int          session_is_logged_in(void);    // 1 only with USER session
int          session_available(void);       // Rust account service initialized
const char  *session_code(void);            // code to display; "" outside flow
const char  *session_login_url(void);       // URL to display
const char  *session_error(void);           // last failure, for screen display
const char  *session_user(void);            // `sub` of JWT; "" when logged out

// Copy the complete user access token under the session lock. Returns 0 and
// an empty string if logged out, anonymous, or the supplied buffer is too small.
// The caller must not write the token to disk or logs. Raw shared pointers are
// unsafe while refresh runs in other threads.
int session_copy_token(char *dst, size_t size);

// Begins the login flow on its own thread (blocking calls). Idempotent while
// a flow is in progress.
void session_login_start(void);

// Performs one step of the flow. Call once per frame; non-blocking. This is
// where poll is rescheduled — no hidden timers inside the module.
void session_step(unsigned now_ms);

void session_cancel(void);

// EMAIL AND PASSWORD LOGIN (#216), identical to web app (authManager.js
// signInWithEmail): POST /auth/v1/token?grant_type=password with anonymous
// key. The session uses the same Rust JSON storage as QR. Interrupts QR flow.
// Non-blocking: result appears in session_state() (LOGGED_IN) or in
// session_error_email() when returning to LOGGED_OUT. Email, password and token
// NEVER go to logs; internal copies are zeroed immediately after request.
void session_login_email(const char *email, const char *password);
// Last failure of email login, already translated; "" without failure.
const char *session_error_email(void);

// Deletes tokens from disk and memory. After this the app returns to having no
// account at all — which is what the user expects from "logout".
void session_logout(void);

// AUTHENTICATED RPC as the user. Returns body (free by caller) or NULL.
// On 401 refreshes once and retries with a private token copy. Concurrent
// callers share one refresh. Server outages preserve the session; rejection
// logs out. BLOCKS — call from a worker, never the draw loop.
char *session_rpc(const char *function_name, const char *body_json, int *status);

// TABLE READ as user. Exists because not every surface has an RPC:
// addon list only comes from table `addons`, and reading with anonymous key
// returns 401 "permission denied for table addons" — RLS needs caller's token.
char *session_table(const char *table_name, const char *query, int *status);

// Backward-compatibility shims
void sessao_iniciar(void);
SesEstado sessao_estado(void);
int sessao_logada(void);
const char *sessao_codigo(void);
const char *sessao_url_login(void);
const char *sessao_erro(void);
const char *sessao_usuario(void);
int sessao_token_copiar(char *dst, size_t size);
void sessao_login_comecar(void);
void sessao_passo(unsigned agoraMs);
void sessao_cancelar(void);
void sessao_login_email(const char *email, const char *senha);
const char *sessao_erro_email(void);
void sessao_sair(void);
char *sessao_rpc(const char *funcao, const char *corpoJson, int *status);
char *sessao_tabela(const char *tabela, const char *consulta, int *status);

#endif
