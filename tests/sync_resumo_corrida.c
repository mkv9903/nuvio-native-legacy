// A worker publishes complete summaries while the UI reads them. Every field
// must come from one publication, including the fixed text and numeric branch.
#include "../src/sync.c"
#include <assert.h>
#include <stdatomic.h>
#include <unistd.h>

const char *i18n(const char *text) { return text; }
static atomic_int started, stop;
static void *writer(void *unused) {
  (void)unused;
  atomic_store(&started, 1);
  for (int i = 0; i < 30000; ++i) {
    SyncResumo r = { .tipo = RES_CICLO, .n = {1, 2, 3, 4, 5} };
    resumoPublicar(&r);
    r = (SyncResumo){ .tipo = RES_CICLO, .n = {6, 7, 8, 9, 10}, .trakt = 1, .pend = 11 };
    resumoPublicar(&r);
    r = (SyncResumo){ .tipo = RES_FORA_COPIA, .http = 504, .data = "yesterday" };
    resumoPublicar(&r);
    resumoFixo("complete fixed summary");
    if (!(i % 50)) usleep(50);
  }
  atomic_store(&stop, 1);
  return NULL;
}
int main(void) {
  pthread_t worker;
  unsigned reads = 0;
  resumoFixo("complete fixed summary");
  assert(pthread_create(&worker, NULL, writer, NULL) == 0);
  while (!atomic_load(&started)) usleep(50);
  while (!atomic_load(&stop)) {
    const char *s = sync_resumo();
    assert(!strcmp(s, "complete fixed summary") ||
           !strcmp(s, "1 addons · 2 progressos · 3 vistos · 4 na lista · 5 coleções") ||
           !strcmp(s, "6 addons · 7 progressos · 8 vistos · 9 na lista · 10 coleções · Trakt · 11 pendentes") ||
           !strcmp(s, "servidor da conta fora do ar (HTTP 504) · usando a cópia de yesterday"));
    reads++;
  }
  assert(pthread_join(worker, NULL) == 0);
  assert(reads > 0);
  printf("sync_resumo_corrida: %u complete UI snapshots\n", reads);
  return 0;
}
