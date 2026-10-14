#include "save_notice.h"
#include "action_notice.h"
#include "title_action_context.h"
#include "sessao.h"
#include "ilha.h"
#include "ajustes.h"
#include "dados.h"
#include "descoberta.h"
#include "salvos.h"
#include "simkl.h"
#include "trakt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Existing C settings markers remain until their backend is rewritten. */
#define PROMPT_MARKER "salvar-perguntado.txt"
#define PREVIOUS_MARKER "salvos-intro.txt"
#define DESTINATION_KEY "salvar:onde"

extern int trakt_watchlist_tipo(const char *imdb, const char *type, int add);
static int prompt_open;
static Uint32 prompt_started;
static int options[3];
static int option_count;
static CatItem pending_title;
static int pending_add;

int save_notice_prompt_open(void) { return prompt_open; }
void save_notice_release(void) {
  catalog_item_clear(&pending_title);
  prompt_open = 0;
}
static int already_asked(void) {
  char *marker = dados_ler(PROMPT_MARKER);
  if (marker) { free(marker); return 1; }
  marker = dados_ler(PREVIOUS_MARKER);
  if (marker) { free(marker); return 1; }
  return salvos_n() > 0;
}
const char *save_notice_destination(void) {
  if (ajustes_salvos_no_trakt() && trakt_ativo()) return "Trakt watchlist";
  if (ajustes_salvos_no_simkl() && simkl_ativo()) return "Simkl Plan to Watch";
  return "Nuvio list";
}
static void write_list(const CatItem *title, int add) {
  const char *id = catalog_title_id(title);
  const char *tracking_id = catalog_title_imdb_id(title);
  salvos_definir(title, add);
  if (*tracking_id && ajustes_salvos_no_trakt() && trakt_ativo())
    trakt_watchlist_tipo(tracking_id, title->tipo, add);
  else if (*tracking_id && ajustes_salvos_no_simkl() && simkl_ativo())
    simkl_lista_tipo(tracking_id, title->tipo, add);
  cat_definir_na_lista_imdb(id, add);
  desc_remontar_fileiras();
}
/* Undo uses the same write path and retains the title until execution. */
static void undo_save(const void *value) {
  const TitleActionContext *context = value;
  write_list(&context->title, !context->intent);
}
void save_notice_show(const CatItem *title, int added) {
  char message[200];
  TitleActionContext context;
  ActionNotice notice = {0};
  if (!title) return;
  if (added) snprintf(message, sizeof message, "Saved to %s", save_notice_destination());
  else snprintf(message, sizeof message, "%s", "Removed from the list");
  title_action_context_init(&context, title, added);
  notice.icon = "aj_bookmark"; notice.message = message;
  notice.title = catalog_title_text(title, TITLE_NAME);
  notice.location = "";
  notice.thumbnail = catalog_title_text(title, TITLE_POSTER);
  notice.artwork = catalog_title_text(title, TITLE_BACKGROUND);
  notice.kind = added ? ILHA_ACENTO : ILHA_INFO;
  notice.animate_thumbnail = added;
  notice.undo = undo_save; notice.context = &context;
  notice.clone_context = title_action_context_clone;
  notice.release_context = title_action_context_release;
  action_notice_show(&notice);
  title_action_context_clear(&context);
}
void save_notice_execute(const CatItem *title, int add) {
  if (!title || !catalog_title_id(title)[0]) return;
  write_list(title, add);
  save_notice_show(title, add);
}
static void finish_prompt(int destination) {
  prompt_open = 0;
  if (destination == AJ_SALVOS_LOCAL || destination == AJ_SALVOS_TRAKT || destination == AJ_SALVOS_SIMKL)
    ajustes_definir_salvos_destino(destination);
  dados_gravar(PROMPT_MARKER, "1\n");
  printf("[save-notice] destination selected: %d\n", destination);
  save_notice_execute(&pending_title, pending_add);
  catalog_item_clear(&pending_title);
}
int save_notice_ask(const CatItem *title, int add) {
  static IlhaModal modal;
  IlhaAvisoEx notice;
  int index;
  if (!title || !catalog_title_id(title)[0] || !add || prompt_open) return prompt_open;
  if (already_asked()) return 0;
  /* Without a data directory the marker cannot be saved. */
  if (!dados_dir()[0]) return 0;
  option_count = 0;
  options[option_count++] = AJ_SALVOS_LOCAL;
  if (trakt_ativo()) options[option_count++] = AJ_SALVOS_TRAKT;
  if (simkl_ativo()) options[option_count++] = AJ_SALVOS_SIMKL;
  memset(&modal, 0, sizeof modal);
  snprintf(modal.kicker, sizeof modal.kicker, "%s", "First time with the list");
  snprintf(modal.titulo, sizeof modal.titulo, "%s", "Where does + save?");
  if (sessao_logada())
    snprintf(modal.texto, sizeof modal.texto, "%s", option_count > 1
             ? "The Nuvio list stays on this TV and in your account profile. Trakt and Simkl also keep it in their own accounts. You can change this in Settings."
             : "The title goes to the Nuvio list, on this TV and in your account profile.");
  else
    snprintf(modal.texto, sizeof modal.texto, "%s", option_count > 1
             ? "The Nuvio list stays on this TV only. Trakt and Simkl keep it in your account, and this TV's list is saved too. You can change it in Settings."
             : "The title stays in the Nuvio list, on this TV. Connect Trakt or Simkl in Settings to keep it in your account too.");
  snprintf(modal.icone, sizeof modal.icone, "%s", "aj_bookmark");
  modal.tipo = ILHA_ACENTO;
  modal.nBotoes = option_count > 1 ? option_count : 1;
  for (index = 0; index < option_count; index++) {
    const char *label = options[index] == AJ_SALVOS_LOCAL ? (option_count > 1 ? "Nuvio list" : "Save to the Nuvio list")
                      : options[index] == AJ_SALVOS_TRAKT ? "Trakt" : "Simkl";
    snprintf(modal.botao[index], sizeof modal.botao[index], "%s", label);
  }
  snprintf(modal.rodape, sizeof modal.rodape, "%s", "Back keeps the default.");
  memset(&notice, 0, sizeof notice);
  notice.chave = DESTINATION_KEY; notice.tipo = ILHA_ACENTO; notice.icone = "aj_bookmark";
  notice.texto = "Where does + save?"; notice.ms = 60000u; notice.modal = &modal; notice.abrir = 1;
  ilha_avisar_ex(&notice);
  catalog_item_replace(&pending_title, title);
  pending_add = add;
  prompt_open = 1;
  prompt_started = SDL_GetTicks();
  printf("[save-notice] first-save prompt opened (%d options)\n", option_count);
  return 1;
}
void save_notice_action(const char *key, int button) {
  if (!key || strcmp(key, DESTINATION_KEY) || !prompt_open) return;
  /* A single option confirms the existing default rather than changing it. */
  finish_prompt(option_count > 1 && button >= 1 && button <= option_count ? options[button - 1] : -1);
}
void save_notice_step(Uint32 now) {
  /* Back dismisses the modal without a button event; use the default. */
  if (prompt_open && now - prompt_started > 600u && !ilha_tem(DESTINATION_KEY) &&
      !ilha_modal_aberto() && !ilha_modal_visivel()) finish_prompt(-1);
}
