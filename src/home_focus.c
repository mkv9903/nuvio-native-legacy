/* Retained transition data; the Home layout and remote navigation are unchanged. */
#include "home_focus.h"
#include "catalog_item.h"
#include <stdlib.h>
#include <string.h>
static CatItem selected_title;
static HomeItem selected;
static char *artwork;
static int available;
void home_focus_set(const HomeItem *item, const CatItem *title) {
  char *copy;
  if (!item || !title) { home_focus_forget(); return; }
  if (!artwork || !item->arte || strcmp(artwork,item->arte)) {
    copy=item->arte ? strdup(item->arte) : NULL;
    free(artwork); artwork=copy;
  }
  catalog_item_replace(&selected_title,title);
  selected=*item;
  selected.arte=artwork ? artwork : catalog_title_text(&selected_title,TITLE_BACKGROUND);
  selected.titulo=catalog_title_text(&selected_title,TITLE_NAME);
  selected.genero=catalog_title_text(&selected_title,TITLE_GENRE_TEXT);
  selected.meta=catalog_title_text(&selected_title,TITLE_METADATA_TEXT);
  available=1;
}
int home_focus_get(HomeItem *output) {
  int index;
  if (!available || !output) return 0;
  index=cat_indice_vivo(selected.indice,catalog_title_id(&selected_title));
  if (index<0) return 0;
  *output=selected; output->indice=index; return 1;
}
void home_focus_forget(void) {
  catalog_item_clear(&selected_title); free(artwork); artwork=NULL;
  memset(&selected,0,sizeof selected); available=0;
}
#ifdef NV_NATIVE_UI_TEST
const CatItem *home_focus_test_title(void) { return available ? &selected_title : NULL; }
const char *home_focus_test_art(void) { return artwork ? artwork : ""; }
#endif
