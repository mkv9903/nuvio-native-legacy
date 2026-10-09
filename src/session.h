// The account: tokens on disk, TV code login, refresh and logout.
//
// This is the module that replaces the owner's secret files (art/trakt.txt,
// art/addons.txt) with a real session. Without this module, the .ipk
// cannot be distributed: it delivers the Trakt token and debrid keys of
// whoever built it.
//
// FLOW, read from js/core/auth/qrLoginService.js. Not the email and password
// login of web, and not by preference: the search keyboard only has "a-z0-9",
// no uppercase, no "@" and no dot. An email is not typable in this app today.
//   1. ANONYMOUS session (POST /auth/v1/signup; if rejected, /auth/v1/token?
//      grant_type=anonymous). Without it the server responds 401 to the two
//      subsequent RPCs.
//   2. start_tv_login_session -> returns a CODE.
//   3. The user opens the URL on mobile and enters the code.
//   4. poll_tv_login_session until authorized.
//   5. /functions/v1/tv-logins-exchange -> access_token + refresh_token.
//
// Step 1 is the most surprising: it looks like login starts from scratch, but
// the TV already needs to be authenticated as SOMEONE to request a code.
#ifndef NV_SESSION_H
#define NV_SESSION_H

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
const char  *session_code(void);            // code to display; "" outside flow
const char  *session_login_url(void);       // URL to display
const char  *session_error(void);           // last failure, for screen display
const char  *session_user(void);            // `sub` of JWT; "" when logged out

// RAW ACCESS TOKEN, for callers that need to authenticate with a server that IS
// NOT Nuvio's Supabase. Today only the recommendations service (recomenda.c),
// which sends `Authorization: Bearer <this>` and asks Supabase itself who it
// belongs to — which is why it needs the token and not the `sub`.
//
// Returns "" when there is no USER session: an anonymous session (see step 1
// of flow above) has a token but no owner, and sending that token to another
// server would create a new "person" on every startup. Caller must never
// write the value to disk or logs.
const char *session_token(void);

// Begins the login flow on its own thread (blocking calls). Idempotent while
// a flow is in progress.
void session_login_start(void);

// Performs one step of the flow. Call once per frame; non-blocking. This is
// where poll is rescheduled — no hidden timers inside the module.
void session_step(unsigned now_ms);

void session_cancel(void);

// EMAIL AND PASSWORD LOGIN (#216), identical to web app (authManager.js
// signInWithEmail): POST /auth/v1/token?grant_type=password with anonymous
// key. The session is saved IDENTICAL to QR (sessao.txt). Interrupts QR flow.
// Non-blocking: result appears in session_state() (LOGGED_IN) or in
// session_error_email() when returning to LOGGED_OUT. Email, password and token
// NEVER go to logs; internal copies are zeroed immediately after request.
void session_login_email(const char *email, const char *password);
// Last failure of email login, already translated; "" without failure.
const char *session_error_email(void);

// Pure, for tests/sessao_email.c: screen phrase for an error response
// from /auth/v1/token (status + body). Writes to dst.
void session_email_error_from(int status, const char *body, char *dst, unsigned size);

// Deletes tokens from disk and memory. After this the app returns to having no
// account at all — which is what the user expects from "logout".
void session_logout(void);

// AUTHENTICATED RPC as the user. Returns body (free by caller) or NULL.
// On 401 refreshes token and retries ONCE; if refresh fails, session drops
// to SESSION_LOGGED_OUT instead of staying in limbo where app appears logged in
// and nothing syncs. BLOCKS — call from sync thread, never draw loop.
char *session_rpc(const char *function_name, const char *body_json, int *status);

// Same thing for /functions/v1/<name>.
char *session_function(const char *name, const char *body_json, int *status);

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
const char *sessao_token(void);
void sessao_login_comecar(void);
void sessao_passo(unsigned agoraMs);
void sessao_cancelar(void);
void sessao_login_email(const char *email, const char *senha);
const char *sessao_erro_email(void);
void sessao_email_erro_de(int status, const char *corpo, char *dst, unsigned tam);
void sessao_sair(void);
char *sessao_rpc(const char *funcao, const char *corpoJson, int *status);
char *sessao_funcao(const char *nome, const char *corpoJson, int *status);
char *sessao_tabela(const char *tabela, const char *consulta, int *status);

#endif
