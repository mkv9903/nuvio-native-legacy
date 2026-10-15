#include "profile_content.h"
#include "progress.h"
#include "catalogo.h"
#include "catalog_item.h"
#include "ajustes.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_NAME "perfilcont.txt"
#define SNAPSHOT_MAX ACCOUNT_PROFILE_MAX

typedef struct {
  int  profile, season, episode;
  char content_id[48];
  char title[160], poster[400], episode_name[96];
} ProfileSnapshot;

static ProfileSnapshot snapshots[SNAPSHOT_MAX];
static int num_snapshots, snapshots_loaded;
static CatItem catalog_scratch;   // static: CatItem is too large for stack

static void sanitize_string(char *s) {
  for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
}

static void load_snapshots(void) {
  char *buf, *ctx = NULL, *line;
  if (snapshots_loaded) return;
  snapshots_loaded = 1;
  num_snapshots = 0;
  buf = dados_ler(FILE_NAME);
  if (!buf) return;
  for (line = strtok_r(buf, "\n", &ctx); line && num_snapshots < SNAPSHOT_MAX;
       line = strtok_r(NULL, "\n", &ctx)) {
    ProfileSnapshot s;
    char *cols[7], *q = line;
    int k;
    memset(&s, 0, sizeof s);
    for (k = 0; k < 7; k++) {
      cols[k] = q;
      if (k < 6) { q = strchr(q, '\t'); if (!q) break; *q++ = 0; }
    }
    if (k < 6) continue;
    s.profile = atoi(cols[0]); s.season = atoi(cols[2]); s.episode = atoi(cols[3]);
    snprintf(s.content_id, sizeof s.content_id, "%s", cols[1]);
    snprintf(s.title, sizeof s.title, "%s", cols[4]);
    snprintf(s.poster, sizeof s.poster, "%s", cols[5]);
    snprintf(s.episode_name, sizeof s.episode_name, "%s", cols[6]);
    if (s.profile > 0 && s.content_id[0] && s.title[0]) snapshots[num_snapshots++] = s;
  }
  free(buf);
}

static void save_snapshots(void) {
  char *b = malloc((size_t)num_snapshots * 800 + 16);
  int i;
  size_t n = 0;
  if (!b) return;
  b[0] = 0;
  for (i = 0; i < num_snapshots; i++)
    n += (size_t)snprintf(b + n, 800, "%d\t%s\t%d\t%d\t%s\t%s\t%s\n", snapshots[i].profile,
                          snapshots[i].content_id, snapshots[i].season, snapshots[i].episode,
                          snapshots[i].title, snapshots[i].poster, snapshots[i].episode_name);
  dados_gravar(FILE_NAME, b);
  free(b);
}

static int find_snapshot(int profile) {
  int i;
  for (i = 0; i < num_snapshots; i++) if (snapshots[i].profile == profile) return i;
  return -1;
}

// Progress record -> catalog-known title, or 0.
static int from_catalog(const ProgressRecord *r) {
  catalog_item_clear(&catalog_scratch);
  return cat_copiar_por_id(r->content_id, r->episode > 0 ? "series" : "movie", &catalog_scratch) &&
         catalog_scratch.titulo[0];
}

int profile_content_from(const AccountProfile *p, ProfileContent *out) {
  ProgressRecord r;
  int i;
  if (!p || !out || p->has_pin || !p->lock_verified) return 0;
  if (!progress_continue_from_profile(p->index, ajustes_cw_concluido(), &r)) return 0;
  memset(out, 0, sizeof *out);
  if (from_catalog(&r)) {
    snprintf(out->title, sizeof out->title, "%s", catalog_scratch.titulo);
    snprintf(out->poster, sizeof out->poster, "%s", catalog_scratch.poster);
    if (catalog_scratch.temporada == r.season && catalog_scratch.episodio == r.episode)
      snprintf(out->episode_name, sizeof out->episode_name, "%s", catalog_scratch.nomeEpisodio);
  } else {
    catalog_item_clear(&catalog_scratch);
    load_snapshots();
    i = find_snapshot(p->index);
    if (i < 0 || strcmp(snapshots[i].content_id, r.content_id)) return 0;
    snprintf(out->title, sizeof out->title, "%s", snapshots[i].title);
    snprintf(out->poster, sizeof out->poster, "%s", snapshots[i].poster);
    if (snapshots[i].season == r.season && snapshots[i].episode == r.episode)
      snprintf(out->episode_name, sizeof out->episode_name, "%s", snapshots[i].episode_name);
  }
  catalog_item_clear(&catalog_scratch);
  out->is_series = r.episode > 0;
  out->season = r.season ? r.season : (out->is_series ? 1 : 0);
  out->episode = r.episode;
  out->progress = (float)(r.pos_sec / r.dur_sec);
  out->remaining_min = (int)((r.dur_sec - r.pos_sec) / 60.0 + 0.5);
  if (out->remaining_min < 1) out->remaining_min = 1;
  return 1;
}

void profile_content_register(void) {
  int i, m = perfis_n(), changed = 0;
  load_snapshots();
  for (i = 0; i < m; i++) {
    const AccountProfile *p = perfis_item(i);
    ProgressRecord r;
    ProfileSnapshot s;
    int k;
    if (!p || p->has_pin || !p->lock_verified || !progress_continue_from_profile(p->index, ajustes_cw_concluido(), &r) ||
        !from_catalog(&r) || strlen(catalog_scratch.poster) >= sizeof s.poster) continue;
    memset(&s, 0, sizeof s);
    s.profile = p->index; s.season = r.season; s.episode = r.episode;
    snprintf(s.content_id, sizeof s.content_id, "%s", r.content_id);
    snprintf(s.title, sizeof s.title, "%s", catalog_scratch.titulo);
    snprintf(s.poster, sizeof s.poster, "%s", catalog_scratch.poster);
    if (catalog_scratch.temporada == r.season && catalog_scratch.episodio == r.episode)
      snprintf(s.episode_name, sizeof s.episode_name, "%s", catalog_scratch.nomeEpisodio);
    sanitize_string(s.title); sanitize_string(s.poster); sanitize_string(s.episode_name);
    k = find_snapshot(s.profile);
    if (k >= 0 && !memcmp(&snapshots[k], &s, sizeof s)) continue;
    if (k < 0 && num_snapshots < SNAPSHOT_MAX) k = num_snapshots++;
    if (k < 0) continue;
    snapshots[k] = s;
    changed = 1;
  }
  catalog_item_clear(&catalog_scratch);
  if (changed) save_snapshots();
}

void profile_content_forget(void) {
  catalog_item_clear(&catalog_scratch);
  num_snapshots = 0;
  snapshots_loaded = 1;
  dados_apagar(FILE_NAME);
}

// Backward-compatibility shims
int  perfilcont_de(const ContaPerfil *p, PerfilCont *saida) {
  return profile_content_from(p, saida);
}
void perfilcont_registrar(void) {
  profile_content_register();
}
void perfilcont_esquecer(void) {
  profile_content_forget();
}
