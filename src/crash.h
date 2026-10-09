// WHERE THE APP CRASHED. "The previous session did not say goodbye" (avisos.c) says that
// a crash happened, not where: in 2.0.0 two webOS 5+ TVs totaled 12 crashes and the log
// had not a single line about the cause. Here fatal signal records, in its own file,
// the signal, failure address, pc/lr, a stack snippet and the module map; next launch
// translates everything into "module+offset" and writes to log, which auto-report transmits.
//
// Only enabled where process is ours (webOS). On Android and .tpk the process belongs to
// ART / .NET, which handle SIGSEGV independently.
#ifndef NV_CRASH_H
#define NV_CRASH_H

// Installs handlers. `file` is the full path for the report; string is copied.
// Call once, early, on main thread.
void crash_arm(const char *file);

// Reads report from previous session, prints "[crash] ..." lines and deletes file.
// Returns 1 if report existed.
int  crash_report(const char *file);

// Backward-compatibility wrappers
void queda_armar(const char *arquivo);
int  queda_relatar(const char *arquivo);

#endif
