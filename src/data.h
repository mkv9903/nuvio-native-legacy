// Where the app saves USER data — session, active profile, sync cache.
//
// Default folders are scoped to NV_APP_ID. This fork never migrates or reads
// the old app's data folders. NUVIO_DATA (or NUVIO_DADOS) explicitly selects
// a production data directory; debug builds use an ID subdirectory inside it.
//
// The directory is PROBED, not guessed: there is no reliable public documentation
// of where a native webOS app can write, and a wrong hardcoded path turns
// "saved nothing" into a silent failure — the app opens, appears logged in, and
// on the next launch forgets everything without any message. The probe tries
// to write in each candidate and logs which one won.
#ifndef NV_DATA_H
#define NV_DATA_H

// Chooses an explicit override, ~/.<app-id>, /media/developer/temp/<app-id>,
// or, as a last resort, artDir/.<app-id>. Call once at startup.
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

// Compatibility alias for writes of cached content; native writes are atomic.
int data_write_light(const char *name, const char *content);

// Reads entire `name` into a new NUL-terminated buffer (caller frees).
char *data_read(const char *name);

int data_delete(const char *name);

// STABLE identifier for this installation, generated on first run and stored.
// Web sync sends this in `p_origin_client_id` so the server does not echo back
// to the device writes it just performed.
const char *data_client_id(void);

// New UUID v4 on each call. Tested against server: start_tv_login_session RPC
// rejects with "Invalid device nonce" any nonce that is not a valid UUID.
void data_uuid(char *dst, unsigned size);

int data_persistent(void);

// Backward-compatibility shims for legacy callers
void        dados_iniciar(const char *dirArte);
const char *dados_dir(void);
char       *dados_caminho(char *dst, unsigned size, const char *name);
int         dados_gravar(const char *nome, const char *conteudo);
int         dados_gravar_leve(const char *nome, const char *conteudo);
char       *dados_ler(const char *nome);
int         dados_apagar(const char *nome);
const char *dados_cliente_id(void);
void        dados_uuid(char *dst, unsigned size);
int         dados_persistente(void);


#endif
