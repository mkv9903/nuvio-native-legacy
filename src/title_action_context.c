#include "title_action_context.h"
#include <stdlib.h>
#include <string.h>

void title_action_context_init(TitleActionContext *context, const CatItem *title, int intent) {
  if (!context) return;
  memset(context, 0, sizeof *context);
  catalog_item_copy(&context->title, title);
  context->intent = intent;
}
void title_action_context_clear(TitleActionContext *context) {
  if (!context) return;
  catalog_item_clear(&context->title);
  context->intent = 0;
}
void *title_action_context_clone(const void *source) {
  const TitleActionContext *context = source;
  TitleActionContext *copy;
  if (!context || !(copy = malloc(sizeof *copy))) return NULL;
  title_action_context_init(copy, &context->title, context->intent);
  return copy;
}
void title_action_context_release(void *context) {
  title_action_context_clear(context);
  free(context);
}
