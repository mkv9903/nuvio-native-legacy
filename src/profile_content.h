// "What this user was watching", under the focused profile in profile selection.
// Selects and describes the item; drawing is done in perfilsel.c / profile_selector.c.
//
// THE ITEM is the most recent unfinished local record of that profile
// (progress.c, progress_continue_from_profile), by Continue Watching limits.
// Only reads disk: no network to pick or describe.
//
// TITLE AND POSTER. In-memory catalog is that of the ACTIVE profile, so another
// person's item is almost never there. Thus, whenever the catalog knows a profile's
// title, a minimal copy (title, poster, episode name) goes to perfilcont.txt;
// profile selection uses that snapshot when the catalog lacks the title.
// Without both, no card appears.
//
// PROFILES WITH PIN SHOW NOTHING. What someone watched is private profile content.
#ifndef NV_PROFILE_CONTENT_H
#define NV_PROFILE_CONTENT_H

#include "profiles.h"

typedef struct {
  union { char title[160]; char titulo[160]; };
  union { char poster[400]; };
  union { char episode_name[96]; char epNome[96]; };  // series only, when episode name is known
  union { int is_series; int serie; };
  union { int season; int t; };
  union { int episode; int e; };
  union { float progress; float progresso; };         // 0..1
  union { int remaining_min; int restanteMin; };
} ProfileContent;

typedef ProfileContent PerfilCont;

// 1 and fills `out` when there is a card for this profile.
int  profile_content_from(const AccountProfile *p, ProfileContent *out);
// Saves minimal title snapshot of each profile known to current catalog.
void profile_content_register(void);
void profile_content_forget(void);

// Backward-compatibility shims
int  perfilcont_de(const ContaPerfil *p, PerfilCont *saida);
void perfilcont_registrar(void);
void perfilcont_esquecer(void);

#endif
