// Safe mode: journal of risky changes and startup decision. Policy
// is in the header (safemode.h); here only the file format and state machine reside.
//
// FORMAT of seguro.txt (text, one line per record, written completely and
// atomically via data_write):
//   nvseguro 1
//   c <session> <fast_crashes> <open> <stable> <safe_profile> <uptime> <notices>
//   m <session> <timestamp> <state> <prev> <new> <uptime_since> <key>
// `c` is the most recent session state; `m` is a change. Non-matching lines
// are ignored: a file from another version must not prevent the app from opening.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safemode.h"
#include "data.h"

static struct {
  int session, fast_crashes, is_open, is_stable, safe_profile;
  unsigned uptime, notices;
} S;
static SafeChange changes[SAFE_MAX_CHANGES];
static int num_changes;
static SafeDecision decision;
static int initialized;
static unsigned last_heartbeat;

static void save(int light) {
  char buf[SAFE_MAX_CHANGES * 96 + 128];
  size_t u = 0;
  int i;
  u += (size_t)snprintf(buf + u, sizeof buf - u, "nvseguro 1\n");
  u += (size_t)snprintf(buf + u, sizeof buf - u, "c %d %d %d %d %d %u %u\n",
                        S.session, S.fast_crashes, S.is_open, S.is_stable,
                        S.safe_profile, S.uptime, S.notices);
  for (i = 0; i < num_changes && u + 96 < sizeof buf; i++)
    u += (size_t)snprintf(buf + u, sizeof buf - u, "m %d %ld %d %d %d %u %s\n",
                          changes[i].session, changes[i].timestamp, changes[i].state,
                          changes[i].prev, changes[i].new_val, changes[i].uptime_s, changes[i].key);
  if (light) data_write_light(SAFE_FILE, buf);
  else      data_write(SAFE_FILE, buf);
}

static void load(void) {
  char *t, *p;
  memset(&S, 0, sizeof S);
  memset(changes, 0, sizeof changes);
  num_changes = 0;
  t = data_read(SAFE_FILE);
  if (!t) return;
  for (p = t; *p; ) {
    char *end_line = strchr(p, '\n');
    if (end_line) *end_line = 0;
    if (p[0] == 'c' && p[1] == ' ') {
      sscanf(p + 2, "%d %d %d %d %d %u %u", &S.session, &S.fast_crashes, &S.is_open,
             &S.is_stable, &S.safe_profile, &S.uptime, &S.notices);
    } else if (p[0] == 'm' && p[1] == ' ' && num_changes < SAFE_MAX_CHANGES) {
      SafeChange m;
      char key[SAFE_KEY_LEN];
      memset(&m, 0, sizeof m);
      if (sscanf(p + 2, "%d %ld %d %d %d %u %31s", &m.session, &m.timestamp,
                 &m.state, &m.prev, &m.new_val, &m.uptime_s, key) == 7 &&
          m.state >= SAFE_PROVISIONAL && m.state <= SAFE_REVERTED) {
        snprintf(m.key, sizeof m.key, "%s", key);
        changes[num_changes++] = m;
      }
    }
    if (!end_line) break;
    p = end_line + 1;
  }
  free(t);
}

static void confirm_provisionals(void) {
  int i;
  for (i = 0; i < num_changes; i++)
    if (changes[i].state == SAFE_PROVISIONAL) changes[i].state = SAFE_CONFIRMED;
}

const SafeDecision *safemode_init(int prev_crashed, long now, SafeApply apply) {
  int i, prov = 0, prev_profile;
  (void)now;
  load();
  memset(&decision, 0, sizeof decision);
  decision.crashed = prev_crashed ? 1 : 0;
  decision.last_uptime_s = S.uptime;
  prev_profile = S.safe_profile;

  if (prev_crashed) {
    // FAST CRASH = session was marked open in journal and never reached 60 s.
    // Without `is_open` (first launch of this version, file cleared) there is
    // no proof of how long it lasted, so when in doubt do not count.
    int fast = S.is_open && !S.is_stable;
    S.fast_crashes = fast ? S.fast_crashes + 1 : 0;
    for (i = 0; i < num_changes; i++) {
      if (changes[i].state != SAFE_PROVISIONAL) continue;
      prov++;
      if (!apply || apply(changes[i].key, changes[i].new_val, changes[i].prev)) {
        if (decision.reverted_count < SAFE_MAX_CHANGES) decision.reverted[decision.reverted_count++] = changes[i];
        printf("[seguro] revertido %s %d -> %d (sessao anterior caiu t=%us)\n",
               changes[i].key, changes[i].new_val, changes[i].prev, decision.last_uptime_s);
      } else {
        printf("[seguro] %s ja nao vale %d (sessao anterior caiu t=%us): nada a desfazer\n",
               changes[i].key, changes[i].new_val, decision.last_uptime_s);
      }
      changes[i].state = SAFE_REVERTED;
    }
    decision.fast_crashes = S.fast_crashes;
    if (prov > 0) {
      // An identifiable culprit was reverted: the next session is the trial.
      // Resetting here avoids enabling safe profile on top of the fix.
      S.fast_crashes = 0;
    } else if (S.fast_crashes >= SAFE_LOOP_COUNT) {
      // Crash loop without identifiable culprit. If previous session ALREADY ran in
      // safe profile and crashed quickly again, safe profile was not enough as a
      // temporary remedy: persist permanently, and notify user.
      decision.mode = prev_profile ? SAFE_PERSIST_SAFE : SAFE_PROFILE_SAFE;
      if (decision.mode == SAFE_PERSIST_SAFE) S.fast_crashes = 0;
    }
  } else {
    // Exited cleanly (or TV hid app): what was in trial passed.
    confirm_provisionals();
    S.fast_crashes = 0;
    decision.fast_crashes = 0;
  }

  S.session++;
  decision.session = S.session;
  S.is_open = 1;
  S.is_stable = 0;
  S.uptime = 0;
  S.safe_profile = decision.mode == SAFE_PROFILE_SAFE;
  initialized = 1;
  last_heartbeat = 0;
  save(0);
  printf("[seguro] sessao %d: anterior %s (t=%us), %d mudanca(s) em prova, quedas rapidas %d -> %s\n",
         S.session, prev_crashed ? "caiu" : "saiu limpo", decision.last_uptime_s, prov, decision.fast_crashes,
         decision.mode == SAFE_PROFILE_SAFE ? "PERFIL SEGURO nesta sessao"
         : decision.mode == SAFE_PERSIST_SAFE ? "valores seguros gravados"
         : "normal");
  fflush(stdout);
  return &decision;
}

void safemode_changed(const char *key, int prev, int new_val, long now, unsigned uptime_s) {
  int i;
  if (!initialized || !key || !key[0] || prev == new_val) return;
  for (i = 0; i < num_changes; i++)
    if (changes[i].state == SAFE_PROVISIONAL && !strcmp(changes[i].key, key)) {
      // Multiple changes to the same setting in the same trial: the "previous" that counts is
      // the FIRST, which had not crashed yet.
      if (new_val == changes[i].prev) {
        memmove(&changes[i], &changes[i + 1], sizeof changes[0] * (size_t)(num_changes - i - 1));
        num_changes--;
      } else {
        changes[i].new_val = new_val; changes[i].timestamp = now; changes[i].uptime_s = uptime_s;
      }
      save(0);
      return;
    }
  if (num_changes >= SAFE_MAX_CHANGES) {
    // Full: remove oldest already resolved; if all in trial, oldest anyway
    // (there aren't as many risky settings as ceiling).
    int old_idx = 0;
    for (i = 0; i < num_changes; i++)
      if (changes[i].state != SAFE_PROVISIONAL) { old_idx = i; break; }
    memmove(&changes[old_idx], &changes[old_idx + 1], sizeof changes[0] * (size_t)(num_changes - old_idx - 1));
    num_changes--;
  }
  memset(&changes[num_changes], 0, sizeof changes[0]);
  snprintf(changes[num_changes].key, sizeof changes[num_changes].key, "%s", key);
  changes[num_changes].prev = prev; changes[num_changes].new_val = new_val;
  changes[num_changes].state = SAFE_PROVISIONAL;
  changes[num_changes].session = S.session; changes[num_changes].timestamp = now; changes[num_changes].uptime_s = uptime_s;
  num_changes++;
  save(0);
  printf("[seguro] em prova %s %d -> %d (confirma em %d s ou na saida limpa)\n",
         key, prev, new_val, SAFE_CONFIRM_S);
  fflush(stdout);
}

void safemode_adjusted(const char *key, int new_val, int is_risky) {
  int i;
  if (!initialized || !key) return;
  for (i = 0; i < num_changes; i++)
    if (changes[i].state == SAFE_PROVISIONAL && !strcmp(changes[i].key, key)) {
      if (!is_risky || new_val == changes[i].prev) {
        memmove(&changes[i], &changes[i + 1], sizeof changes[0] * (size_t)(num_changes - i - 1));
        num_changes--;
      } else changes[i].new_val = new_val;
      save(0);
      return;
    }
}

void safemode_heartbeat(unsigned uptime_s) {
  int i, durable = 0;
  if (!initialized) return;
  S.uptime = uptime_s;
  if (!S.is_stable && uptime_s >= SAFE_STABLE_S) {
    S.is_stable = 1; S.fast_crashes = 0; durable = 1;
  }
  for (i = 0; i < num_changes; i++)
    if (changes[i].state == SAFE_PROVISIONAL &&
        uptime_s >= changes[i].uptime_s + SAFE_CONFIRM_S) {
      changes[i].state = SAFE_CONFIRMED;
      durable = 1;
      printf("[seguro] confirmado %s %d (aberto %u s depois da mudanca)\n",
             changes[i].key, changes[i].new_val, uptime_s - changes[i].uptime_s);
      fflush(stdout);
    }
  if (durable) { save(0); last_heartbeat = uptime_s; }
  else if (uptime_s >= last_heartbeat + 30) { save(1); last_heartbeat = uptime_s; }
}

void safemode_shutdown(void) {
  if (!initialized) return;
  confirm_provisionals();
  S.is_open = 0;
  S.fast_crashes = 0;
  save(0);
  initialized = 0;
}

int safemode_profile_active(void) { return initialized && S.safe_profile; }

int safemode_notice_seen(int bit) { return (S.notices & (unsigned)bit) != 0; }
void safemode_notice_mark(int bit) {
  if (S.notices & (unsigned)bit) return;
  S.notices |= (unsigned)bit;
  if (initialized) save(0);
}

int safemode_change_count(void) { return num_changes; }
const SafeChange *safemode_change(int i) { return i >= 0 && i < num_changes ? &changes[i] : NULL; }
int safemode_provisional_count(void) {
  int i, k = 0;
  for (i = 0; i < num_changes; i++) if (changes[i].state == SAFE_PROVISIONAL) k++;
  return k;
}

// Backward-compatibility wrappers
const SegDecisao *seguro_iniciar(int anteriorCaiu, long agora, SegAplicar aplicar) {
  return safemode_init(anteriorCaiu, agora, aplicar);
}
void seguro_mudou(const char *chave, int ant, int novo, long agora, unsigned uptimeS) {
  safemode_changed(chave, ant, novo, agora, uptimeS);
}
void seguro_ajustou(const char *chave, int novo, int arriscado) {
  safemode_adjusted(chave, novo, arriscado);
}
void seguro_batida(unsigned uptimeS) {
  safemode_heartbeat(uptimeS);
}
void seguro_encerrar(void) {
  safemode_shutdown();
}
int seguro_perfil_ativo(void) {
  return safemode_profile_active();
}
int seguro_aviso_visto(int bit) {
  return safemode_notice_seen(bit);
}
void seguro_aviso_marcar(int bit) {
  safemode_notice_mark(bit);
}
int seguro_n_mud(void) {
  return safemode_change_count();
}
const SegMud *seguro_mud(int i) {
  return safemode_change(i);
}
int seguro_n_provisorias(void) {
  return safemode_provisional_count();
}
