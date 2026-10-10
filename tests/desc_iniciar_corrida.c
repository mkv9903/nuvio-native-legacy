// Concurrent catalogue starts must create one worker. Hook the real lock and
// pthread_create path; reuse catalogue dependencies from montagem_cedo.c.
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
static void *montar(void *u);
static int testeTravar(pthread_mutex_t *m);
static int testeCriar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg);
#define pthread_mutex_lock(m) testeTravar(m)
#define pthread_create(t, a, f, arg) testeCriar(t, a, f, arg)
#define main montagem_cedo_main
#include "montagem_cedo.c"
#undef main
#undef pthread_mutex_lock
#undef pthread_create

static atomic_int corridaLigada, chegaram, montaresCriados;
static _Thread_local int ehCorredor;

static int testeTravar(pthread_mutex_t *m) {
  if (ehCorredor && atomic_load(&corridaLigada) && m == &listaTrava) {
    int i;
    ehCorredor = 0;
    atomic_fetch_add(&chegaram, 1);
    // os dois corredores se encontram aqui (teto de 2 s se um nao vier)
    for (i = 0; i < 2000 && atomic_load(&chegaram) < 2; i++) usleep(1000);
  }
  return pthread_mutex_lock(m);
}
static void *fioVazio(void *u) { (void)u; return NULL; }
static int testeCriar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) {
  if (f == montar) {
    atomic_fetch_add(&montaresCriados, 1);
    return pthread_create(t, a, fioVazio, NULL);
  }
  return pthread_create(t, a, f, arg);
}
static void volta_terminou(void) {   // o que montar() faz no fim, sem montar
  pthread_mutex_lock(&listaTrava); buscando = 0; pthread_mutex_unlock(&listaTrava);
}

static void *corredor(void *u) {
  (void)u;
  ehCorredor = 1;
  desc_iniciar();
  return NULL;
}

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHOU: "); falhas++; } else printf("ok: "); \
                             printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void) {
  pthread_t a, b;
  int n;
  setvbuf(stdout, NULL, _IOLBF, 0);

  // 1. dois desc_iniciar ao mesmo tempo
  atomic_store(&corridaLigada, 1);
  pthread_create(&a, NULL, corredor, NULL);
  pthread_create(&b, NULL, corredor, NULL);
  pthread_join(a, NULL); pthread_join(b, NULL);
  atomic_store(&corridaLigada, 0);
  n = atomic_load(&montaresCriados);
  CONFERE(n == 1, "dois desc_iniciar juntos largam UM montar (largaram %d)", n);
  volta_terminou();
  desc_iniciar();
  CONFERE(atomic_load(&montaresCriados) == 2, "a finished worker permits the next build");
  volta_terminou();

  printf(falhas ? "desc_iniciar_corrida: %d falha(s)\n" : "desc_iniciar_corrida: ok\n", falhas);
  return falhas ? 1 : 0;
}
