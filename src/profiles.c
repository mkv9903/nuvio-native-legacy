void servidores_perfil_trocou(void);   // servidores.c: cancel in-flight work, load this profile
#include "profiles.h"
#include "session.h"
#include "nuvem.h"
#include "dados.h"
#include "fileiras.h"   /* fil_definir_perfil: row layout is per profile */

void fontepref_definir_perfil(int perfil);
void arteesc_definir_perfil(int perfil);
int ajustes_addons_do_principal(void);
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_ACTIVE "perfil.txt"
#define FILE_LIST   "perfis.txt"

static AccountProfile profile_list[ACCOUNT_PROFILE_MAX];
static int profile_count;
static char owner_id[64];
static int active_profile_index = 1;
static int chosen_this_session;  // 1 after user explicitly chose in this session
static int has_saved_profile;    // 1 when perfil.txt existed on startup

static void fetch_owner(void) {
  char *r;
  int st = 0;
  if (owner_id[0]) return;
  r = sessao_rpc("get_sync_owner", "{}", &st);
  if (r && st >= 200 && st < 300) {
    const char *a = strchr(r, '"');
    const char *b = a ? strchr(a + 1, '"') : NULL;
    if (a && b && b > a + 1 && (size_t)(b - a - 1) < sizeof owner_id) {
      memcpy(owner_id, a + 1, (size_t)(b - a - 1));
      owner_id[b - a - 1] = 0;
    }
  }
  free(r);
  if (!owner_id[0]) {
    snprintf(owner_id, sizeof owner_id, "%s", sessao_usuario());
    printf("[profiles] get_sync_owner did not respond; using token sub\n");
  }
}

// --- ON-DISK CACHE OF PROFILES LIST -----------------------------------------
static void clean_field(char *dst, size_t size, const char *src) {
  size_t i = 0;
  if (!size) return;
  for (; src && src[i] && i + 1 < size; i++)
    dst[i] = (src[i] == '\t' || src[i] == '\n' || src[i] == '\r') ? ' ' : src[i];
  dst[i] = 0;
}

static void save_cache(void) {
  char buf[8192];
  size_t p = 0;
  int i;
  if (profile_count <= 0) return;
  for (i = 0; i < profile_count && p < sizeof buf; i++) {
    char name[64], av[300], bg[300], color[10];
    clean_field(name, sizeof name, profile_list[i].name);
    clean_field(av, sizeof av, profile_list[i].avatar_url);
    clean_field(bg, sizeof bg, profile_list[i].background_url);
    clean_field(color, sizeof color, profile_list[i].color_hex);
    p += (size_t)snprintf(buf + p, sizeof buf - p, "%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
                          profile_list[i].index, profile_list[i].has_pin,
                          profile_list[i].is_primary, profile_list[i].uses_primary_addons,
                          color, name, av, bg);
  }
  if (p >= sizeof buf) return;
  dados_gravar(FILE_LIST, buf);
}

static void read_field(const char **p, char *dst, size_t size) {
  const char *s = *p;
  size_t i = 0;
  while (*s && *s != '\t' && *s != '\n') {
    if (i + 1 < size) dst[i++] = *s;
    s++;
  }
  if (size) dst[i] = 0;
  if (*s == '\t') s++;
  *p = s;
}

static void load_cache(void) {
  char *b = dados_ler(FILE_LIST);
  const char *p;
  int new_count = 0;
  if (!b) return;
  memset(profile_list, 0, sizeof profile_list);
  for (p = b; *p && new_count < ACCOUNT_PROFILE_MAX;) {
    char num[16];
    AccountProfile *d = &profile_list[new_count];
    read_field(&p, num, sizeof num); d->index = atoi(num);
    read_field(&p, num, sizeof num); d->has_pin = atoi(num);
    read_field(&p, num, sizeof num); d->is_primary = atoi(num);
    read_field(&p, num, sizeof num); d->uses_primary_addons = atoi(num);
    read_field(&p, d->color_hex, sizeof d->color_hex);
    read_field(&p, d->name, sizeof d->name);
    read_field(&p, d->avatar_url, sizeof d->avatar_url);
    read_field(&p, d->background_url, sizeof d->background_url);
    while (*p == '\n' || *p == '\r') p++;
    if (d->index > 0) new_count++;
    else memset(d, 0, sizeof *d);
  }
  free(b);
  if (new_count > 0) {
    profile_count = new_count;
    printf("[profiles] %d profile(s) from cache: selection screen can open now\n", profile_count);
  }
}

// --- OFFICIAL AVATAR CATALOG ------------------------------------------------
#define AVATAR_CAT_MAX 64
typedef struct { char id[64], path[128]; } AvatarCatalogItem;
static AvatarCatalogItem avatar_cat[AVATAR_CAT_MAX];
static int num_avatar_cat, avatar_cat_attempted;

static void load_avatar_catalog(void) {
  char *r;
  int st = 0;
  const char *p;
  if (avatar_cat_attempted) return;
  avatar_cat_attempted = 1;
  r = nuvem_rpc_com("get_avatar_catalog", "{}", NULL, &st);
  if (!r || st < 200 || st >= 300) {
    if (st) printf("[profiles] avatar catalog: HTTP %d\n", st);
    free(r);
    return;
  }
  for (p = js_raiz_array(r); p && num_avatar_cat < AVATAR_CAT_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    AvatarCatalogItem *a = &avatar_cat[num_avatar_cat];
    a->id[0] = a->path[0] = 0;
    js_texto(p, f, "id", a->id, sizeof a->id);
    js_texto(p, f, "storage_path", a->path, sizeof a->path);
    if (a->id[0] && a->path[0]) num_avatar_cat++;
  }
  free(r);
  printf("[profiles] %d official avatars in catalog\n", num_avatar_cat);
  fflush(stdout);
}

static int get_official_avatar(const char *id, char *dst, size_t size) {
  int i;
  if (!id || !id[0] || !dst || !size) return 0;
  load_avatar_catalog();
  for (i = 0; i < num_avatar_cat; i++) {
    if (strcmp(avatar_cat[i].id, id)) continue;
    snprintf(dst, size, "%s/storage/v1/object/public/avatars/%s",
             nuvem_url(), avatar_cat[i].path);
    return 1;
  }
  return 0;
}

int profiles_pull(void) {
  char *r;
  int st = 0;
  const char *p;

  fetch_owner();

  r = sessao_rpc("sync_pull_profiles", "{}", &st);
  if (!r || st < 200 || st >= 300) { free(r); return profile_count; }

  {
    int new_count = 0;
    AccountProfile tmp[ACCOUNT_PROFILE_MAX];
    memset(tmp, 0, sizeof tmp);
    for (p = js_raiz_array(r); p && new_count < ACCOUNT_PROFILE_MAX; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      double idx = js_num(p, f, "profile_index", 0);
      if (idx <= 0) idx = js_num(p, f, "id", 0);
      if (idx <= 0) continue;
      tmp[new_count].index = (int)idx;
      if (!js_texto(p, f, "name", tmp[new_count].name, sizeof tmp[new_count].name))
        snprintf(tmp[new_count].name, sizeof tmp[new_count].name, "Profile %d", (int)idx);
      js_texto(p, f, "avatar_url", tmp[new_count].avatar_url, sizeof tmp[new_count].avatar_url);
      if (!tmp[new_count].avatar_url[0]) {
        char aid[64] = "";
        if (js_texto(p, f, "avatar_id", aid, sizeof aid) && aid[0] &&
            !get_official_avatar(aid, tmp[new_count].avatar_url, sizeof tmp[new_count].avatar_url))
          printf("[profiles] avatar \"%s\" not in catalog\n", aid);
      }
      js_texto(p, f, "profile_background_url", tmp[new_count].background_url,
               sizeof tmp[new_count].background_url);
      if (!js_texto(p, f, "avatar_color_hex", tmp[new_count].color_hex, sizeof tmp[new_count].color_hex))
        snprintf(tmp[new_count].color_hex, sizeof tmp[new_count].color_hex, "#1E88E5");
      {
        char b[16];
        tmp[new_count].is_primary = js_bruto(p, f, "is_primary", b, sizeof b)
                                    ? (strcmp(b, "true") == 0) : ((int)idx == 1);
      }
      {
        char b[16];
        tmp[new_count].uses_primary_addons =
          js_bruto(p, f, "uses_primary_addons", b, sizeof b)
          ? (strcmp(b, "true") == 0)
          : (js_bruto(p, f, "uses_primary_plugins", b, sizeof b)
             ? (strcmp(b, "true") == 0) : 0);
      }
      new_count++;
    }
    if (new_count > 0) {
      memcpy(profile_list, tmp, sizeof profile_list);
      profile_count = new_count;
    }
  }
  free(r);

  // Profile locks
  r = sessao_rpc("sync_pull_profile_locks", "{}", &st);
  if (r && st >= 200 && st < 300) {
    for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      int idx = (int)js_num(p, f, "profile_id", 0);
      char b[16];
      int i, locked;
      if (!idx) idx = (int)js_num(p, f, "profile_index", 0);
      locked = js_bruto(p, f, "pin_enabled", b, sizeof b)
               ? (strcmp(b, "true") == 0) : 0;
      for (i = 0; i < profile_count; i++)
        if (profile_list[i].index == idx) profile_list[i].has_pin = locked;
    }
  }
  free(r);

  save_cache();
  printf("[profiles] %d profile(s), owner=%s, active=%d\n", profile_count, owner_id, active_profile_index);
  return profile_count;
}

int                  profiles_count(void)      { return profile_count; }
const AccountProfile *profiles_get(int i)       { return (i >= 0 && i < profile_count) ? &profile_list[i] : NULL; }

const AccountProfile *profiles_get_active(void) {
  int i;
  for (i = 0; i < profile_count; i++)
    if (profile_list[i].index == active_profile_index) return &profile_list[i];
  return NULL;
}

const char *profiles_owner(void)  { return owner_id; }
int         profiles_active(void) { return active_profile_index > 0 ? active_profile_index : 1; }

int profiles_active_addons(void) {
  const AccountProfile *p = profiles_get_active();
  int a = profiles_active();
  if (a == 1) return 1;
  return ((p && p->uses_primary_addons) || ajustes_addons_do_principal()) ? 1 : a;
}

void profiles_load_active(void) {
  char *b = dados_ler(FILE_ACTIVE);
  if (b) {
    int v = atoi(b);
    if (v > 0) { active_profile_index = v; has_saved_profile = 1; }
    free(b);
  }
  fil_definir_perfil(active_profile_index);
  fontepref_definir_perfil(active_profile_index);
  arteesc_definir_perfil(active_profile_index);
  load_cache();
}

void plugins_perfil_mudou(void);

void profiles_set_active(int index) {
  char line[32];
  int prev = active_profile_index;
  if (index <= 0) return;
  active_profile_index = index;
  fil_definir_perfil(index);
  fontepref_definir_perfil(index);
  arteesc_definir_perfil(index);
  plugins_perfil_mudou();
  if (index != prev) servidores_perfil_trocou();
  chosen_this_session = 1;
  has_saved_profile = 1;
  snprintf(line, sizeof line, "%d\n", index);
  dados_gravar(FILE_ACTIVE, line);
  printf("[profiles] active profile: %d\n", index);
}

void profiles_keep_active(void) { chosen_this_session = 1; }

int profiles_without_choice(void) {
  if (profile_count <= 0) return 1;
  if (profile_count == 1) return !profile_list[0].has_pin;
  return 0;
}

int profiles_needs_selection(void) {
  return !chosen_this_session && !profiles_without_choice();
}

int profiles_can_dismiss(void) {
  const AccountProfile *p;
  if (chosen_this_session) return 1;
  if (!has_saved_profile) return 0;
  p = profiles_get_active();
  return p && !p->has_pin;
}

int profiles_suggested_index(void) {
  int i;
  for (i = 0; i < profile_count; i++)
    if (profile_list[i].index == active_profile_index) return i;
  return 0;
}

ProfileAction profiles_action(int i) {
  if (i < 0 || i >= profile_count) return PROFILE_ACTION_NONE;
  return profile_list[i].has_pin ? PROFILE_ACTION_PIN : PROFILE_ACTION_ENTER;
}

int profiles_verify_pin(int index, const char *pin) {
  Jsw w;
  char *r;
  int st = 0, ok = 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", index);
  jsw_cs(&w, "p_pin", pin ? pin : "");
  jsw_obj_fim(&w);
  r = sessao_rpc("verify_profile_pin", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!r || st < 200 || st >= 300) { free(r); return -1; }
  ok = (strstr(r, "true") != NULL);
  free(r);
  return ok;
}

void profiles_forget(void) {
  memset(profile_list, 0, sizeof profile_list);
  profile_count = 0;
  owner_id[0] = 0;
  active_profile_index = 1;
  chosen_this_session = 0;
  has_saved_profile = 0;
  dados_apagar(FILE_ACTIVE);
  dados_apagar(FILE_LIST);
  printf("[profiles] profiles cleared (logged out)\n");
}

// Backward-compatibility shims
int perfis_puxar(void) { return profiles_pull(); }
int perfis_n(void) { return profiles_count(); }
const ContaPerfil *perfis_item(int i) { return profiles_get(i); }
const ContaPerfil *perfis_item_ativo(void) { return profiles_get_active(); }
const char *perfis_dono(void) { return profiles_owner(); }
int perfis_ativo(void) { return profiles_active(); }
int perfis_ativo_addons(void) { return profiles_active_addons(); }
void perfis_definir_ativo(int indice) { profiles_set_active(indice); }
void perfis_carregar_ativo(void) { profiles_load_active(); }
int perfis_precisa_escolher(void) { return profiles_needs_selection(); }
int perfis_sem_escolha(void) { return profiles_without_choice(); }
int perfis_pode_dispensar(void) { return profiles_can_dismiss(); }
void perfis_manter_ativo(void) { profiles_keep_active(); }
int perfis_indice_sugerido(void) { return profiles_suggested_index(); }
PerfilAcao perfis_acao(int i) { return profiles_action(i); }
int perfis_verificar_pin(int indice, const char *pin) { return profiles_verify_pin(indice, pin); }
void perfis_esquecer(void) { profiles_forget(); }
