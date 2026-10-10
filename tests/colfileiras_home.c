// #233: assemble the real Home after an account pull, without a renderer.
// Reuse the focused Home fixture's service doubles; no network is involved.
#define main cwordem_home_fixture_main
#include "cwordem_home.c"
#undef main
#include "../src/colfileiras.h"

Uint32 SDL_GetTicks(void) { return 1000; }

int addons_n(void) { return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return "account-home"; }

#define GRUPO(id,pasta) "{\"id\":\"" id "\",\"title\":\"Same title\",\"folders\":[{\"id\":\"" pasta "\",\"title\":\"Folder\",\"sources\":[{\"addonBaseUrl\":\"https://collection.invalid\",\"type\":\"movie\",\"catalogId\":\"" pasta "\"}]}]}"
static const char *duas = "{\"collections\":[" GRUPO("c1","f1") "," GRUPO("c2","f2") "]}";

int main(void) {
  fil_definir_limite(3);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");
  assert(colfileiras_receber(duas) == 2);
  colfileiras_sincronizar();
  catordem_ler("{\"items\":[{\"is_collection\":true,\"collection_id\":\"c2\",\"order\":0},{\"is_collection\":true,\"collection_id\":\"c1\",\"order\":1}]}");
  sincronizarFileiras();
  assert(nFileiras == 2 && posicao("collection_c2") < posicao("collection_c1"));
  assert(naHome("collection_c2")->n == 1 && naHome("collection_c1")->n == 1);
  assert(naHome("collection_c2")->folders[0] != naHome("collection_c1")->folders[0]);
  assert(fil_n_na_home() == nFileiras && fil_n_capacidade() == 0);

  // Changing just account visibility/order must invalidate the Home fast path.
  catordem_ler("{\"items\":[{\"is_collection\":true,\"collection_id\":\"c1\",\"enabled\":false}]}");
  sincronizarFileiras();
  assert(nFileiras == 1 && !naHome("collection_c1") && naHome("collection_c2"));
  assert(fil_estado_chave("collection_c1") == FIL_FORA && fil_n_na_home() == nFileiras);

  // A last deletion clears every collection.
  colfileiras_receber("{\"collections\":[]}"); colfileiras_sincronizar();
  sincronizarFileiras();
  assert(nFileiras == 0 && fil_n_na_home() == 0);
  for (int i = 0; i < fil_n(); i++) assert(strncmp(fil_chave(i), "collection_", 11));
  assert(!naHome("collection_c1") && !naHome("collection_c2"));
  catordem_esquecer();

  static CatItem items[5]; static CatFileira rows[5];
  const char *keys[] = { "continue_watching", "social_activity", "addon_movie_a", "addon_movie_b", "addon_movie_c" };
  for (int i = 0; i < 5; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%d", i);
    snprintf(items[i].titulo, sizeof items[i].titulo, "Title %d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(rows[i].chave, sizeof rows[i].chave, "%s", keys[i]);
    snprintf(rows[i].titulo, sizeof rows[i].titulo, "Row %d", i);
    if (i >= 2) {
      snprintf(rows[i].base, sizeof rows[i].base, "https://addon.invalid");
      snprintf(rows[i].catId, sizeof rows[i].catId, "%c", 'a' + i - 2);
      snprintf(rows[i].tipo, sizeof rows[i].tipo, "movie");
    }
    rows[i].ini = i; rows[i].n = 1;
  }
  cat_definir_tudo(items, 5, rows, 5);
  colfileiras_receber(duas); colfileiras_sincronizar(); sincronizarFileiras();
  assert(nFileiras == 6 && fil_n_capacidade() == 3 && fil_n_na_home() == 6);
  for (int i = 0; i < 5; i++) assert((naHome(keys[i]) != NULL) == (i != 1));
  assert(naHome("collection_c1") && naHome("collection_c2"));
  assert(fil_estado_chave("https://collection.invalid_movie_f2") == FIL_FORA);
  sincronizarFileiras(); sincronizarFileiras();
  unsigned filaRev = fil_revisao(), colecaoRev = col_revisao();
  for (int i = 0; i < 10; i++) sincronizarFileiras();
  assert(fil_revisao() == filaRev && col_revisao() == colecaoRev && nFileiras == 6);
  puts("ok #233: real Home/editor order, hidden collection, empty snapshot, equal titles and catalogue-only quota");
  return 0;
}
