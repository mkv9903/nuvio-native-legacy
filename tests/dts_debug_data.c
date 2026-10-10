#include "app_id.h"
#include "dados.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void checkSentinel(const char *path) {
  char sentinel[40] = {0};
  FILE *file = fopen(path, "r"); assert(file);
  assert(fread(sentinel, 1, sizeof sentinel - 1, file)); fclose(file);
  assert(!strcmp(sentinel, "production-sentinel"));
}

int main(int argc,char **argv) {
  assert(argc==3);
  char root[512],expected[600];
  snprintf(root,sizeof(root),"%s/%s",argv[1],argv[2]); assert(!mkdir(root,0700));
  char legacyDir[600],legacy[640],production[600];
  snprintf(legacyDir,sizeof legacyDir,"%s/.nuvio",root); assert(!mkdir(legacyDir,0700));
  snprintf(legacy,sizeof legacy,"%s/sessao.txt",legacyDir);
  snprintf(production,sizeof production,"%s/sessao.txt",root);
  FILE *file=fopen(production,"w"); assert(file); fputs("production-sentinel",file); fclose(file);
  file=fopen(legacy,"w"); assert(file); fputs("production-sentinel",file); fclose(file);
#ifdef NV_DTS_DEBUG
  char normalDir[600],normalSession[640];
  snprintf(normalDir,sizeof normalDir,"%s/.%s",root,NV_APP_ID_PRODUCTION);
  assert(!mkdir(normalDir,0700));
  snprintf(normalSession,sizeof normalSession,"%s/sessao.txt",normalDir);
  file=fopen(normalSession,"w"); assert(file); fputs("production-sentinel",file); fclose(file);
#endif
  int envMode = !strcmp(argv[2],"env") || !strcmp(argv[2],"env-data");
  unsetenv("NUVIO_DATA"); unsetenv("NUVIO_DADOS");
  if(envMode) {
    unsetenv("HOME");
    if(!strcmp(argv[2],"env-data")) {
      setenv("NUVIO_DATA",root,1);
      setenv("NUVIO_DADOS",legacyDir,1); // English alias wins over the old alias.
    } else setenv("NUVIO_DADOS",root,1);
  } else setenv("HOME",root,1);
#ifdef NV_DTS_DEBUG
  snprintf(expected,sizeof(expected),envMode?"%s/%s":"%s/.%s",root,NV_APP_ID);
#else
  if(envMode) snprintf(expected,sizeof(expected),"%s",root);
  else snprintf(expected,sizeof(expected),"%s/.%s",root,NV_APP_ID);
#endif
  dados_iniciar(root); assert(!strcmp(dados_dir(),expected));
#ifdef NV_DTS_DEBUG
  int isolated = 1;
#else
  int isolated = !envMode;
#endif
  if(isolated) {
    assert(!dados_ler("sessao.txt"));
    assert(dados_gravar("sessao.txt","fork-only"));
    char *saved = dados_ler("sessao.txt"); assert(saved && !strcmp(saved,"fork-only")); free(saved);
  } else {
    char *saved = dados_ler("sessao.txt");
    assert(saved && !strcmp(saved,"production-sentinel")); free(saved);
  }
  checkSentinel(production);
  checkSentinel(legacy);
#ifdef NV_DTS_DEBUG
  checkSentinel(normalSession);
#endif
  return 0;
}
