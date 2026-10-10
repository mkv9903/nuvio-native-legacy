#include "app_id.h"
#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[512];
static char clientId[64];

// Tries to create the directory and write to it.
static int probe(const char *candidate) {
  char testPath[600];
  FILE *f;
  if (!candidate || !*candidate) return 0;
  mkdir(candidate, 0755);
  snprintf(testPath, sizeof testPath, "%s/.escrita", candidate);
  f = fopen(testPath, "w");
  if (!f) return 0;
  if (fputs("ok\n", f) < 0) { fclose(f); return 0; }
  if (fclose(f) != 0) return 0;
  remove(testPath);
  return 1;
}

void data_init(const char *artDir) {
  char homeDir[512], envHome[512], artHome[512];
  int isolated = strcmp(NV_APP_ID, NV_APP_ID_PRODUCTION) != 0;
  const char *env = getenv("NUVIO_DATA");
  if (!env || !*env) env = getenv("NUVIO_DADOS");
  const char *home = getenv("HOME");
  const char *candidates[5];
  int n = 0, i;

  if (env && *env) {
    if (!isolated) candidates[n++] = env;
    else {
      int size = snprintf(envHome, sizeof envHome, "%s/%s", env, NV_APP_ID);
      if (size > 0 && size < (int)sizeof envHome) candidates[n++] = envHome;
    }
  }
  if (home && *home) {
    int size = snprintf(homeDir, sizeof homeDir, isolated ? "%s/.%s" : "%s/.nuvio", home, NV_APP_ID);
    if (size > 0 && size < (int)sizeof homeDir) candidates[n++] = homeDir;
  }
  candidates[n++] = isolated ? "/media/developer/temp/" NV_APP_ID : "/media/developer/temp/nuvio";
  if (artDir && *artDir) {
    if (!isolated) candidates[n++] = artDir;
    else {
      int size = snprintf(artHome, sizeof artHome, "%s/.%s", artDir, NV_APP_ID);
      if (size > 0 && size < (int)sizeof artHome) candidates[n++] = artHome;
    }
  }

  for (i = 0; i < n; i++) {
    if (probe(candidates[i])) {
      snprintf(dir, sizeof dir, "%s", candidates[i]);
      printf("[data] writing to %s\n", dir);
      fflush(stdout);
      return;
    }
    printf("[data] rejected %s\n", candidates[i]);
  }
  dir[0] = 0;
  printf("[data] NO writable directory: session and settings will not survive next launch\n");
  fflush(stdout);
}

const char *data_dir(void) { return dir; }

char *data_path(char *dst, unsigned size, const char *name) {
  if (!dir[0] || !name || !*name) return NULL;
  snprintf(dst, size, "%s/%s", dir, name);
  return dst;
}

static int writeInternal(const char *name, const char *content) {
  char path[600], tmp[600];
  FILE *f;
  size_t n;
  if (!data_path(path, sizeof path, name)) return 0;
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  f = fopen(tmp, "w");
  if (!f) { return 0; }
  n = content ? strlen(content) : 0;
  if (n && fwrite(content, 1, n, f) != n) { fclose(f); remove(tmp); return 0; }
  if (fclose(f) != 0) { remove(tmp); return 0; }
  if (rename(tmp, path) != 0) { remove(tmp); return 0; }
  return 1;
}

int data_write(const char *name, const char *content) {
  return writeInternal(name, content);
}

int data_write_light(const char *name, const char *content) {
  return writeInternal(name, content);
}

char *data_read(const char *name) {
  char path[600];
  FILE *f;
  long n;
  char *buf;
  if (!data_path(path, sizeof path, name)) return NULL;
  f = fopen(path, "rb");
  if (!f) { return NULL; }
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) { fclose(f); return NULL; }
  buf = (char *)malloc((size_t)n + 1);
  if (!buf) { fclose(f); return NULL; }
  n = (long)fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[n] = 0;
  return buf;
}

int data_delete(const char *name) {
  char path[600];
  if (!data_path(path, sizeof path, name)) return 0;
  if (remove(path) != 0) { return 0; }
  return 1;
}

void data_uuid(char *dst, unsigned size) {
  static const char *hex = "0123456789abcdef";
  static int seeded;
  int i;
  if (size < 37) { if (size) dst[0] = 0; return; }
  if (!seeded) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)(size_t)dst);
    seeded = 1;
  }
  for (i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { dst[i] = '-'; continue; }
    if (i == 14) { dst[i] = '4'; continue; }
    if (i == 19) { dst[i] = hex[8 + (rand() & 3)]; continue; }
    dst[i] = hex[rand() & 15];
  }
  dst[36] = 0;
}

const char *data_client_id(void) {
  char *readBuf;
  if (clientId[0]) return clientId;

  readBuf = data_read("cliente.txt");
  if (readBuf) {
    char *end = readBuf + strlen(readBuf);
    while (end > readBuf && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) *--end = 0;
    if (readBuf[0]) snprintf(clientId, sizeof clientId, "%s", readBuf);
    free(readBuf);
    if (clientId[0]) return clientId;
  }

  data_uuid(clientId, sizeof clientId);

  { char line[64];
    snprintf(line, sizeof line, "%s\n", clientId);
    data_write("cliente.txt", line); }
  return clientId;
}

int data_persistent(void) {
  return dir[0] != 0;
}

// Binary backward-compatibility wrappers for legacy callers
void        dados_iniciar(const char *dirArte) { data_init(dirArte); }
const char *dados_dir(void) { return data_dir(); }
char       *dados_caminho(char *dst, unsigned size, const char *name) { return data_path(dst, size, name); }
int         dados_gravar(const char *nome, const char *conteudo) { return data_write(nome, conteudo); }
int         dados_gravar_leve(const char *nome, const char *conteudo) { return data_write_light(nome, conteudo); }
char       *dados_ler(const char *nome) { return data_read(nome); }
int         dados_apagar(const char *nome) { return data_delete(nome); }
const char *dados_cliente_id(void) { return data_client_id(); }
void        dados_uuid(char *dst, unsigned size) { data_uuid(dst, size); }
int         dados_persistente(void) { return data_persistent(); }
