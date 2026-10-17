/* Real catalog/model operations for Rust ownership regression tests. */
#include "catalog_item.h"
#include "action_notice.h"
#include "title_action_context.h"
#include "detail.h"
#include "player.h"
#include "ctxmenu.h"
#include "trakt.h"
#include "catalog_scene_adapter.h"
#include "catalog_row.h"
#include "home_focus.h"
#include "catalog_runtime.h"
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
CatItem *native_title_copy_catalog(int index) {
  CatItem *item = calloc(1, sizeof *item);
  if (item && !catalog_copy_index(index, item)) { free(item); item = NULL; }
  return item;
}
CatItem *native_metadata_copy(int index) {
  CatItem *title=calloc(1,sizeof *title);
  if (title && !catalog_metadata_copy(index,title)) { free(title); title=NULL; }
  return title;
}
int native_metadata_try_episode(void) {
  CatEp episode={0}; episode.temporada=1; episode.episodio=1;
  snprintf(episode.nome,sizeof episode.nome,"Delayed episode");
  cat_definir_episodios(0,&episode,1); return cat_n_episodios(0);
}
void native_catalog_project_reorder(void) {
  CatFileira owned[CAT_FIL_MAX]={0}, projected[CAT_FIL_MAX]={0};
  int count=catalog_copy_rows(owned,CAT_FIL_MAX),index;
  catalog_rows_project(projected,owned,(size_t)count,CAT_FIL_MAX);
  for (index=0;index<count/2;index++) { CatFileira row=projected[index]; projected[index]=projected[count-index-1]; projected[count-index-1]=row; }
  cat_republicar_fileiras(projected,count);
  catalog_rows_clear(owned,(size_t)count);
}
void native_title_destroy(CatItem *item) { catalog_item_clear(item); free(item); }
const char *native_title_text(const CatItem *item, unsigned field) { return catalog_title_text(item, field); }
const char *native_catalog_row_text(int index, unsigned field) { return catalog_row_text(cat_fileira(index),field); }
void native_title_focus_hold(const CatItem *title, const char *artwork) {
  HomeItem item={0}; item.arte=artwork; home_focus_set(&item,title);
}
int native_title_focus_available(void) { HomeItem item; return home_focus_get(&item); }
void native_title_overlay_poster(CatItem *item, const char *poster) {
  snprintf(item->poster, sizeof item->poster, "%s", poster);
}
void native_title_overlay_resume(CatItem *item, int percentage, int season, int episode, int remaining, int64_t watched) {
  item->progresso=percentage; item->temporada=season; item->episodio=episode;
  item->restanteMin=remaining; item->retomadoMs=watched;
}
void native_title_overlay_cast(CatItem *item, int mode) {
  if (mode == 0) { item->nElenco=0; item->nTemporadas=0; }
  if (mode == 1 && item->nElenco) {
    snprintf(item->elenco[0].papel,sizeof item->elenco[0].papel,"Updated role");
    if (item->nTemporadas) item->temporadas[0]=9;
  }
  if (mode == 2) { item->nElenco=CAT_ELENCO_MAX+1; }
  if (mode == 3 && item->nElenco) memset(item->elenco[0].nome,'x',sizeof item->elenco[0].nome);
  if (mode == 4) { item->nElenco=2; item->nTemporadas=3; }
}
const RustSceneSnapshot *native_title_capture(const CatItem *item, const RustRowSnapshot *source) {
  RustSceneCapture *capture=nuvio_scene_capture_create();
  CatFileira row={0};
  const RustRowInfo *info;
  if (!catalog_scene_capture_title(capture,item)) goto failed;
  if (source) {
#define FIELD(selector,member) nuvio_row_copy_text(source,selector,row.member,sizeof row.member)
    FIELD(ROW_KEY,chave); FIELD(ROW_NAME,titulo); FIELD(ROW_MEDIA_TYPE,tipo);
    FIELD(ROW_ADDON_URL,base); FIELD(ROW_CATALOG_ID,catId);
#undef FIELD
    info=nuvio_row_info(source);
    row.ini=(int)info->start; row.n=(int)(info->end-info->start); row.estado=info->state==ROW_CONFIRMED_EMPTY;
    if (!catalog_scene_capture_row(capture,&row,source)) goto failed;
  }
  return nuvio_scene_capture_finish(capture);
failed:
  nuvio_scene_capture_destroy(capture); return NULL;
}
const RustSceneSnapshot *native_title_capture_live_c_values(void) {
  CatItem item={0};
  CatFileira row={0};
  snprintf(item.imdb,sizeof item.imdb,"kitsu:41370"); snprintf(item.tipo,sizeof item.tipo,"series");
  snprintf(item.titulo,sizeof item.titulo,"Live title"); snprintf(item.poster,sizeof item.poster,"https://art.invalid/poster");
  snprintf(item.genero,sizeof item.genero,"TV Show · Drama"); snprintf(item.meta,sizeof item.meta,"2026 · 48 min");
  snprintf(item.classificacao,sizeof item.classificacao,"TV-14"); snprintf(item.sinopse,sizeof item.sinopse,"Live description");
  snprintf(item.origem,sizeof item.origem,"live-provider"); item.nota=85; item.tmdb=987;
  item.progresso=37; item.temporada=2; item.episodio=8; item.restanteMin=19; item.retomadoMs=123456789;
  item.naLista=1; item.nElenco=1; item.nTemporadas=2; item.temporadas[0]=1; item.temporadas[1]=2;
  snprintf(item.elenco[0].nome,sizeof item.elenco[0].nome,"Live actor");
  snprintf(item.provNome,sizeof item.provNome,"Live subscription");
  snprintf(row.chave,sizeof row.chave,"live-row"); snprintf(row.titulo,sizeof row.titulo,"Live catalog");
  snprintf(row.tipo,sizeof row.tipo,"series"); snprintf(row.base,sizeof row.base,"https://provider.invalid/config?private=keep");
  snprintf(row.catId,sizeof row.catId,"catalog"); row.n=1;
  return catalog_scene_capture(&item,1,&row,1);
}
int64_t native_title_projection_value(const CatItem *item, unsigned field) {
  switch (field) {
    case 0: return item->progresso; case 1: return item->temporada;
    case 2: return item->episodio; case 3: return item->restanteMin;
    case 4: return item->retomadoMs; case 5: return item->naLista;
    case 6: return item->naColecao; case 7: return item->nElenco;
    case 8: return item->nTemporadas;
    default: return -1;
  }
}
const char *native_title_cast_projection(const CatItem *item, size_t index, unsigned field) {
  if (!item || index >= (size_t)item->nElenco) return "";
  switch (field) {
    case 0: return item->elenco[index].nome;
    case 1: return item->elenco[index].papel;
    case 2: return item->elenco[index].foto;
    default: return "";
  }
}
int native_title_season_projection(const CatItem *item, size_t index) {
  return item && index < (size_t)item->nTemporadas ? item->temporadas[index] : -1;
}
void native_title_publish(const CatItem *item) { cat_definir_tudo(item, item ? 1 : 0, NULL, 0); }
void native_title_update(const CatItem *item) { cat_atualizar_item(0, item); }
const CatItem *native_title_current(void) { return cat_n() ? cat_item(0) : NULL; }
void native_title_frame(void) { cat_quadro(); }
int native_title_find(const char *id) { return cat_indice_por_imdb(id); }

extern int discovery_test_filter_owned(CatItem *items, int count);
/* The providers here have non-IMDb IDs, so the real enrichment path never
 * performs network I/O. The first unusable title must release its owner and
 * the second must move into its place without duplicating ownership. */
CatItem *native_title_compact_enrichment(const CatItem *rejected, const CatItem *kept) {
  CatItem *items = calloc(2, sizeof *items);
  int count;
  if (!items) return NULL;
  catalog_item_copy(&items[0], rejected);
  catalog_item_copy(&items[1], kept);
  items[0].poster[0] = 0;
  count = trakt_enfeitar_lote(items, 2);
  if (count != 1 || items[1].rust_title) {
    catalog_items_clear(items, 2); free(items); return NULL;
  }
  return items;
}
CatItem *native_title_filter_progress(const CatItem *finished, const CatItem *kept) {
  CatItem *items = calloc(2, sizeof *items);
  int count;
  if (!items) return NULL;
  catalog_item_copy(&items[0], finished);
  catalog_item_copy(&items[1], kept);
  items[0].progresso = 100; items[1].progresso = 40;
  count = discovery_test_filter_owned(items, 2);
  if (count != 1 || items[1].rust_title) {
    catalog_items_clear(items, 2); free(items); return NULL;
  }
  return items;
}

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

void native_search_test_seed(const CatItem *title) {
  CatFileira row = {0};
  snprintf(row.chave,sizeof row.chave,"search-fixture-movies");
  snprintf(row.titulo,sizeof row.titulo,"Movies");
  snprintf(row.tipo,sizeof row.tipo,"movie");
  row.n = title ? 1 : 0;
  cat_definir_tudo(title,title ? 1 : 0,title ? &row : NULL,title ? 1 : 0);
}
