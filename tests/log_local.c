/* Local-file logging survives removal of all upload controls. No GL or network. */
#include "../src/log.c"
#include <assert.h>
#include <unistd.h>

int ajustes_idioma(void) { return IDIOMA_EN; }

static int key(Uint32 type, SDL_Keycode sym) {
  SDL_Event event = {0};
  event.type = type;
  event.key.keysym.sym = sym;
  event.key.keysym.scancode = SDL_GetScancodeFromKey(sym);
  return log_event(&event);
}

int main(void) {
  char dir[] = "/tmp/nuvio-log-local-XXXXXX", path[256];
  FILE *file;
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  data_init(dir);
  snprintf(path, sizeof path, "%s/session.log", dir);
  setenv("NUVIO_LOG", path, 1);
  assert(!strcmp(log_file_path(), path));
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  file = fopen(path, "w"); assert(file);
  fputs("[net] request https://example.test/private-token/video.mkv\n"
        "[video] decode falhou\n[system] local log ready\n", file);
  fclose(file);

  log_open();
  assert(log_is_open() && !semFonte && nLin == 3 && nVis == 3);
  assert(strstr(lin[0].t, "https://example.test/"));
  assert(!strstr(lin[0].t, "private-token"));
  assert(nErros == 1 && cont[RG_TUDO] == 3);
  assert(key(SDL_KEYDOWN, SDLK_UP) && pausado && foco == 2);
  assert(key(SDL_KEYDOWN, SDLK_DOWN) && !pausado);

  // The last filter is a stable boundary after removal of the send button.
  segEd = 1; area = RG_SISTEMA; filtrar();
  assert(key(SDL_KEYDOWN, SDLK_RIGHT));
  assert(segEd && area == RG_SISTEMA && log_is_open());
  assert(key(SDL_KEYDOWN, SDLK_LEFT) && area == RG_IMAGENS);
  assert(key(SDL_KEYDOWN, SDLK_RETURN) && !segEd);
  assert(key(SDL_KEYUP, SDLK_RETURN));
  area = RG_TUDO; filtrar();
  assert(key(SDL_KEYDOWN, SDLK_RETURN) && pausado && detalhe);
  assert(key(SDL_KEYUP, SDLK_RETURN));

  // Live refresh keeps data local; leaving the modal consumes key release.
  file = fopen(path, "a"); assert(file);
  fputs("[net] new local result\n", file); fclose(file);
  seguir();
  assert(nLin == 4 && nVis == 4 && !pausado);
  assert(key(SDL_KEYDOWN, SDLK_ESCAPE) && !log_is_open());
  assert(key(SDL_KEYUP, SDLK_ESCAPE));
  assert(!key(SDL_KEYDOWN, SDLK_DOWN));
  assert(key(SDL_KEYDOWN, SDLK_F9) && log_is_open());
  assert(key(SDL_KEYUP, SDLK_F9));
  assert(key(SDL_KEYDOWN, SDLK_F9) && !log_is_open());
  assert(key(SDL_KEYUP, SDLK_F9));

  // Display redaction must not rewrite or destroy the original local log.
  file = fopen(path, "r"); assert(file);
  { char original[512]; assert(fgets(original, sizeof original, file));
    assert(strstr(original, "private-token")); }
  fclose(file);
  unlink(path);
  data_delete("registro-aberto.txt");
  rmdir(dir);
  SDL_Quit();
  puts("PASS: local log, redaction, filters, modal keys and live refresh");
  return 0;
}
