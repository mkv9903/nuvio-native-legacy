/* Retained addon screen controller. Layout and remote navigation are unchanged;
 * accepted actions are saved and synchronized by the Rust addon capability. */
#include "addon_screen.h"
#include "addon_registry.h"
#include "descoberta.h"
#include "addons.h"
#include "gfx.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"

#define ROW_HEIGHT 96.0f
#define ROW_GAP 8.0f
#define TOP (NV_MARGEM_Y + 118.0f)
#define BOTTOM (NV_TELA_H - NV_MARGEM_Y - 48.0f)

static int focus, exit_requested, changed;
static float scroll_y;
static float focus_animation[64];

void addon_screen_open(void) {
  focus = 0; exit_requested = 0; changed = 0; scroll_y = 0.0f;
  addons_sondar_manifestos();
}
int addon_screen_wants_exit(void) {
  /* Batch Home reloading when leaving, just as the original screen did. */
  if (exit_requested && changed) {
    changed = 0;
    desc_tirar_fileiras_de_addons_ausentes();
    desc_repetir();
  }
  return exit_requested;
}
void addon_screen_event(const SDL_Event *event) {
  int count = addons_n();
  if (event->type != SDL_KEYDOWN) return;
  switch (event->key.keysym.sym) {
    case SDLK_UP: if (focus > 0) focus--; break;
    case SDLK_DOWN: if (focus < count - 1) focus++; break;
    case SDLK_RETURN:
      if (count > 0 && addons_alternar(focus) >= 0) changed = 1;
      break;
    case SDLK_AC_BACK: case SDLK_LEFT: exit_requested = 1; break;
    default: break;
  }
}
void addon_screen_update(float dt, Uint32 now) {
  int i, count = addons_n();
  float target;
  (void)now;
  if (focus >= count) focus = count > 0 ? count - 1 : 0;
  for (i = 0; i < count && i < 64; i++)
    focus_animation[i] = anim_mola(focus_animation[i], i == focus ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  target = (float)focus * (ROW_HEIGHT + ROW_GAP);
  if (target - scroll_y > BOTTOM - TOP - ROW_HEIGHT) scroll_y = target - (BOTTOM - TOP - ROW_HEIGHT);
  if (target - scroll_y < 0.0f) scroll_y = target;
}
void addon_screen_draw(Uint32 now) { (void)now; settings_draw_addons(focus); }
