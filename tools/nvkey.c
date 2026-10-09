// Injects keys into the TV remote control input device.
//
// Exists to navigate in ANY app — including Apple's, which is the
// reference — and capture the same screen on both. Without this, comparing our app
// with the original required someone with remote in hand.
//
// Usage: nvkey /dev/input/event1 up down left right ok back ...
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int codigo(const char *nome) {
  if (!strcmp(nome, "up"))    return KEY_UP;
  if (!strcmp(nome, "down"))  return KEY_DOWN;
  if (!strcmp(nome, "left"))  return KEY_LEFT;
  if (!strcmp(nome, "right")) return KEY_RIGHT;
  if (!strcmp(nome, "ok"))    return KEY_ENTER;
  if (!strcmp(nome, "back"))  return KEY_BACK;
  if (!strcmp(nome, "home"))  return KEY_HOMEPAGE;
  // accepts raw numeric code: LG remote Back is not kernel KEY_BACK,
  // and finding which it is requires experimentation
  if (nome[0] >= '0' && nome[0] <= '9') return atoi(nome);
  return -1;
}

static void manda(int fd, int tipo, int cod, int valor) {
  struct input_event e;
  memset(&e, 0, sizeof e);
  e.type = tipo; e.code = cod; e.value = valor;
  if (write(fd, &e, sizeof e) != (ssize_t)sizeof e) perror("write");
}

int main(int argc, char **argv) {
  if (argc < 3) { printf("usage: nvkey <device> <key>...\n"); return 1; }
  int fd = open(argv[1], O_WRONLY);
  if (fd < 0) { perror("open"); return 1; }
  for (int i = 2; i < argc; i++) {
    int c = codigo(argv[i]);
    if (c < 0) { printf("unknown key: %s\n", argv[i]); continue; }
    manda(fd, EV_KEY, c, 1); manda(fd, EV_SYN, SYN_REPORT, 0);
    usleep(60000);
    manda(fd, EV_KEY, c, 0); manda(fd, EV_SYN, SYN_REPORT, 0);
    // pause between keys: TV navigation has animations, and without waiting the
    // application drops subsequent keys
    usleep(420000);
  }
  close(fd);
  return 0;
}
