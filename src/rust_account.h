#ifndef NV_RUST_ACCOUNT_H
#define NV_RUST_ACCOUNT_H
#include <stddef.h>
#include <stdint.h>

typedef struct RustAccount RustAccount;
typedef struct RustAccountSnapshot RustAccountSnapshot;
typedef struct RustAccountReply RustAccountReply;

typedef struct {
  uint32_t id;
  const char *name, *color, *avatar, *background;
  int primary, lock, share_primary_addons;
} RustProfileView;
typedef struct {
  uint64_t revision;
  int login;
  const char *user, *code, *login_url, *error, *email_error, *owner;
  int persistence_error;
  const RustProfileView *profiles;
  size_t profile_count;
  uint32_t selected;
  int needs_selection, can_dismiss;
  size_t suggested_slot;
  uint64_t selection_action;
  int selection_result;
} RustAccountView;
typedef struct { int status; const unsigned char *data; size_t length; } RustReplyView;

/* The backend copies action inputs before returning. Snapshots and replies
 * remain valid until their explicit destroy call, including their strings. */
RustAccount *nuvio_account_create(const char *directory, const char *app_id);
/* Diagnostic equality only; no credentials are returned or logged. */
int nuvio_account_configuration_matches(const char *base, const char *public_key, const char *login_base);
void nuvio_account_destroy(RustAccount *account);
RustAccountSnapshot *nuvio_account_snapshot(const RustAccount *account);
const RustAccountView *nuvio_account_snapshot_view(const RustAccountSnapshot *snapshot);
void nuvio_account_snapshot_destroy(RustAccountSnapshot *snapshot);
void nuvio_account_step(const RustAccount *account);
uint64_t nuvio_account_revision(const RustAccount *account);
uint32_t nuvio_account_active_id(const RustAccount *account);
int nuvio_account_logged_in(const RustAccount *account);
int nuvio_account_needs_selection(const RustAccount *account);
char *nuvio_account_user(const RustAccount *account);
void nuvio_text_destroy(char *text);
void nuvio_account_login_email(const RustAccount *account, const char *email, const char *password);
void nuvio_account_login_tv(const RustAccount *account);
void nuvio_account_cancel(const RustAccount *account);
void nuvio_account_logout(const RustAccount *account);
uint64_t nuvio_account_select(const RustAccount *account, uint32_t id, const char *pin);
void nuvio_account_cancel_selection(const RustAccount *account);
int nuvio_account_keep_profile(const RustAccount *account);
int nuvio_account_load_profiles(const RustAccount *account);
uint32_t nuvio_account_addon_profile(const RustAccount *account, int force_primary);
RustAccountReply *nuvio_account_request(const RustAccount *account, int table, const char *name, const char *argument);
const RustReplyView *nuvio_account_reply_view(const RustAccountReply *reply);
void nuvio_account_reply_destroy(RustAccountReply *reply);
int nuvio_account_copy_token(const RustAccount *account, char *output, size_t capacity);

/* C UI adapter. Profile selection is an asynchronous Rust action, including
 * PIN verification. A result belongs to exactly one action. */
uint64_t account_select_profile(int index, const char *pin);
int account_selection_result(uint64_t action);
void account_cancel_selection(void);
void account_apply_profile(void);

#endif
