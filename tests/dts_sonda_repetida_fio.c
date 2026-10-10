/* Fio residente da biblioteca falsa da LG: no webOS 3 a libplayerAPIs puxa
 * GLib/GObject/GStreamer, que sobem fios proprios ao carregar. Este imita
 * isso: um fio que roda codigo DESTA .so para sempre. Se o dlclose da sonda
 * descarregar a .so, o fio executa memoria sem mapa e o processo cai. */
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <unistd.h>
static volatile unsigned long batidas;
static void *girar(void *a) { (void)a; for (;;) { batidas++; usleep(1000); } return 0; }
__attribute__((constructor)) static void subir(void) {
  pthread_t t;
  if (!pthread_create(&t, 0, girar, 0)) pthread_detach(t);
}
