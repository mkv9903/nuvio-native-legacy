#ifndef NV_SAVE_NOTICE_H
#define NV_SAVE_NOTICE_H
#include "catalogo.h"
#include <SDL2/SDL.h>

/* First save: 1 defers the action until the existing destination prompt is
 * answered. Back uses the default. Removing a title never opens the prompt. */
int save_notice_ask(const CatItem *title, int add);
void save_notice_show(const CatItem *title, int added);
void save_notice_action(const char *key, int button);
void save_notice_step(Uint32 now);
void save_notice_execute(const CatItem *title, int add);
int save_notice_prompt_open(void);
const char *save_notice_destination(void);
void save_notice_release(void);
#endif
