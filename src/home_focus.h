#ifndef NV_HOME_FOCUS_H
#define NV_HOME_FOCUS_H
#include "home.h"
#include "catalogo.h"
/* The focused transition owns its title and chosen artwork across frames. */
void home_focus_set(const HomeItem *item, const CatItem *title);
int home_focus_get(HomeItem *output);
void home_focus_forget(void);
void home_clear_focus(void);
#endif
