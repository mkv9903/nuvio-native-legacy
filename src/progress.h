// Playback progress: local record with identity and timestamp.
//
// Previously progress lived in progresso.txt as "imdb pos dur [season ep]",
// written by catalogo.c. Three things were missing, each causing data loss
// (PROGRESS-PLAN.md, Part 1):
//
//   1. THE KEY. Web and tvOS identify the server record by
//      `progress_key` = "tt123_s4e9"; this app sent "tt123:4:9". The server
//      upserts by this key, so the same scene turned into TWO records.
//   2. THE TIMESTAMP. Without `last_watched` there is no way to know if what
//      came from the server is newer than what is on disk. The sync cycle pulls
//      before pushing (correctly) and applied the OLD remote over the newly-
//      watched local — reverting position until the next cycle.
//   3. THE PENDING FLAG. A record THIS device wrote and has not yet pushed
//      beats anything returned by the server. This is the tvOS rule
//      (`isPendingPush`), resolving item 2 without synchronized clocks.
//
// The file remains ONE (progresso.txt in data_dir()), now with a profile
// column: pulling profile 2 must not overwrite what profile 1 saved, and
// logout deletes everything at once without enumerating profiles.
//
// The old format is read and migrated on first save. Legacy records enter as
// PENDING: they were never pushed with the correct key, and pushing again
// fixes parallel records on the server.
//
// Thread-safe: all public functions acquire a mutex.
#ifndef NV_PROGRESS_H
#define NV_PROGRESS_H

#define PROGRESS_MAX 480   // all rows, all profiles
#define PROG_MAX PROGRESS_MAX

typedef struct {
  union { int profile; int perfil; };
  union { char key[48]; char chave[48]; };          // progress_key: "tt123_s4e9" or "tt123"
  union { char content_id[48]; char contentId[48]; };// clean title ("tt...", "kitsu:41370")
  union { char type[8]; char tipo[8]; };            // "movie" | "series"
  union { int season; int temporada; };             // 0 when movie
  union { int episode; int episodio; };             // 0 when movie
  union { double pos_sec; double posSeg; };
  union { double dur_sec; double durSeg; };
  union { long long last_watched_ms; long long lastWatchedMs; }; // 0 = unknown (migrated)
  union { int pending; int pendente; };             // 1 = written locally, not yet pushed
} ProgressRecord;

typedef ProgressRecord ProgRegistro;

// Key formatted for web (toProgressKey): "%s_s%de%d" when season >= 0 and
// episode > 0; otherwise just content_id.
void progress_key(char *dst, unsigned n, const char *content_id, int season, int episode);

// Cuts a composite id ("tt123:4:9") at the first non-base ':'. Fills season/episode if non-NULL.
void progress_content_id(char *dst, unsigned n, const char *imdb, int *season, int *episode);

// Records of ACTIVE PROFILE, newest to oldest. Returns count.
int  progress_read(ProgressRecord *out, int max);

// Most recent UNFINISHED item of any profile (active or not) according to Continue Watching rules:
// duration >= 60s, between 1% and completed_pct, and not dismissed by user.
int  progress_continue_from_profile(int profile, int completed_pct, ProgressRecord *out);

// Copies record of active profile matching key into `out`. Returns 1 if found.
int  progress_by_key(const char *key, ProgressRecord *out);

// LOCAL write (player closing, watched toggle, post-play).
int  progress_write_local(const char *imdb, int season, int episode,
                          double pos_sec, double dur_sec);

// Applies a REMOTE record (pull). Pending local wins; newer wins; ties keep local.
int  progress_apply_remote(const ProgressRecord *r);

// Pending records of active profile, for push. Returns count.
int  progress_pending(ProgressRecord *out, int max);

// Confirms successfully pushed records.
void progress_confirm_pushed(const ProgressRecord *sent, int n);

// Immediate key confirmation (compatibility).
void progress_mark_pushed(const char *const *keys, int n);

void progress_remove(const char *key);

// Marks item as dismissed from Continue Watching at current timestamp.
void progress_mark_removed(const char *imdb);

// Returns 1 if removal of item wins over timestamp `timestamp_ms`.
int  progress_removed_wins(const char *imdb, long long timestamp_ms);

// Clears file completely across all profiles.
void progress_forget_all(void);

// Invalidates cache; next read re-reads disk.
void progress_invalidate(void);

// Clock in ms since epoch.
long long progress_now_ms(void);
void      progress_set_clock(long long (*clock_fn)(void));

// Compatibility function shims
void prog_chave(char *dst, unsigned n, const char *contentId, int temporada, int episodio);
void prog_content_id(char *dst, unsigned n, const char *imdb, int *temporada, int *episodio);
int  prog_ler(ProgRegistro *saida, int max);
int  prog_continuar_de_perfil(int perfil, int pctConcluido, ProgRegistro *saida);
int  prog_por_chave(const char *chave, ProgRegistro *saida);
int  prog_gravar_local(const char *imdb, int temporada, int episodio, double posSeg, double durSeg);
int  prog_aplicar_remoto(const ProgRegistro *r);
int  prog_pendentes(ProgRegistro *saida, int max);
void prog_confirmar_empurrados(const ProgRegistro *enviados, int n);
void prog_remover(const char *chave);
void prog_marcar_removido(const char *imdb);
int  prog_removido_vence(const char *imdb, long long instanteMs);
void prog_esquecer_tudo(void);
void prog_invalidar(void);
long long prog_agora_ms(void);
void prog_definir_relogio(long long (*relogio)(void));

#endif
