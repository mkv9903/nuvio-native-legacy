/* The existing profile picker retains its layout, materials, backgrounds,
 * animations and remote navigation. Rust owns selection and PIN verification.
 * Cached profiles are available on the first frame; confirmation resets on
 * each launch. No profile PIN is stored or logged. */
#ifndef NV_PROFILE_PICKER_H
#define NV_PROFILE_PICKER_H
#include <SDL2/SDL.h>

void profile_picker_init(void);
void profile_picker_event(const SDL_Event *event);
void profile_picker_update(float dt, Uint32 now);
void profile_picker_draw(Uint32 now);
int profile_picker_complete(void);
void profile_picker_keep_active(void);
int profile_picker_wants_logout(void);
int profile_picker_wants_retry(void);
/* Retain the chosen card while the app prepares its Home. */
void profile_picker_prepare(int enabled, Uint32 now);
int profile_picker_preparing(void);

#ifdef NV_PROFILE_PICKER_TEST
typedef struct {
  float focus[8], pin, light[6], light_target[6], fade[9];
  float wall_time, burst_time;
  int wall_count, particles, burst, burst_drawn;
  float ambient_fade;
  int ambient_current, ambient_previous;
  int ambient_source; /* zero: light, one: artwork, two: avatar */
  int continue_available[8];
  int pin_slot, pin_focus, verifying, pin_digits, complete;
} ProfilePickerTestState;
void profile_picker_test_state(ProfilePickerTestState *state);
void profile_picker_test_key(int key);
void profile_picker_test_activate(int slot, int id);
void profile_picker_test_focus_pin(int key);
#endif

#endif
