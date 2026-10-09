#include "app_id.h"
#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#if defined(__EMSCRIPTEN__) || defined(NV_DADOS_TEST)
static volatile int dirty, dirtyLight;
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// PERSISTENCE ON TIZEN TARGET: IDBFS, not the default filesystem.
//
// The filesystem that Emscripten mounts by default (MEMFS) lives in page memory
// and DIES ON EVERY RELOAD. With it the app writes the session, says
// "[data] writing to ...", and on the next launch forgets everything.
// IDBFS stores in IndexedDB, which survives reload and TV reboot.
EM_ASYNC_JS(int, nv_idbfs_clean_art_cache, (const char *point), {
  var dbName = UTF8ToString(point);
  return await new Promise(function (resolve) {
    var req;
    try { req = indexedDB.open(dbName); } catch (e) { resolve(0); return; }
    req.onerror = function () { resolve(0); };
    req.onblocked = function () { resolve(0); };
    req.onsuccess = function () {
      var db = req.result;
      if (!db.objectStoreNames || !db.objectStoreNames.contains("FILE_DATA")) {
        db.close(); resolve(0); return;
      }
      var removed = 0, tx;
      try {
        tx = db.transaction(["FILE_DATA"], "readwrite");
        var store = tx.objectStore("FILE_DATA");
        var cursorReq = store.openKeyCursor();
        cursorReq.onsuccess = function () {
          var cursor = cursorReq.result;
          if (!cursor) return;
          var path = cursor.key;
          if (typeof path === "string" && path.indexOf("/nuvio/cache/") === 0 &&
              path.indexOf("/", 13) < 0 && /[.](jpe?g|png|webp|gif|avif)$/i.test(path)) {
            store.delete(path); removed++;
          }
          cursor.continue();
        };
        tx.oncomplete = function () { db.close(); resolve(removed); };
        tx.onerror = tx.onabort = function () { db.close(); resolve(0); };
      } catch (e) { db.close(); resolve(0); }
    };
  });
});

EM_ASYNC_JS(int, nv_idbfs_mount, (const char *point, int nArt), {
  var path = UTF8ToString(point);
  Module.nvRecoveryMode = !!window.__nvModoRecuperacao;
  if (Module.nvRecoveryMode) {
    var msg = "[startup] recovery active; " + nArt +
              " legacy art file(s) removed, personal data kept";
    if (window.__nvDiag) window.__nvDiag(msg); else console.log(msg);
  }
  try {
    FS.mkdirTree(path);
    FS.mount(IDBFS, {}, path);
  } catch (e) {
    if (!e || e.errno !== 16) { return 0; }
  }
  return await new Promise(function (r) {
    FS.syncfs(true, function (err) { r(err ? 0 : 1); });
  });
});

// FLUSH WITHOUT WAITING FOR IndexedDB.
EM_JS(void, nv_idbfs_flush, (int type), {
  if (Module.nvSyncInFlight) return;
  Module.nvSyncInFlight = 1;
  Module.nvSyncPendingResult = 0;
  Module.nvSyncResultType = type;
  try {
    FS.syncfs(false, function (err) {
      Module.nvSyncInFlight = 0;
      Module.nvSyncPendingResult = err ? -1 : 1;
      if (err) {
        Module.nvSyncFailAttempts = (Module.nvSyncFailAttempts || 0) + 1;
        var backoffMs = Math.min(30000, 1000 * Math.pow(2, Module.nvSyncFailAttempts - 1));
        Module.nvSyncCanAt = Date.now() + backoffMs;
      } else {
        Module.nvSyncFailAttempts = 0;
        Module.nvSyncCanAt = 0;
      }
    });
  } catch (e) {
    Module.nvSyncInFlight = 0;
    Module.nvSyncPendingResult = -1;
  }
});

EM_JS(int, nv_idbfs_result, (void), {
  var r = Module.nvSyncPendingResult || 0;
  Module.nvSyncPendingResult = 0;
  return r;
});

EM_JS(int, nv_idbfs_in_flight, (void), {
  return Module.nvSyncInFlight ? 1 : 0;
});

EM_JS(int, nv_idbfs_can_write, (void), {
  if (!Module.nvSyncCanAt) return 1;
  return Date.now() >= Module.nvSyncCanAt ? 1 : 0;
});

EM_JS(void, nv_idbfs_safety_net, (int *pLock), {
  if (Module.nvSafetyNetInstalled) return;
  Module.nvSafetyNetInstalled = 1;
  var idx = pLock >> 2;
  function trigger() {
    if (Module.nvSyncInFlight || Atomics.load(HEAP32, idx) !== 0) return;
    Module.nvSyncInFlight = 1;
    try {
      FS.syncfs(false, function () { Module.nvSyncInFlight = 0; });
    } catch (e) { Module.nvSyncInFlight = 0; }
  }
  window.addEventListener("beforeunload", trigger);
  document.addEventListener("visibilitychange", function () {
    if (document.visibilityState === "hidden") trigger();
  });
});

EM_JS(void, nv_idbfs_flush_and_exit, (int *pLock), {
  var idx = pLock >> 2, done = 0;
  var exit = function () {
    if (done) return;
    done = 1;
    try {
      if (Module.nvExit) Module.nvExit();
      else tizen.application.getCurrentApplication().exit();
    } catch (e) {}
  };
  setTimeout(exit, 3000);
  (function tryFlush() {
    if (done) return;
    if (Module.nvSyncInFlight || Atomics.load(HEAP32, idx) !== 0) { setTimeout(tryFlush, 20); return; }
    Module.nvSyncInFlight = 1;
    try {
      FS.syncfs(false, function () { Module.nvSyncInFlight = 0; exit(); });
    } catch (e) { Module.nvSyncInFlight = 0; exit(); }
  })();
});

EM_JS(void, nv_farewell_arm, (void), {
  if (Module.nvFarewellArmed) return;
  Module.nvFarewellArmed = 1;
  document.addEventListener("visibilitychange", function () {
    try {
      var v = localStorage.getItem("nv-despedida");
      if (document.visibilityState === "hidden") { if (v !== "fim") localStorage.setItem("nv-despedida", "oculto"); }
      else if (v === "oculto") localStorage.removeItem("nv-despedida");
    } catch (e) {}
  });
});

static int idbfsMounted = 0;

#define NV_FLUSH_MIN_MS   700.0
#define NV_FLUSH_LIGHT_MS 15000.0
static double lastFlush;

static pthread_mutex_t dataLock = PTHREAD_MUTEX_INITIALIZER;
static volatile int fsBusy;
#define NV_FS_LOCK()   do { pthread_mutex_lock(&dataLock); \
                            __atomic_add_fetch(&fsBusy, 1, __ATOMIC_SEQ_CST); } while (0)
#define NV_FS_UNLOCK() do { __atomic_sub_fetch(&fsBusy, 1, __ATOMIC_SEQ_CST); \
                            pthread_mutex_unlock(&dataLock); } while (0)
#elif defined(NV_DADOS_TRAVA_TESTE)
#include <errno.h>
static pthread_mutex_t dataLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
static void nvLockTest(void) {
  if (pthread_mutex_lock(&dataLock) == EDEADLK) {
    fprintf(stderr, "[data] DOUBLE LOCK on same thread\n");
    abort();
  }
}
#define NV_FS_LOCK()   nvLockTest()
#define NV_FS_UNLOCK() pthread_mutex_unlock(&dataLock)
#else
#define NV_FS_LOCK()   ((void)0)
#define NV_FS_UNLOCK() ((void)0)
#endif

int    data_flush_count;
double data_flush_ms;
void data_flush_reset(void) { data_flush_count = 0; data_flush_ms = 0.0; }
int    data_sync_successes;
int    data_sync_failures;

#if defined(__EMSCRIPTEN__) || defined(NV_DADOS_TEST)
static void data_sync_apply_result(int result, int type) {
  if (result > 0) data_sync_successes++;
  else if (result < 0) {
    data_sync_failures++;
    if (type == 2) dirtyLight = 1;
    else dirty = 1;
  }
}
#endif

void data_fs_lock(void)   { NV_FS_LOCK(); }
void data_fs_unlock(void) { NV_FS_UNLOCK(); }

void data_mark_dirty(int light) {
#ifdef __EMSCRIPTEN__
  if (light) dirtyLight = 1; else dirty = 1;
#else
  (void)light;
#endif
}

static char dir[512];
static char clientId[64];

// Tries to create the directory and write to it.
static int probe(const char *candidate) {
  char testPath[600];
  FILE *f;
  if (!candidate || !*candidate) return 0;
  mkdir(candidate, 0755);
  snprintf(testPath, sizeof testPath, "%s/.escrita", candidate);
  f = fopen(testPath, "w");
  if (!f) return 0;
  if (fputs("ok\n", f) < 0) { fclose(f); return 0; }
  if (fclose(f) != 0) return 0;
  remove(testPath);
  return 1;
}

void data_init(const char *artDir) {
  char homeDir[512], envHome[512], artHome[512];
  int isolated = strcmp(NV_APP_ID, NV_APP_ID_PRODUCTION) != 0;
  const char *env = getenv("NUVIO_DATA");
  if (!env || !*env) env = getenv("NUVIO_DADOS");
  const char *home = getenv("HOME");
  const char *candidates[5];
  int n = 0, i;

#ifdef __EMSCRIPTEN__
  int nArt = 0;
  if (EM_ASM_INT({ return !!window.__nvModoRecuperacao; }))
    nArt = nv_idbfs_clean_art_cache("/nuvio");
  if (nv_idbfs_mount("/nuvio", nArt)) {
    candidates[n++] = "/nuvio";
    idbfsMounted = 1;
    nv_idbfs_safety_net((int *)&fsBusy);
  } else {
    printf("[data] IDBFS DID NOT MOUNT: nothing persists — Trakt, progress and sync\n"
           "       will reset on close. file:// origin blocks IndexedDB.\n");
  }
#endif
  if (env && *env) {
    if (!isolated) candidates[n++] = env;
    else {
      int size = snprintf(envHome, sizeof envHome, "%s/%s", env, NV_APP_ID);
      if (size > 0 && size < (int)sizeof envHome) candidates[n++] = envHome;
    }
  }
  if (home && *home) {
    int size = snprintf(homeDir, sizeof homeDir, isolated ? "%s/.%s" : "%s/.nuvio", home, NV_APP_ID);
    if (size > 0 && size < (int)sizeof homeDir) candidates[n++] = homeDir;
  }
  candidates[n++] = isolated ? "/media/developer/temp/" NV_APP_ID : "/media/developer/temp/nuvio";
  if (artDir && *artDir) {
    if (!isolated) candidates[n++] = artDir;
    else {
      int size = snprintf(artHome, sizeof artHome, "%s/.%s", artDir, NV_APP_ID);
      if (size > 0 && size < (int)sizeof artHome) candidates[n++] = artHome;
    }
  }

  for (i = 0; i < n; i++) {
    if (probe(candidates[i])) {
      snprintf(dir, sizeof dir, "%s", candidates[i]);
      printf("[data] writing to %s\n", dir);
      fflush(stdout);
      return;
    }
    printf("[data] rejected %s\n", candidates[i]);
  }
  dir[0] = 0;
  printf("[data] NO writable directory: session and settings will not survive next launch\n");
  fflush(stdout);
}

const char *data_dir(void) { return dir; }

int data_recovery_mode(void) {
#ifdef __EMSCRIPTEN__
  return EM_ASM_INT({ return Module.nvRecoveryMode ? 1 : 0; });
#else
  return 0;
#endif
}

int data_sync_pending(void) {
#ifdef __EMSCRIPTEN__
  return nv_idbfs_in_flight();
#else
  return 0;
#endif
}

int data_sync_in_backoff(void) {
#ifdef __EMSCRIPTEN__
  return EM_ASM_INT({ return Module.nvSyncFailAttempts && Module.nvSyncCanAt && Date.now() < Module.nvSyncCanAt; });
#else
  return 0;
#endif
}

void data_sync(void) {
#ifdef __EMSCRIPTEN__
  double now, waitTime, cost;
  int result, type;
  if (!idbfsMounted) return;
  while ((result = nv_idbfs_result()) != 0) {
    int resultType = EM_ASM_INT({ return Module.nvSyncResultType || 0; });
    data_sync_apply_result(result, resultType);
  }
  if (!dirty && !dirtyLight) return;
  now = emscripten_get_now();
  waitTime = dirty ? NV_FLUSH_MIN_MS : NV_FLUSH_LIGHT_MS;
  if (now - lastFlush < waitTime) return;
  if (nv_idbfs_in_flight()) return;
  if (!nv_idbfs_can_write()) return;
  type = dirty ? 1 : 2;
  dirty = 0; dirtyLight = 0;
  lastFlush = now;
  NV_FS_LOCK();
  nv_idbfs_flush(type);
  NV_FS_UNLOCK();
  cost = emscripten_get_now() - now;
  data_flush_count++;
  if (cost > data_flush_ms) data_flush_ms = cost;
#endif
}

char *data_path(char *dst, unsigned size, const char *name) {
  if (!dir[0] || !name || !*name) return NULL;
  snprintf(dst, size, "%s/%s", dir, name);
  return dst;
}

static int writeInternal(const char *name, const char *content, int light) {
  char path[600], tmp[600];
  FILE *f;
  size_t n;
  if (!data_path(path, sizeof path, name)) return 0;
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  NV_FS_LOCK();
  f = fopen(tmp, "w");
  if (!f) { NV_FS_UNLOCK(); return 0; }
  n = content ? strlen(content) : 0;
  if (n && fwrite(content, 1, n, f) != n) { fclose(f); remove(tmp); NV_FS_UNLOCK(); return 0; }
  if (fclose(f) != 0) { remove(tmp); NV_FS_UNLOCK(); return 0; }
  if (rename(tmp, path) != 0) { remove(tmp); NV_FS_UNLOCK(); return 0; }
  NV_FS_UNLOCK();
#ifdef __EMSCRIPTEN__
  if (light) dirtyLight = 1; else dirty = 1;
#else
  (void)light;
#endif
  return 1;
}

int data_write(const char *name, const char *content) {
  return writeInternal(name, content, 0);
}

int data_write_light(const char *name, const char *content) {
  return writeInternal(name, content, 1);
}

char *data_read(const char *name) {
  char path[600];
  FILE *f;
  long n;
  char *buf;
  if (!data_path(path, sizeof path, name)) return NULL;
  NV_FS_LOCK();
  f = fopen(path, "rb");
  if (!f) { NV_FS_UNLOCK(); return NULL; }
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) { fclose(f); NV_FS_UNLOCK(); return NULL; }
  buf = (char *)malloc((size_t)n + 1);
  if (!buf) { fclose(f); NV_FS_UNLOCK(); return NULL; }
  n = (long)fread(buf, 1, (size_t)n, f);
  fclose(f);
  NV_FS_UNLOCK();
  buf[n] = 0;
  return buf;
}

int data_delete(const char *name) {
  char path[600];
  if (!data_path(path, sizeof path, name)) return 0;
  NV_FS_LOCK();
  if (remove(path) != 0) { NV_FS_UNLOCK(); return 0; }
  NV_FS_UNLOCK();
#ifdef __EMSCRIPTEN__
  dirty = 1;
#endif
  return 1;
}

void data_uuid(char *dst, unsigned size) {
  static const char *hex = "0123456789abcdef";
  static int seeded;
  int i;
  if (size < 37) { if (size) dst[0] = 0; return; }
  if (!seeded) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)(size_t)dst);
    seeded = 1;
  }
  for (i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { dst[i] = '-'; continue; }
    if (i == 14) { dst[i] = '4'; continue; }
    if (i == 19) { dst[i] = hex[8 + (rand() & 3)]; continue; }
    dst[i] = hex[rand() & 15];
  }
  dst[36] = 0;
}

const char *data_client_id(void) {
  char *readBuf;
  if (clientId[0]) return clientId;

  readBuf = data_read("cliente.txt");
  if (readBuf) {
    char *end = readBuf + strlen(readBuf);
    while (end > readBuf && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) *--end = 0;
    if (readBuf[0]) snprintf(clientId, sizeof clientId, "%s", readBuf);
    free(readBuf);
    if (clientId[0]) return clientId;
  }

  data_uuid(clientId, sizeof clientId);

  { char line[64];
    snprintf(line, sizeof line, "%s\n", clientId);
    data_write("cliente.txt", line); }
  return clientId;
}

int data_farewell_read(void) {
#ifdef __EMSCRIPTEN__
  int r = EM_ASM_INT({
    try {
      var v = localStorage.getItem("nv-despedida") || "";
      localStorage.removeItem("nv-despedida");
      return v === "fim" ? 1 : v === "oculto" ? 2 : 0;
    } catch (e) { return 0; }
  });
  nv_farewell_arm();
  return r;
#elif defined(NV_ANDROID) || defined(NV_TPK)
  char *v = data_read("despedida.txt");
  int r = 0;
  if (v) { r = !strncmp(v, "fim", 3) ? 1 : !strncmp(v, "oculto", 6) ? 2 : 0; free(v); }
  data_delete("despedida.txt");
  return r;
#else
  return 0;
#endif
}

void data_farewell_finish(void) {
#ifdef __EMSCRIPTEN__
  EM_ASM({ try { localStorage.setItem("nv-despedida", "fim"); } catch (e) {} });
#elif defined(NV_ANDROID) || defined(NV_TPK)
  data_write("despedida.txt", "fim\n");
#endif
}

void data_flush_and_exit(void) {
#ifdef __EMSCRIPTEN__
  if (idbfsMounted) { nv_idbfs_flush_and_exit((int *)&fsBusy); return; }
  EM_ASM({
    try { if (Module.nvExit) Module.nvExit(); else tizen.application.getCurrentApplication().exit(); } catch (e) {}
  });
#endif
}

int data_persistent(void) {
#ifdef __EMSCRIPTEN__
  return idbfsMounted;
#else
  return dir[0] != 0;
#endif
}

// Binary backward-compatibility wrappers for legacy callers
void        dados_iniciar(const char *dirArte) { data_init(dirArte); }
const char *dados_dir(void) { return data_dir(); }
char       *dados_caminho(char *dst, unsigned size, const char *name) { return data_path(dst, size, name); }
int         dados_gravar(const char *nome, const char *conteudo) { return data_write(nome, conteudo); }
int         dados_gravar_leve(const char *nome, const char *conteudo) { return data_write_light(nome, conteudo); }
char       *dados_ler(const char *nome) { return data_read(nome); }
int         dados_apagar(const char *nome) { return data_delete(nome); }
void        dados_sincronizar(void) { data_sync(); }
int         dados_modo_recuperacao(void) { return data_recovery_mode(); }
int         dados_sync_pendente(void) { return data_sync_pending(); }
int         dados_sync_em_recuo(void) { return data_sync_in_backoff(); }
void        dados_marcar_sujo(int leve) { data_mark_dirty(leve); }
void        dados_fs_travar(void) { data_fs_lock(); }
void        dados_fs_liberar(void) { data_fs_unlock(); }
void        dados_desc_zerar(void) { data_flush_reset(); }
const char *dados_cliente_id(void) { return data_client_id(); }
void        dados_uuid(char *dst, unsigned size) { data_uuid(dst, size); }
int         dados_despedida_ler(void) { return data_farewell_read(); }
void        dados_despedida_fim(void) { data_farewell_finish(); }
void        dados_descarregar_e_sair(void) { data_flush_and_exit(); }
int         dados_persistente(void) { return data_persistent(); }
