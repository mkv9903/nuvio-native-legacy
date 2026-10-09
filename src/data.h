// Where the app saves USER data — session, active profile, sync cache.
//
// Previously settings and catalog wrote inside artDir, the PACKAGE folder.
// In developer mode this works, which is why it went unnoticed; in a truly
// installed app the package folder is the wrong place, shared across anyone
// using the device. With login, saving there would mean one person's session
// living inside another's app.
//
// The directory is PROBED, not guessed: there is no reliable public documentation
// of where a native webOS app can write, and a wrong hardcoded path turns
// "saved nothing" into a silent failure — the app opens, appears logged in, and
// on the next launch forgets everything without any message. The probe tries
// to write in each candidate and logs which one won.
#ifndef NV_DATA_H
#define NV_DATA_H

// Chooses the directory. `artDir` enters as a LAST resort (legacy behavior,
// better than not saving anything). Call once at startup.
void data_init(const char *artDir);

// Chosen directory without trailing slash. Never NULL after data_init; may be
// "" if no candidate accepted writing — in which case saving is a no-op and the
// log explains why.
const char *data_dir(void);

// Assembles `data_dir()/name` into `dst`. Returns dst, or NULL if there is no directory.
char *data_path(char *dst, unsigned size, const char *name);

// Writes `content` to `name` atomically (temp file + rename). Returns 1 on success.
// Atomic because losing the session due to a half-written file is exactly the
// kind of bug that only appears on someone else's device.
int data_write(const char *name, const char *content);

// Same write, but flushing to IndexedDB can wait: 15 s timer instead of 700 ms.
// Only for RE-OBTAINABLE content — cursor position, image cache.
// User data continues using data_write.
int data_write_light(const char *name, const char *content);

// Reads entire `name` into a new NUL-terminated buffer (caller frees).
char *data_read(const char *name);

int data_delete(const char *name);

// Flushes to persistent storage what was written since the last flush.
// On webOS and Mac this is a no-op: fopen already wrote to disk. On Tizen target
// the file lives in IndexedDB and only syncs here.
//
// Call from the MAIN LOOP, once per frame, not from worker threads.
// The function decides whether it is time to flush: almost every frame it costs
// reading two integers and returns. See policy in data.c.
void data_sync(void);

// Startup after two launches without a first frame: conservative mode without
// wiping IndexedDB. On-demand art fetching remains available.
int data_recovery_mode(void);
int data_sync_pending(void);
int data_sync_in_backoff(void);

// Notifies that someone wrote to the filesystem outside data_write.
// `light` = 1 for re-obtainable content (image cache in tex_cache.c), which
// does not merit paying for a flush on its own: losing the last downloaded poster
// costs one download, losing the session costs a QR login.
void data_mark_dirty(int light);

// FILESYSTEM LOCK, for writes outside this module.
//
// Only does anything on Tizen, where it is mandatory: WASM "filesystem"
// is a shared JavaScript data structure between workers and is NOT thread-safe.
// See note in data.c — ignoring this caused the app to FREEZE without error.
// Wrap only WRITES and removals; concurrent reads during flush are safe,
// and locking image decoding (30 ms) would serialize decode threads needlessly.
void data_fs_lock(void);
void data_fs_unlock(void);

// Flush telemetry for main.c frame line: flushes since last report and
// SYNCHRONOUS cost of the worst one in ms. Without this one cannot distinguish
// "spike vanished" from "spike moved".
extern int    data_flush_count;
extern double data_flush_ms;
extern int    data_sync_successes;
extern int    data_sync_failures;
void data_flush_reset(void);

// STABLE identifier for this installation, generated on first run and stored.
// Web sync sends this in `p_origin_client_id` so the server does not echo back
// to the device writes it just performed.
const char *data_client_id(void);

// New UUID v4 on each call. Tested against server: start_tv_login_session RPC
// rejects with "Invalid device nonce" any nonce that is not a valid UUID.
void data_uuid(char *dst, unsigned size);

// EXIT HANDLING ON TIZEN (issue #120). No effect on webOS or Mac.
int  data_farewell_read(void);
void data_farewell_finish(void);
void data_flush_and_exit(void);

// Indicates backend configured: IDBFS mounted on Tizen; on native targets,
// directory verified with actual write probe.
int data_persistent(void);

// Backward-compatibility shims for legacy callers
void        dados_iniciar(const char *dirArte);
const char *dados_dir(void);
char       *dados_caminho(char *dst, unsigned size, const char *name);
int         dados_gravar(const char *nome, const char *conteudo);
int         dados_gravar_leve(const char *nome, const char *conteudo);
char       *dados_ler(const char *nome);
int         dados_apagar(const char *nome);
void        dados_sincronizar(void);
int         dados_modo_recuperacao(void);
int         dados_sync_pendente(void);
int         dados_sync_em_recuo(void);
void        dados_marcar_sujo(int leve);
void        dados_fs_travar(void);
void        dados_fs_liberar(void);
void        dados_desc_zerar(void);
const char *dados_cliente_id(void);
void        dados_uuid(char *dst, unsigned size);
int         dados_despedida_ler(void);
void        dados_despedida_fim(void);
void        dados_descarregar_e_sair(void);
int         dados_persistente(void);

#define dados_desc_n data_flush_count
#define dados_desc_ms data_flush_ms
#define dados_sync_sucessos data_sync_successes
#define dados_sync_falhas data_sync_failures

#endif
