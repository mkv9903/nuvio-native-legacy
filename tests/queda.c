// O registrador de queda (queda.h): um filho morre com SIGSEGV de proposito e
// o pai confere que o relato foi gravado, lido e apagado.
#include "../src/queda.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  char arq[256];
  int st = 0;
  pid_t f;
  snprintf(arq, sizeof arq, "/tmp/nuvio-queda-teste-%d.txt", (int)getpid());
  remove(arq);
  assert(!queda_relatar(arq));                 // sem relato nao ha o que dizer
  f = fork();
  if (f == 0) {
    volatile int *nulo = (volatile int *)(long)0x10;
    queda_armar(arq);
    *nulo = 1;
    _exit(0);
  }
  waitpid(f, &st, 0);
  assert(WIFSIGNALED(st));                     // o sinal continua matando o processo
  { FILE *g = fopen(arq, "rb"); char b[64] = ""; assert(g); fread(b, 1, sizeof b - 1, g); fclose(g);
    assert(!strncmp(b, "sinal=0x", 8) && strstr(b, "addr=0x10")); }
  assert(queda_relatar(arq));
  assert(access(arq, F_OK) != 0);              // lido uma vez so
  // Relato com mapa: o endereco vira modulo+deslocamento.
  { FILE *g = fopen(arq, "wb");
    fputs("sinal=0xb codigo=0x1 addr=0x0 pc=0x10234 lr=0x76f01010 sp=0x7e000000\n"
          "pilha=0x1 0x10400 0x76f02000 0x5 \nmapas:\n"
          "00010000-00090000 r-xp 00000000 b3:02 100 /media/developer/apps/x/nuvio-proto.arm\n"
          "76f00000-76f80000 r-xp 00001000 b3:02 200 /usr/lib/libSDL2-2.0.so.0\n"
          "7e000000-7e100000 rw-p 00000000 00:00 0 [stack]\n", g);
    fclose(g); }
  fflush(stdout);
  { char sai[sizeof arq + 4]; FILE *g; char tudo[2048] = ""; int fd, velho;
    snprintf(sai, sizeof sai, "%s.out", arq);
    velho = dup(1); g = fopen(sai, "wb"); fd = fileno(g); dup2(fd, 1);
    assert(queda_relatar(arq));
    fflush(stdout); dup2(velho, 1); close(velho); fclose(g);
    g = fopen(sai, "rb"); fread(tudo, 1, sizeof tudo - 1, g); fclose(g); remove(sai);
    assert(strstr(tudo, "signal 11"));
    assert(strstr(tudo, "pc nuvio-proto.arm+0x234"));
    assert(strstr(tudo, "lr libSDL2-2.0.so.0+0x2010"));
    assert(strstr(tudo, "stack nuvio-proto.arm+0x400"));
    assert(strstr(tudo, "stack libSDL2-2.0.so.0+0x3000"));
    assert(!strstr(tudo, "[stack]")); }
  puts("queda: relato gravado no sinal, traduzido para modulo+deslocamento e apagado");
  return 0;
}
