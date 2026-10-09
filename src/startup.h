// STARTUP TRACE (webOS only). #317 and #211: two LG UK65xx (2018, webOS 4.0)
// "close upon opening", without a byte of log. Normal log only exists after
// main() and the crash handler (crash.c) is only armed after SDL_Init, so
// a crash before that, or inside a static constructor of the binary,
// left nothing behind. Here:
//   - a constructor with priority 101 (before any other) arms the crash
//     handler to a /tmp file and writes the first stage;
//   - main.c records each subsequent stage (a write of a few bytes);
//   - the next startup, if previous failed to reach first frame, logs
//     "[startup] previous session stopped at stage <stage>" and interprets report.
// LOADER failure (missing lib, illegal instruction before any of our code runs)
// remains impossible to trace: only binary output via SSH reveals it.
#ifndef NV_STARTUP_H
#define NV_STARTUP_H

void startup_stage(const char *name);
// Call right after log freopen, still at the start of main().
void startup_report(void);

// Backward-compatibility wrappers
void arranque_etapa(const char *nome);
void arranque_relatar(void);

#endif
