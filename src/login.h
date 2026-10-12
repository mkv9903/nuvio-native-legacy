/* Full-screen QR and email login. Existing rendering and remote navigation
 * consume Rust account snapshots; the backend owns authentication. */
#ifndef NV_LOGIN_H
#define NV_LOGIN_H
#include <SDL2/SDL.h>

void login_set_assets(const char *asset_directory);
/* Returns zero when no striped background texture is available. */
int login_draw_background(float alpha);
void login_release(void);
void login_init(void);
void login_event(const SDL_Event *event);
void login_update(float dt, Uint32 now);
void login_draw(Uint32 now);
int login_complete(void);
#ifdef NV_NATIVE_UI_TEST
int login_test_qr_ready(void);
void login_test_email_input(const char *email, const char *password);
#endif

#endif
