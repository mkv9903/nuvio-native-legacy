/* Invoke production native workers. Assertions and provider fixtures live in
 * Rust; this file only bridges C-owned results and SDL event layout. */
#include "addons.h"
#include "addon_screen.h"
#include "fontecache.h"
#include "net.h"
#include "streams.h"
#include <stdlib.h>

int native_addons_test_source_count(const char *id, const char *type, const char *base) {
  Stream *sources = NULL;
  int count;
  net_prepare();
  count = addons_consultar(id, type, base, 3, NULL, NULL, &sources);
  free(sources);
  return count;
}
void native_addons_test_key(int key) {
  SDL_Event event = {0};
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = (SDL_Keycode)key;
  addon_screen_event(&event);
}
