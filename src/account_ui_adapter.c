#include "app_id.h"
#include "data.h"
#include "session.h"
#include "profiles.h"
#include "rust_account.h"
#include "addon_registry.h"
#include "home_order_runtime.h"
#include "catalog_runtime.h"
#include "nuvem.h"
#include "fileiras.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_SUPABASE_URL
#define NV_SUPABASE_URL ""
#endif
#ifndef NV_SUPABASE_ANON_KEY
#define NV_SUPABASE_ANON_KEY ""
#endif
#ifndef NV_TV_LOGIN_BASE
#define NV_TV_LOGIN_BASE ""
#endif

void fontepref_definir_perfil(int profile);
void arteesc_definir_perfil(int profile);
void plugins_perfil_mudou(void);

static RustAccount *account;
static pthread_key_t view_key;
static pthread_once_t view_key_once = PTHREAD_ONCE_INIT;
static const char *configuration_error = "The account backend could not start. Check this build's service configuration and test data directory.";

typedef struct {
  RustAccountSnapshot *snapshot;
  const RustAccountView *view;
  AccountProfile profiles[ACCOUNT_PROFILE_MAX];
  int count;
  char *user;
} ThreadView;

static void release_view(void *pointer) {
  ThreadView *cache = pointer;
  if (!cache) return;
  nuvio_account_snapshot_destroy(cache->snapshot);
  free(cache->user);
  free(cache);
}
static void create_view_key(void) { pthread_key_create(&view_key, release_view); }
static ThreadView *thread_view(void) {
  ThreadView *cache;
  pthread_once(&view_key_once, create_view_key);
  cache = pthread_getspecific(view_key);
  if (!cache) {
    cache = calloc(1, sizeof *cache);
    if (cache) pthread_setspecific(view_key, cache);
  }
  return cache;
}

/* Publish at frame/action boundaries. All pointers used during a C draw
 * remain valid throughout that frame, even while Rust publishes newer state.
 * Workers own independent snapshots and release them at thread termination. */
static void refresh_view(void) {
  ThreadView *cache = thread_view();
  RustAccountSnapshot *snapshot;
  const RustAccountView *view;
  size_t i;
  if (!cache || !account) return;
  if (cache->view && cache->view->revision == nuvio_account_revision(account)) return;
  snapshot = nuvio_account_snapshot(account);
  view = nuvio_account_snapshot_view(snapshot);
  if (!view) { nuvio_account_snapshot_destroy(snapshot); return; }
  nuvio_account_snapshot_destroy(cache->snapshot);
  cache->snapshot = snapshot; cache->view = view;
  cache->count = (int)(view->profile_count > ACCOUNT_PROFILE_MAX ? ACCOUNT_PROFILE_MAX : view->profile_count);
  memset(cache->profiles, 0, sizeof cache->profiles);
  for (i = 0; i < (size_t)cache->count; i++) {
    const RustProfileView *source = &view->profiles[i];
    AccountProfile *profile = &cache->profiles[i];
    profile->index = (int)source->id; profile->name = source->name; profile->color_hex = source->color;
    profile->avatar_url = source->avatar; profile->background_url = source->background;
    profile->is_primary = source->primary; profile->has_pin = source->lock == 2;
    profile->lock_verified = source->lock != 0; profile->uses_primary_addons = source->share_primary_addons;
  }
}
static const RustAccountView *current_view(void) {
  ThreadView *cache = thread_view();
  if (cache && !cache->view) refresh_view();
  return cache ? cache->view : NULL;
}
#ifdef NV_NATIVE_UI_TEST
/* Test driver supplies a Rust-owned account with a fake transport. */
void account_ui_test_attach(RustAccount *test_account) {
  ThreadView *cache = thread_view();
  if (cache) { nuvio_account_snapshot_destroy(cache->snapshot); cache->snapshot = NULL; cache->view = NULL; cache->count = 0; }
  account = test_account;
  refresh_view();
}
#endif

void session_init(void) {
  account = nuvio_account_create(data_dir(), NV_APP_ID);
  addon_registry_init(account, data_dir());
  home_order_runtime_init(account, data_dir());
  catalog_runtime_init(account, addon_registry_controller(), data_dir());
  refresh_view();
  printf("[account] Rust backend %s\n", account ? "initialized" : "unavailable");
  printf("[account] Compiled C/Rust account configuration matches: %s\n",
         nuvio_account_configuration_matches(NV_SUPABASE_URL, NV_SUPABASE_ANON_KEY, NV_TV_LOGIN_BASE) ? "yes" : "no");
}
SessionState session_state(void) { const RustAccountView *view = current_view(); return view ? (SessionState)view->login : SESSION_ERROR; }
int session_is_logged_in(void) { return nuvio_account_logged_in(account); }
int session_available(void) { return account != NULL; }
const char *session_code(void) { const RustAccountView *view = current_view(); return view ? view->code : ""; }
const char *session_login_url(void) { const RustAccountView *view = current_view(); return view ? view->login_url : ""; }
const char *session_error(void) { const RustAccountView *view = current_view(); return view ? view->error : configuration_error; }
const char *session_error_email(void) { const RustAccountView *view = current_view(); return view ? view->email_error : configuration_error; }
const char *session_user(void) {
  ThreadView *cache = thread_view();
  char *user = nuvio_account_user(account);
  if (cache && user && (!cache->user || strcmp(cache->user, user))) {
    char *copy = strdup(user);
    if (copy) { free(cache->user); cache->user = copy; }
  }
  nuvio_text_destroy(user);
  return cache && cache->user ? cache->user : "";
}
int session_copy_token(char *output, size_t capacity) { return nuvio_account_copy_token(account, output, capacity); }
void session_login_start(void) { nuvio_account_login_tv(account); refresh_view(); }
void session_login_email(const char *email, const char *password) { nuvio_account_login_email(account, email, password); refresh_view(); }
void session_step(unsigned now_ms) { (void)now_ms; nuvio_account_step(account); refresh_view(); }
void session_cancel(void) { nuvio_account_cancel(account); refresh_view(); }
void session_logout(void) { nuvio_account_logout(account); addon_registry_logout(); home_order_runtime_logout(); refresh_view(); }

static char *request_account(int table, const char *name, const char *argument, int *status) {
  RustAccountReply *reply = nuvio_account_request(account, table, name, argument ? argument : (table ? "" : "{}"));
  const RustReplyView *view = nuvio_account_reply_view(reply);
  char *body = NULL;
  if (status) *status = view ? view->status : 0;
  if (view && view->data) {
    body = malloc(view->length + 1);
    if (body) { memcpy(body, view->data, view->length); body[view->length] = 0; }
  }
  if (!view || view->status == 0 || view->status == 429 || view->status >= 500) nuvem_falhou();
  else nuvem_ok();
  nuvio_account_reply_destroy(reply);
  return body;
}
char *session_rpc(const char *name, const char *body, int *status) { return request_account(0, name, body, status); }
char *session_table(const char *name, const char *query, int *status) { return request_account(1, name, query, status); }

int profiles_pull(void) { int result = nuvio_account_load_profiles(account); refresh_view(); return result < 0 ? profiles_count() : result; }
int profiles_count(void) { ThreadView *cache = thread_view(); current_view(); return cache ? cache->count : 0; }
const AccountProfile *profiles_get(int slot) { ThreadView *cache = thread_view(); current_view(); return cache && slot >= 0 && slot < cache->count ? &cache->profiles[slot] : NULL; }
const AccountProfile *profiles_get_active(void) {
  int i, active = profiles_active();
  for (i = 0; i < profiles_count(); i++) { const AccountProfile *profile = profiles_get(i); if (profile->index == active) return profile; }
  return NULL;
}
const char *profiles_owner(void) { const RustAccountView *view = current_view(); return view ? view->owner : ""; }
int profiles_active(void) { return (int)nuvio_account_active_id(account); }
int profiles_active_addons(void) { return (int)nuvio_account_addon_profile(account, 0); }
void account_apply_profile(void) {
  int active = profiles_active();
  fil_definir_perfil(active); fontepref_definir_perfil(active); arteesc_definir_perfil(active);
  plugins_perfil_mudou();
  addon_registry_step(0, 0);
  home_order_runtime_step(0);
}
void profiles_load_active(void) { refresh_view(); account_apply_profile(); }
int profiles_needs_selection(void) { return nuvio_account_needs_selection(account); }
int profiles_without_choice(void) { const AccountProfile *profile = profiles_get(0); return profiles_count() == 0 || (profiles_count() == 1 && profile->lock_verified && !profile->has_pin); }
int profiles_can_dismiss(void) { const RustAccountView *view = current_view(); return view ? view->can_dismiss : 0; }
void profiles_keep_active(void) { nuvio_account_keep_profile(account); refresh_view(); }
int profiles_suggested_index(void) { const RustAccountView *view = current_view(); return view ? (int)view->suggested_slot : 0; }
ProfileAction profiles_action(int slot) {
  const AccountProfile *profile = profiles_get(slot);
  if (!profile) return PROFILE_ACTION_NONE;
  if (!profile->lock_verified) return PROFILE_ACTION_RETRY;
  return profile->has_pin ? PROFILE_ACTION_PIN : PROFILE_ACTION_ENTER;
}
void profiles_forget(void) { refresh_view(); }
uint64_t account_select_profile(int index, const char *pin) { return index > 0 ? nuvio_account_select(account, (uint32_t)index, pin) : 0; }
int account_selection_result(uint64_t action) {
  const RustAccountView *view = current_view();
  if (!action) return 0;
  if (!view) return -2;
  return view->selection_action == action ? view->selection_result : -3;
}
void account_cancel_selection(void) { nuvio_account_cancel_selection(account); refresh_view(); }

/* Existing C capabilities keep their historical symbols until their own
 * rewrite. Application state, storage and networking stay owned by Rust. */
void sessao_iniciar(void) { session_init(); }
SesEstado sessao_estado(void) { return session_state(); }
int sessao_logada(void) { return session_is_logged_in(); }
const char *sessao_codigo(void) { return session_code(); }
const char *sessao_url_login(void) { return session_login_url(); }
const char *sessao_erro(void) { return session_error(); }
const char *sessao_usuario(void) { return session_user(); }
int sessao_token_copiar(char *output, size_t capacity) { return session_copy_token(output, capacity); }
void sessao_login_comecar(void) { session_login_start(); }
void sessao_passo(unsigned now) { session_step(now); }
void sessao_cancelar(void) { session_cancel(); }
void sessao_login_email(const char *email, const char *password) { session_login_email(email, password); }
const char *sessao_erro_email(void) { return session_error_email(); }
void sessao_sair(void) { session_logout(); }
char *sessao_rpc(const char *name, const char *body, int *status) { return session_rpc(name, body, status); }
char *sessao_tabela(const char *name, const char *query, int *status) { return session_table(name, query, status); }
int perfis_puxar(void) { return profiles_pull(); }
int perfis_n(void) { return profiles_count(); }
const ContaPerfil *perfis_item(int slot) { return profiles_get(slot); }
const ContaPerfil *perfis_item_ativo(void) { return profiles_get_active(); }
const char *perfis_dono(void) { return profiles_owner(); }
int perfis_ativo(void) { return profiles_active(); }
int perfis_ativo_addons(void) { return profiles_active_addons(); }
void perfis_carregar_ativo(void) { profiles_load_active(); }
int perfis_precisa_escolher(void) { return profiles_needs_selection(); }
int perfis_sem_escolha(void) { return profiles_without_choice(); }
int perfis_pode_dispensar(void) { return profiles_can_dismiss(); }
void perfis_manter_ativo(void) { profiles_keep_active(); }
int perfis_indice_sugerido(void) { return profiles_suggested_index(); }
PerfilAcao perfis_acao(int slot) { return profiles_action(slot); }
void perfis_esquecer(void) { profiles_forget(); }
