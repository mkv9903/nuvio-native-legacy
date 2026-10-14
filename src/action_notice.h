#ifndef NV_ACTION_NOTICE_H
#define NV_ACTION_NOTICE_H
#include <stddef.h>

/* Post after committing an action. The island displays the message and opens
 * its existing modal with optional Undo. Offer Undo only for a real inverse. */
typedef void (*ActionUndo)(const void *context);
typedef void *(*ActionContextClone)(const void *context);
typedef void (*ActionContextRelease)(void *context);
typedef struct {
  const char *icon;
  const char *message;
  const char *title;
  const char *location;
  const char *thumbnail;
  const char *artwork;
  int kind;
  int animate_thumbnail;
  ActionUndo undo;
  const void *context;
  size_t context_size;
  /* Optional clone/release for contexts containing owned title references.
   * Plain contexts are copied by value and freed as before. */
  ActionContextClone clone_context;
  ActionContextRelease release_context;
} ActionNotice;

void action_notice_show(const ActionNotice *notice);
void action_notice_button(int button);
int action_notice_has_undo(void);
/* Drop the deferred context at shutdown or when its scope is invalidated. */
void action_notice_clear(void);
#endif
