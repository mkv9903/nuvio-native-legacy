/* Real catalog/model operations for Rust ownership regression tests. */
#include "catalog_item.h"
#include "action_notice.h"
#include "title_action_context.h"
#include "detail.h"
#include "player.h"
#include "ctxmenu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

CatItem *native_title_create(const RustTitleSnapshot *snapshot) {
  CatItem *item = calloc(1, sizeof *item);
  if (!item) { nuvio_title_release(snapshot); return NULL; }
  if (item && !catalog_item_from_rust(item, snapshot)) { free(item); item = NULL; }
  return item;
}
CatItem *native_title_clone(const CatItem *source) {
  CatItem *item = calloc(1, sizeof *item);
  if (item) catalog_item_copy(item, source);
  return item;
}
void native_title_destroy(CatItem *item) { catalog_item_clear(item); free(item); }
const char *native_title_text(const CatItem *item, unsigned field) { return catalog_title_text(item, field); }
void native_title_overlay_poster(CatItem *item, const char *poster) {
  snprintf(item->poster, sizeof item->poster, "%s", poster);
}
void native_title_publish(const CatItem *item) { cat_definir_tudo(item, item ? 1 : 0, NULL, 0); }
void native_title_update(const CatItem *item) { cat_atualizar_item(0, item); }
const CatItem *native_title_current(void) { return cat_n() ? cat_item(0) : NULL; }
void native_title_frame(void) { cat_quadro(); }
int native_title_find(const char *id) { return cat_indice_por_imdb(id); }

extern void details_test_hold(const CatItem *title, int index);
extern int details_test_revalidate(void);
extern const CatItem *details_test_selection(void);
extern void player_test_hold_title(const CatItem *title);
extern const CatItem *player_test_selection(void);
void native_title_details_hold(const CatItem *title) { details_test_hold(title, 0); }
int native_title_details_revalidate(void) { return details_test_revalidate(); }
const CatItem *native_title_details_selection(void) { return details_test_selection(); }
void native_title_details_release(void) { details_release(); }
void native_title_player_hold(const CatItem *title) { player_test_hold_title(title); }
const CatItem *native_title_player_selection(void) { return player_test_selection(); }
void native_title_player_release(void) { player_encerrar(); }
void native_title_open_saved_menu(const CatItem *title) { context_menu_open_saved(title); }
const CatItem *native_title_menu_selection(void) { return ctx_titulo(); }
void native_title_menu_release(void) { context_menu_release(); }
void native_title_set_channel(const CatItem *title) { player_set_channel_title(title); }
const char *native_title_channel_id(void) { return player_channel_id(); }

static char *executed_id;
static int executed_intent, executions;
static void record_undo(const void *value) {
  const TitleActionContext *context = value;
  free(executed_id);
  executed_id = strdup(catalog_title_id(&context->title));
  executed_intent = context->intent;
  executions++;
}
static void *failed_context_clone(const void *value) { (void)value; return NULL; }
void native_title_queue_undo(const CatItem *title, int intent, int clone_failure) {
  TitleActionContext context;
  ActionNotice notice = {0};
  title_action_context_init(&context, title, intent);
  notice.message = "Saved to Nuvio list";
  notice.title = catalog_title_text(title, TITLE_NAME);
  notice.undo = record_undo;
  notice.context = &context;
  notice.clone_context = clone_failure ? failed_context_clone : title_action_context_clone;
  notice.release_context = title_action_context_release;
  action_notice_show(&notice);
  title_action_context_clear(&context);
}
const char *native_title_undo_id(void) { return executed_id ? executed_id : ""; }
int native_title_undo_intent(void) { return executed_intent; }
int native_title_undo_executions(void) { return executions; }
void native_title_reset_undo(void) {
  action_notice_clear();
  free(executed_id); executed_id = NULL; executed_intent = 0; executions = 0;
}
