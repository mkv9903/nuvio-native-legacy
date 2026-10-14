#include "action_notice.h"
#include "ilha.h"
#include "tex_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ActionUndo pending_undo;
static void *pending_context;
static ActionContextRelease pending_release;

int action_notice_has_undo(void) { return pending_undo != NULL; }

void action_notice_clear(void) {
  void *context = pending_context;
  ActionContextRelease release = pending_release;
  pending_context = NULL; pending_release = NULL; pending_undo = NULL;
  if (context) { if (release) release(context); else free(context); }
}

void action_notice_show(const ActionNotice *notice) {
  static IlhaModal modal;
  if (!notice || !notice->message || !notice->message[0]) return;
  action_notice_clear();
  if (notice->undo) {
    if (notice->context && (notice->clone_context || notice->context_size)) {
      if (notice->clone_context) pending_context = notice->clone_context(notice->context);
      else {
        pending_context = malloc(notice->context_size);
        if (pending_context) memcpy(pending_context, notice->context, notice->context_size);
      }
      if (pending_context) {
        pending_undo = notice->undo;
        pending_release = notice->release_context;
      }
    } else if (!notice->context) pending_undo = notice->undo;
  }
  memset(&modal, 0, sizeof modal);
  snprintf(modal.kicker, sizeof modal.kicker, "%s", "Done");
  snprintf(modal.titulo, sizeof modal.titulo, "%s",
           notice->title && notice->title[0] ? notice->title : notice->message);
  if (notice->title && notice->title[0]) snprintf(modal.linha, sizeof modal.linha, "%s", notice->message);
  if (notice->location) snprintf(modal.nota, sizeof modal.nota, "%s", notice->location);
  if (notice->artwork && notice->artwork[0] && !tex_falhou(notice->artwork)) {
    snprintf(modal.arte, sizeof modal.arte, "%s", notice->artwork);
    tex_obter_larg(notice->artwork, 480.0f); /* Warm before the thumbnail flight finishes. */
  } else if (notice->thumbnail && notice->thumbnail[0])
    snprintf(modal.arte, sizeof modal.arte, "%s", notice->thumbnail);
  else snprintf(modal.icone, sizeof modal.icone, "%s", notice->icon && notice->icon[0] ? notice->icon : "check");
  modal.tipo = notice->kind;
  if (pending_undo) {
    snprintf(modal.botao[modal.nBotoes], sizeof modal.botao[0], "%s", "Undo");
    snprintf(modal.botaoIcone[modal.nBotoes++], sizeof modal.botaoIcone[0], "aj_rotate-cw");
  }
  snprintf(modal.botao[modal.nBotoes++], sizeof modal.botao[0], "%s", "OK");
  ilha_acao(notice->icon, notice->message, notice->kind,
            notice->animate_thumbnail ? notice->thumbnail : NULL, NULL, &modal);
}

void action_notice_button(int button) {
  if (button == 1 && pending_undo) {
    ActionUndo undo = pending_undo;
    void *context = pending_context;
    ActionContextRelease release = pending_release;
    pending_undo = NULL; pending_context = NULL; pending_release = NULL;
    undo(context);
    if (context) { if (release) release(context); else free(context); }
    ilha_acao("aj_rotate-cw", "Undone", ILHA_INFO, NULL, NULL, NULL);
  }
}
