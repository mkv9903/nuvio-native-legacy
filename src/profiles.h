// Account profiles.
//
// Everything the sync requests passes `p_profile_id`. Without selecting a profile,
// the app would always sync profile 1 — which in a family account means
// showing another person's library and writing their progress. Profiles must
// therefore precede other surfaces.
//
// THE ACCOUNT OWNER is not necessarily the user who logged in: `get_sync_owner`
// returns the id of who actually owns the data (shared account).
// Addons table reading filters by THIS id, not by the token `sub`.
#ifndef NV_PROFILES_H
#define NV_PROFILES_H

#define ACCOUNT_PROFILE_MAX 8
#define CONTA_PERFIL_MAX ACCOUNT_PROFILE_MAX

typedef struct {
  union { int index; int indice; };                // profile_index (1..n) — passes in p_profile_id
  union { const char *name; const char *nome; };
  union { const char *color_hex; const char *corHex; };  // avatar_color_hex, "#1E88E5"
  union { const char *avatar_url; const char *avatarUrl; };
  union { const char *background_url; const char *fundoUrl; };
  union { int is_primary; int primario; };          // is_primary
  union { int has_pin; int temPin; };               // from sync_pull_profile_locks
  int lock_verified;
  union { int uses_primary_addons; int usaAddonsDoPrimario; }; // uses_primary_addons
} AccountProfile;

typedef AccountProfile ContaPerfil;

typedef enum {
  PROFILE_ACTION_NONE = 0,   // non-existent slot
  PROFILE_ACTION_ENTER,      // enter directly
  PROFILE_ACTION_PIN,        // locked: prompt for PIN first
  PROFILE_ACTION_RETRY       // lock not verified: retry account request
} ProfileAction;

typedef ProfileAction PerfilAcao;
#define PERFIL_ACAO_NADA   PROFILE_ACTION_NONE
#define PERFIL_ACAO_ENTRAR  PROFILE_ACTION_ENTER
#define PERFIL_ACAO_PIN    PROFILE_ACTION_PIN

// Fetches profiles and owner. BLOCKS — call from sync thread.
// Returns count found. Zero is NOT an error: new account may have no profiles.
int profiles_pull(void);

int                  profiles_count(void);
const AccountProfile *profiles_get(int i);
// Currently active profile, or NULL if list has not yet arrived.
const AccountProfile *profiles_get_active(void);
const char          *profiles_owner(void);       // uuid from get_sync_owner; "" if absent

// Active profile index. Persisted to disk.
int  profiles_active(void);                      // profile_index; 1 when none chosen
// Profile from which ADDONS are loaded: equal to profiles_active(), unless
// profile inherits primary (uses_primary_addons), in which case it is 1.
int  profiles_active_addons(void);
// Reads active profile AND PROFILE LIST OF LAST SESSION from disk. Call once on startup.
void profiles_load_active(void);

// 1 when selection screen has a real question to ask in this session.
int  profiles_needs_selection(void);

// Same condition without session flag: 1 when current list offers no choice.
int  profiles_without_choice(void);

// 1 when Back can dismiss screen without explicit selection.
int  profiles_can_dismiss(void);

// Confirms profile already saved without rewriting file.
void profiles_keep_active(void);

// Suggested slot (0..n-1 of list) to focus first.
int  profiles_suggested_index(void);

// Action on pressing OK on slot `i`.
ProfileAction profiles_action(int i);

// Clears profiles, owner and saved choice on logout.
void profiles_forget(void);

// Backward-compatibility shims
int perfis_puxar(void);
int perfis_n(void);
const ContaPerfil *perfis_item(int i);
const ContaPerfil *perfis_item_ativo(void);
const char *perfis_dono(void);
int perfis_ativo(void);
int perfis_ativo_addons(void);
void perfis_carregar_ativo(void);
int perfis_precisa_escolher(void);
int perfis_sem_escolha(void);
int perfis_pode_dispensar(void);
void perfis_manter_ativo(void);
int perfis_indice_sugerido(void);
PerfilAcao perfis_acao(int i);
void perfis_esquecer(void);

#endif
