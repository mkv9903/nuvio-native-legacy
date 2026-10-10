#include "startup.h"
#ifdef NV_WEBOS
#include "crash.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define FILE_STAGE  "/tmp/space.mkv.nuvio.native-startup.txt"
#define FILE_PREV   "/tmp/space.mkv.nuvio.native-startup-previous.txt"
#define FILE_CRASH  "/tmp/space.mkv.nuvio.native-startup-crash.txt"
#define STAGE_END   "quadro-1"   // reached this point = not a startup failure

void startup_stage(const char *name) {
  char b[96];
  int n = snprintf(b, sizeof b, "%s t=%ld\n", name, (long)time(NULL));
  int fd = open(FILE_STAGE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0 || n <= 0) { if (fd >= 0) close(fd); return; }
  if (write(fd, b, (size_t)n) < 0) { /* untraceable failure */ }
  close(fd);
}

// Before any other constructor (default is 65535; libtorrent and FFmpeg
// are in the binary, and their static C++ runs in that range).
__attribute__((constructor(101)))
static void startup_constructor(void) {
  rename(FILE_STAGE, FILE_PREV);
  startup_stage("construtores");
  crash_arm(FILE_CRASH);
}

void startup_report(void) {
  char b[160] = "";
  struct utsname u;
  char mem[64] = "";
  FILE *f = fopen(FILE_PREV, "r");
  if (f) {
    if (!fgets(b, sizeof b, f)) b[0] = 0;
    fclose(f);
    unlink(FILE_PREV);
  }
  f = fopen("/proc/meminfo", "r");
  if (f) {
    char l[96];
    unsigned long kb;
    while (fgets(l, sizeof l, f))
      if (sscanf(l, "MemTotal: %lu", &kb) == 1) {
        snprintf(mem, sizeof mem, "MemTotal=%luMB", kb / 1024);
        break;
      }
    fclose(f);
  }
  if (uname(&u) == 0) printf("[arranque] kernel=%s %s %s\n", u.release, u.machine, mem);
  b[strcspn(b, "\n")] = 0;
  if (b[0] && strncmp(b, STAGE_END, sizeof STAGE_END - 1) != 0)
    printf("[arranque] sessao anterior parou na etapa: %s\n", b);
  crash_report(FILE_CRASH);
  fflush(stdout);
}
#else
void startup_stage(const char *name) { (void)name; }
void startup_report(void) {}
#endif

// Backward compatibility wrappers
void arranque_etapa(const char *nome) { startup_stage(nome); }
void arranque_relatar(void) { startup_report(); }
