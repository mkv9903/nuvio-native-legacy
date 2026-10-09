// SAFE MODE: the app automatically reverts a setting change that crashed it.
//
// THE PROBLEM. Settings like "Home rows" at 40, "Items per row" at 24,
// 4K interface or glass UI consume memory and GPU that a 1 GB TV might
// not have. When app crashes because of this, user reopens and it crashes AGAIN,
// because the crash-triggering value is persisted in settings file: a loop that
// can only be broken by clearing data or reinstalling. The owner requested that memory
// warnings have consequences — "if the app closes, it automatically reverts to previous value".
//
// HOW IT WORKS. Three parts, and this module only contains the second and third;
// which settings are risky and how to restore them is known by ajustes.c.
//
//   1. JOURNAL. When changing a RISKY setting (ajustes.c, RISKS), ajustes.c
//      calls safemode_changed(key, prev, new). Change enters journal
//      (seguro.txt, data_write: atomic, on this device only) as PROVISIONAL.
//   2. CONFIRMATION. A provisional change becomes CONFIRMED when app stayed
//      SAFE_CONFIRM_S (3 min) open after it, or on clean exit (safemode_shutdown).
//      Confirmed, it is never reverted again by this module.
//   3. STARTUP. safemode_init receives verdict from avisos.c about previous session
//      (did not say goodbye = crashed; on Tizen this already considers farewell
//      mark in localStorage, so "TV hid app" does not count as crash).
//      If crashed and there is a PROVISIONAL change, returns each to previous value
//      via `apply` hook and marks it REVERTED. Decision is made here;
//      user notification is from ajustes.c, which knows UI labels.
//
// CRASH LOOP. Without provisional change the fault is not from an identifiable setting
// (could be catalog, network, TV). Two crashes in a row before session completes
// SAFE_STABLE_S (1 min) => next session runs in SAFE PROFILE: ajustes.c disables in-memory
// heavy features (glass, 4K, immersive theme, trailers, extra rows/items) WITHOUT touching file.
// If session in safe profile still crashes quickly, safe values become persisted
// (SAFE_PERSIST_SAFE) and user is notified; previous values remain in notice.
//
// "STABLE" IS A DURABLE WRITE, not a heartbeat. Previous session lifetime cannot come
// from a file that Tizen might not have flushed to IndexedDB (data_write_light),
// otherwise every crash would look fast. So journal writes with data_write when
// session passes 60 s (stable=1, fast=0) and on each confirmation; light heartbeat
// only feeds log "t=Ns".
#ifndef NV_SAFEMODE_H
#define NV_SAFEMODE_H

#define SAFE_CONFIRM_S   180   // provisional -> confirmed
#define SAFE_STABLE_S     60   // session passing this does not count as fast crash
#define SAFE_LOOP_COUNT    2   // consecutive fast crashes triggering safe profile
#define SAFE_MAX_CHANGES  12
#define SAFE_KEY_LEN      32

enum { SAFE_PROVISIONAL = 0, SAFE_CONFIRMED = 1, SAFE_REVERTED = 2 };

typedef struct {
  union { char key[SAFE_KEY_LEN]; char chave[SAFE_KEY_LEN]; };
  union { int prev; int ant; };
  union { int new_val; int novo; };
  union { int state; int estado; };
  union { int session; int sessao; };
  union { long timestamp; long quando; };
  union { unsigned uptime_s; unsigned desde; };
} SafeChange;

// Restoration hook. Returns 1 if reverted, 0 if not applicable (setting already had
// value other than `new_val` — user already corrected it, or prior reversion touched it).
typedef int (*SafeApply)(const char *key, int new_val, int prev);

enum { SAFE_NORMAL = 0, SAFE_PROFILE_SAFE, SAFE_PERSIST_SAFE };

typedef struct {
  union { int mode; int modo; };
  union { int crashed; int caiu; };
  union { int reverted_count; int nRevertidas; };
  union { SafeChange reverted[SAFE_MAX_CHANGES]; SafeChange revertidas[SAFE_MAX_CHANGES]; };
  union { unsigned last_uptime_s; unsigned ultimoT; };
  union { int fast_crashes; int rapidas; };
  union { int session; int sessao; };
} SafeDecision;

// At startup, AFTER settings are read and avisos_init rendered verdict.
const SafeDecision *safemode_init(int prev_crashed, long now, SafeApply apply);

// Risky setting changed. If `new_val` returns to original `prev` of provisional
// for same key, entry is removed (user reverted manually).
void safemode_changed(const char *key, int prev, int new_val, long now, unsigned uptime_s);

// Setting changed WITHOUT increasing risk.
void safemode_adjusted(const char *key, int new_val, int is_risky);

// Call per frame (cheap): every ~30 s records light signal; at 60 s marks
// session stable; confirms changes older than 3 min.
void safemode_heartbeat(unsigned uptime_s);

// Clean exit: confirms all, closes session in journal.
void safemode_shutdown(void);

// 1 if this session runs in safe profile (ajustes.c checks on every read).
int  safemode_profile_active(void);

// First-time notices ("More rows use more memory..."): bitmask in journal.
enum { SAFE_NOTICE_ROWS = 1, SAFE_NOTICE_ITEMS = 2 };
int  safemode_notice_seen(int bit);
void safemode_notice_mark(int bit);

// Reading (Settings screen, tests).
int  safemode_change_count(void);
const SafeChange *safemode_change(int i);
int  safemode_provisional_count(void);

#define SAFE_FILE "seguro.txt"

// Backward-compatibility shims
#define SEG_CONFIRMA_S       SAFE_CONFIRM_S
#define SEG_ESTAVEL_S        SAFE_STABLE_S
#define SEG_LACO_N           SAFE_LOOP_COUNT
#define SEG_MAX_MUD          SAFE_MAX_CHANGES
#define SEG_CHAVE            SAFE_KEY_LEN

#define SEG_PROVISORIA       SAFE_PROVISIONAL
#define SEG_CONFIRMADA       SAFE_CONFIRMED
#define SEG_REVERTIDA        SAFE_REVERTED

#define SEG_NORMAL           SAFE_NORMAL
#define SEG_PERFIL_SEGURO    SAFE_PROFILE_SAFE
#define SEG_PERSISTIR_SEGURO SAFE_PERSIST_SAFE

#define SEG_AVISO_FILEIRAS   SAFE_NOTICE_ROWS
#define SEG_AVISO_ITENS      SAFE_NOTICE_ITEMS

#define SEG_ARQ              SAFE_FILE

typedef SafeChange   SegMud;
typedef SafeApply    SegAplicar;
typedef SafeDecision SegDecisao;

const SegDecisao *seguro_iniciar(int anteriorCaiu, long agora, SegAplicar aplicar);
void seguro_mudou(const char *chave, int ant, int novo, long agora, unsigned uptimeS);
void seguro_ajustou(const char *chave, int novo, int arriscado);
void seguro_batida(unsigned uptimeS);
void seguro_encerrar(void);
int  seguro_perfil_ativo(void);
int  seguro_aviso_visto(int bit);
void seguro_aviso_marcar(int bit);
int  seguro_n_mud(void);
const SegMud *seguro_mud(int i);
int  seguro_n_provisorias(void);

#endif
