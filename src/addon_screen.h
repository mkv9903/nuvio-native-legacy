/* Addon screen: Rust-owned account data, retained C layout/navigation. */
#ifndef NV_ADDON_SCREEN_H
#define NV_ADDON_SCREEN_H
#include <SDL2/SDL.h>
void addon_screen_open(void);
void addon_screen_event(const SDL_Event *event);
void addon_screen_update(float dt, Uint32 now);
void addon_screen_draw(Uint32 now);
int addon_screen_wants_exit(void);
#endif
