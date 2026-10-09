// WHAT THE USER MARKED ON TV AND HAS NOT YET REACHED THE ACCOUNT (persistent journal).
//
// Background: "there's an issue with watched status that we save, we must
// sync with profile so nothing is lost, watched, marked, etc."
//
// WHERE IT WAS LOST previously:
//   - "Mark as watched" (visto.c) sent ONE synchronous RPC on a detached thread.
//     Offline, 401, 5xx or app closed mid-way: the mark only existed in memory
//     (vistoep.c and catalog title history DO NOT go to disk) and was never retried.
//   - Unmarking offline was also lost, and the subsequent account pull re-marked it.
//   - "+" (Saved / library) never uploaded to account: only local salvos.txt and Trakt/Simkl.
//
// WHAT THIS MODULE GUARANTEES:
//   - Each action creates ONE journal entry (per profile, per item), saved to disk
//     before any network attempt. Latest intent per item wins.
//   - Sending is per ITEM, never "the entire local list": watched via
//     sync_push_watched_items / sync_delete_watched_items with exact diffs.
//     Entry leaves journal only after 2xx response; failure retries next cycle.
//   - Library uses read-merge-write with sync_push_library.
//   - Pull respects journal: remote row of an item the user un-marked / removed
//     (and not newer than action) does not re-mark anything; newer remote row wins.
//
// The file is per USER (conta-pend-<sub>.txt) and survives logout: only contains
// ids, profile and timestamps — no tokens.
#ifndef NV_ACCOUNT_PENDING_H
#define NV_ACCOUNT_PENDING_H

#include "vistoep.h"

#define ACCOUNT_PENDING_MAX 4000
#define CONTAPEND_MAX ACCOUNT_PENDING_MAX

// ---- logging (MAIN THREAD or caller of watched status) -----------------------
// Episodes (season >= 0, episode >= 1) or full title (n == 0 with NULL pairs).
// `watched`: 1 = mark, 0 = unmark. Profile = perfis_ativo() at time of gesture.
// Returns count of changed entries.
int  account_pending_episodes(const char *imdb, const char *type,
                              const VistoPar *pairs, int n, int watched);
int  account_pending_title(const char *imdb, const char *type, int watched);

// "+" / remove from list. `name`/`poster` used only for new account items.
int  account_pending_list(const char *imdb, const char *type, const char *name,
                          const char *poster, int saved);

// ---- dispatch ---------------------------------------------------------------
// SYNCHRONOUS. Sends everything pending across all profiles of logged-in user.
// Returns count of confirmed entries, or -1 if any failed.
int  account_pending_send(void);

// Triggers account_pending_send on a background thread if none is running.
void account_pending_kick(void);

// ---- pull (MAIN THREAD) -----------------------------------------------------
// 1 when remote row (active profile) should be IGNORED because user un-marked /
// removed item on this TV after remote_ms.
int  account_pending_watched_hidden(const char *imdb, int season, int episode,
                                    long long remote_ms);
int  account_pending_list_hidden(const char *imdb, long long remote_ms);

// Reapplies journal to local state (vistoep + title history) for active profile.
int  account_pending_apply_local(void);

// Prunes journal entries CONFIRMED prior to since_ms.
void account_pending_prune(long long since_ms);

// ---- status -----------------------------------------------------------------
int  account_pending_count(void);      // unconfirmed entries, all profiles
void account_pending_forget(void);     // logout: drops memory, file kept
void account_pending_set_clock(long long (*fn)(void));
void account_pending_set_threadless(int enable);
long long account_pending_now_ms(void);

// Backward compatibility wrappers
int  contapend_episodios(const char *imdb, const char *tipo,
                         const VistoPar *pares, int n, int visto);
int  contapend_titulo(const char *imdb, const char *tipo, int visto);
int  contapend_lista(const char *imdb, const char *tipo, const char *nome,
                     const char *poster, int salvo);
int  contapend_enviar(void);
void contapend_chutar(void);
int  contapend_visto_oculto(const char *imdb, int temporada, int episodio,
                            long long remotoMs);
int  contapend_lista_oculta(const char *imdb, long long remotoMs);
int  contapend_aplicar_local(void);
void contapend_podar(long long desdeMs);
int  contapend_pendentes(void);
void contapend_esquecer(void);
void contapend_relogio(long long (*f)(void));
void contapend_sem_fio(int sim);
long long contapend_agora_ms(void);

#endif
