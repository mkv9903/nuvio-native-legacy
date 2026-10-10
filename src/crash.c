#include "crash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(__linux__)
#include <ucontext.h>
#endif

#define CRASH_STACK_WORDS    192          // stack words copied
#define CRASH_MAPS_MAX       24000        // bytes of /proc/self/maps copied
#define CRASH_ALT_STACK_SIZE (64 * 1024)  // alternative stack: stack overflow is also SIGSEGV

static char crash_file[512];
static unsigned char crash_alt_stack[CRASH_ALT_STACK_SIZE];

// EVERYTHING IN THE HANDLER IS ONLY write(): no printf, malloc or locks.
// The process is broken and the heap may be precisely what broke.
static void crash_write_text(int fd, const char *s) {
  size_t n = strlen(s);
  while (n) {
    ssize_t w = write(fd, s, n);
    if (w <= 0) return;
    s += w;
    n -= (size_t)w;
  }
}

static void crash_write_hex(int fd, uintptr_t v) {
  char b[2 + sizeof v * 2 + 1];
  int i = (int)sizeof b - 1;
  b[i] = 0;
  do {
    b[--i] = "0123456789abcdef"[v & 15];
    v >>= 4;
  } while (v && i > 2);
  b[--i] = 'x';
  b[--i] = '0';
  crash_write_text(fd, b + i);
}

static void crash_write_field(int fd, const char *name, uintptr_t v) {
  crash_write_text(fd, name);
  crash_write_hex(fd, v);
}

static void crash_handler(int sig, siginfo_t *si, void *ctx) {
  uintptr_t pc = 0, lr = 0, sp = 0;
  int fd = open(crash_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#if defined(__linux__) && defined(__arm__)
  {
    ucontext_t *u = ctx;
    pc = u->uc_mcontext.arm_pc;
    lr = u->uc_mcontext.arm_lr;
    sp = u->uc_mcontext.arm_sp;
  }
#elif (defined(__linux__) && defined(__aarch64__))
  {
    ucontext_t *u = ctx;
    pc = u->uc_mcontext.pc;
    lr = u->uc_mcontext.regs[30];
    sp = u->uc_mcontext.sp;
  }
#else
  (void)ctx;
#endif
  if (fd >= 0) {
    crash_write_field(fd, "sinal=", (uintptr_t)sig);
    crash_write_field(fd, " codigo=", (uintptr_t)(si ? si->si_code : 0));
    crash_write_field(fd, " addr=", (uintptr_t)(si ? si->si_addr : 0));
    crash_write_field(fd, " pc=", pc);
    crash_write_field(fd, " lr=", lr);
    crash_write_field(fd, " sp=", sp);
    crash_write_text(fd, "\npilha=");
    // Raw stack: reader separates code pointers later. Without sp
    // (target without registers) nothing to copy. Word aligned.
    if (sp && !(sp & (sizeof(uintptr_t) - 1)))
      for (int i = 0; i < CRASH_STACK_WORDS; i++) {
        crash_write_hex(fd, ((uintptr_t *)sp)[i]);
        crash_write_text(fd, " ");
      }
    crash_write_text(fd, "\nmapas:\n");
    {
      int m = open("/proc/self/maps", O_RDONLY);
      if (m >= 0) {
        char b[2048];
        long total = 0;
        ssize_t n;
        while (total < CRASH_MAPS_MAX && (n = read(m, b, sizeof b)) > 0) {
          ssize_t o = 0;
          while (o < n) {
            ssize_t w = write(fd, b + o, (size_t)(n - o));
            if (w <= 0) break;
            o += w;
          }
          total += n;
        }
        close(m);
      }
    }
    close(fd);
  }
  crash_write_text(1, "[crash] fatal signal: report recorded\n");
  // SA_RESETHAND already restored default action: returning repeats failure
  // and the OS terminates process normally (generating its crash report).
  if (sig == SIGABRT) {
    signal(SIGABRT, SIG_DFL);
    raise(SIGABRT);
  }
}

void crash_arm(const char *file) {
  static const int signals[] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL };
  struct sigaction sa;
  stack_t alt;
  if (!file || !file[0] || strlen(file) >= sizeof crash_file) return;
  snprintf(crash_file, sizeof crash_file, "%s", file);
  alt.ss_sp = crash_alt_stack;
  alt.ss_size = sizeof crash_alt_stack;
  alt.ss_flags = 0;
  sigaltstack(&alt, NULL);
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = crash_handler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);
  for (size_t i = 0; i < sizeof signals / sizeof *signals; i++)
    sigaction(signals[i], &sa, NULL);
}

// --- reading on next launch --------------------------------------------------
typedef struct {
  uintptr_t start, end, offset;
  int exec;
  char name[96];
} CrashMap;

static const char *crash_basename(const char *path) {
  const char *b = strrchr(path, '/');
  return b ? b + 1 : path;
}

// "module+0xoffset" for an address; 0 if not falling into any mapped range.
static int crash_resolve_addr(const CrashMap *m, int n, uintptr_t e, int exec_only, char *out, size_t cap) {
  for (int i = 0; i < n; i++) {
    if (e < m[i].start || e >= m[i].end || (exec_only && !m[i].exec)) continue;
    snprintf(out, cap, "%s+0x%lx", m[i].name[0] ? m[i].name : "?", (unsigned long)(e - m[i].start + m[i].offset));
    return 1;
  }
  return 0;
}

int crash_report(const char *file) {
  FILE *f = file ? fopen(file, "rb") : NULL;
  char *txt, *stack, *maps, *p;
  CrashMap *m;
  int nm = 0, capm = 512;
  long size;
  size_t read_bytes;
  unsigned long sig = 0, code = 0, addr = 0, pc = 0, lr = 0, sp = 0;
  char where[160];
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size <= 0 || size > 256 * 1024) {
    fclose(f);
    remove(file);
    return 0;
  }
  txt = malloc((size_t)size + 1);
  m = calloc((size_t)capm, sizeof *m);
  if (!txt || !m) {
    free(txt);
    free(m);
    fclose(f);
    return 0;
  }
  read_bytes = fread(txt, 1, (size_t)size, f);
  txt[read_bytes] = 0;
  fclose(f);
  remove(file);
  if (sscanf(txt, "sinal=%lx codigo=%lx addr=%lx pc=%lx lr=%lx sp=%lx", &sig, &code, &addr, &pc, &lr, &sp) < 3) {
    printf("[crash] unreadable report (%ld bytes)\n", size);
    free(txt);
    free(m);
    return 1;
  }
  stack = strstr(txt, "\npilha=");
  maps = strstr(txt, "\nmapas:\n");
  if (maps) {
    *maps = 0;
    maps += 8;
  }
  for (p = maps; p && *p && nm < capm; ) {
    char *end_line = strchr(p, '\n'), perm[8] = "", path[256] = "";
    unsigned long a, b, d;
    if (end_line) *end_line = 0;
    if (sscanf(p, "%lx-%lx %7s %lx %*s %*s %255[^\n]", &a, &b, perm, &d, path) >= 4) {
      m[nm].start = a;
      m[nm].end = b;
      m[nm].offset = d;
      m[nm].exec = perm[2] == 'x';
      snprintf(m[nm].name, sizeof m[nm].name, "%s", crash_basename(path));
      nm++;
    }
    if (!end_line) break;
    p = end_line + 1;
  }
  printf("[crash] previous session crashed with signal %lu (code %lu), address 0x%lx\n", sig, code, addr);
  if (crash_resolve_addr(m, nm, pc, 0, where, sizeof where)) printf("[crash] pc %s\n", where);
  else printf("[crash] pc 0x%lx (outside any module)\n", pc);
  if (crash_resolve_addr(m, nm, lr, 0, where, sizeof where)) printf("[crash] lr %s\n", where);
  // From stack, only interested in what points to CODE: return address candidates.
  // Not an exact unwinding (old frame leftovers exist), but with package symbols
  // shows path to the crash.
  if (stack) {
    int found = 0;
    for (p = stack + 7; *p && *p != '\n' && found < 24; ) {
      char *end_num;
      unsigned long v = strtoul(p, &end_num, 16);
      if (end_num == p) break;
      if (crash_resolve_addr(m, nm, v, 1, where, sizeof where)) {
        printf("[crash] stack %s\n", where);
        found++;
      }
      p = end_num;
      while (*p == ' ') p++;
    }
  }
  fflush(stdout);
  free(txt);
  free(m);
  return 1;
}

// Backward compatibility wrappers
void queda_armar(const char *arquivo) { crash_arm(arquivo); }
int  queda_relatar(const char *arquivo) { return crash_report(arquivo); }
