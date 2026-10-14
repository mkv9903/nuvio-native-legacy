#ifndef NV_TITLE_ACTION_CONTEXT_H
#define NV_TITLE_ACTION_CONTEXT_H
#include "catalog_item.h"

/* An owned title and its reversible action's intent. The generic notice must
 * clone/release this context rather than memcpy an opaque Rust owner. */
typedef struct { CatItem title; int intent; } TitleActionContext;
void title_action_context_init(TitleActionContext *context, const CatItem *title, int intent);
void title_action_context_clear(TitleActionContext *context);
void *title_action_context_clone(const void *context);
void title_action_context_release(void *context);
#endif
