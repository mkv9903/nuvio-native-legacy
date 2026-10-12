#include "profile_picker.h"
#include "rust_account.h"
#include "login.h"
#include "idioma.h"
#include "profiles.h"
#include "sync.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "session.h"
#include "catalogo.h"
#include "iconeapp.h"
#include "cachearte.h"
#include "ponteiro.h"
#include "plrui.h"
#include "profile_content.h"
#include "psparede.h"
#include "psestilos.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Layout: one horizontal row leaves Up/Down available for the PIN pad. Avatar diameter scales with profile count, between 168 and 288 pixels.
#define PS_AV_MAX      288.0f
#define PS_AV_MIN      168.0f
#define PS_GAP_RATIO     0.34f   // Gap as a fraction of avatar diameter.
#define PS_GAP_MIN       28.0f
// Expand the gap when space remains, up to 0.62 of the diameter, keeping focused halos separate.
#define PS_GAP_RATIO_MAX 0.62f
#define PS_TITLE_Y     148.0f
#define PS_SUB_Y        258.0f
#define PS_ROW_Y       396.0f   // Top of an unfocused avatar.
#define PS_NAME_GAP      36.0f   // Avatar base to name.
#define PS_BADGE_GAP      12.0f   // Name base to PIN badge.
#define PS_HINT_Y       948.0f
#define PS_PILL_H        68.0f
#define PS_PILL_PAD      30.0f
#define PS_ART_H       648.0f   // Height of the focused profile artwork. The Continue Watching card uses the established player material, poster and progress track.
#define PS_CONT_W      860.0f
#define PS_CONT_H      200.0f
#define PS_CONT_RADIUS    36.0f
#define PS_CONT_POSTER_W 108.0f
#define PS_CONT_POSTER_H 156.0f
#define PS_CONT_Y_MAX  800.0f   // Maximum card top; it moves upward with smaller avatars.

// Poster wall: reuse Home poster URLs and texture-cache width buckets. Empty catalogs show the background immediately; posters fade in when ready.
#define PS_WALL_MAX       9
#define PS_WALL_CARD_W_A 216.0f
#define PS_WALL_CARD_H_A 324.0f
#define PS_WALL_CARD_W_B 240.0f
#define PS_WALL_CARD_H_B 360.0f
#define PS_WALL_GAP      40.0f
#define PS_PART_MAX       32
#define PS_BURST_MAX      24
#define PS_WALL_RADIUS    0.080f

typedef struct {
  char url[512];
  float phase;
} PosterWallItem;

typedef struct {
  float x, y, speed, size, phase;
  float orbit, angular, depth;
  int color;
} Particle;

typedef struct {
  float x, y, vx, vy, size, color;
} PSBurst;

static PosterWallItem wall_items[PS_WALL_MAX];
static Particle wall_particles[PS_PART_MAX];
static PSBurst wall_burst[PS_BURST_MAX];
static GLuint wall_textures[PS_WALL_MAX];
static float wall_fade[PS_WALL_MAX];
static int wall_count;
// Build Continue Watching cards on entry or profile-list changes, never every frame.
typedef struct {
  int available;
  ProfileContent c;
  char meta[128];      // Season, episode and episode name.
  char remaining[64];   // Remaining time text.
  GLuint tex;          // Requested during update only.
} ContinueCard;
static ContinueCard continue_cards[ACCOUNT_PROFILE_MAX];
static int continue_enabled = 1;   // Last Continue Watching setting.
static int continue_count = -1;
static int particle_count;
static int burst_count;
#ifdef NV_PROFILE_PICKER_TEST
static int burst_drawn;
#endif
static unsigned wall_revision;
static float wall_time;
static float burst_time;
static float wall_card_width, wall_card_height;
static float wall_pressure;
static int wall_auto_compact;
static float wall_light[6]; /* Current focused-profile and adjacent-profile RGB colors. */
static float wall_light_target[6];
static float wall_previous_light_target[6];
static float wall_light_start[6];
static float wall_light_time;

// PIN pad: phone order, three columns and four rows. The fourth digit verifies automatically; the bottom row is blank, zero, delete (#289).
#define PS_PIN_MAX       8
#define PS_PIN_LEN       4
#define PS_KEY        96.0f
#define PS_KEY_GAP    18.0f
#define PS_KEY_COLUMNS       3
#define PS_KEY_ROWS       4
#define PS_PIN_BLANK        9   // Blank key never receives focus.
#define PS_PIN_ZERO        10
#define PS_PIN_DELETE      11
#define PS_PIN_START       4   // Start on the middle key, five.
#define PS_DOT        22.0f    // Diameter of each masked PIN dot.
#define PS_DOT_STEP  40.0f

static int focus;
static int complete, wants_logout, wants_retry;
// Keep the chosen card and its loading indicator visible while the app prepares that profile's Home.
static int preparing;
static Uint32 preparation_started;
static float focus_animation[ACCOUNT_PROFILE_MAX];
static float entrance_animation;        // Entrance animation, zero to one.
static float pin_animation;            // PIN veil and keyboard animation, zero to one.

// Negative means no profile is requesting a PIN.
static int pin_slot = -1;
static char pin[PS_PIN_MAX + 1];
static int pin_focus;              // Slot in the phone keypad.
static int wrong_pin, pin_network_error;
static float pin_shake;          // Seconds remaining in the wrong-PIN shake.
static int verifying;
static uint64_t selection_action;

static unsigned wall_random(unsigned *state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

// Use one poster request width based on the largest 1.22 scale. Drawing reuses the texture and never decodes it.
static float wall_request_width(void) { return wall_card_width * 1.22f; }

static void configure_wall(void) {
  const char *v = getenv("NUVIO_PROFILE_PICKER_VARIANT");
  const char *e = getenv("NUVIO_PROFILE_PICKER_EFFECTS");
  int variant_b = v && (!strcmp(v, "B") || !strcmp(v, "b") || !strcmp(v, "240"));
  int compact = e && (!strcmp(e, "compact") || !strcmp(e, "20"));
  wall_card_width = variant_b ? PS_WALL_CARD_W_B : PS_WALL_CARD_W_A;
  wall_card_height = variant_b ? PS_WALL_CARD_H_B : PS_WALL_CARD_H_A;
  particle_count = compact ? 20 : PS_PART_MAX;
  burst_count = compact ? 18 : PS_BURST_MAX;
  wall_auto_compact = compact;
}

static int wall_contains_url(const char *url) {
  int i;
  for (i = 0; i < wall_count; i++)
    if (!strcmp(wall_items[i].url, url)) return 1;
  return 0;
}

static int parse_color(const char *hex, float *r, float *g, float *b);
static void readable_color(float *r, float *g, float *b);

static void rebuild_wall(void) {
  unsigned rev = cat_revisao();
  PosterWallItem previous_items[PS_WALL_MAX];
  float fades[PS_WALL_MAX];
  int previous_count = wall_count, total, i, same_set;
  if (wall_revision == rev) return;
  memcpy(previous_items, wall_items, sizeof previous_items);
  memcpy(fades, wall_fade, sizeof fades);
  wall_revision = rev;
  wall_count = 0;
  memset(wall_textures, 0, sizeof wall_textures);
  memset(wall_fade, 0, sizeof wall_fade);
  total = cat_n();
  for (i = 0; i < total && wall_count < PS_WALL_MAX; i++) {
    const CatItem *item = cat_item(i);
    if (!item || !item->poster[0] || wall_contains_url(item->poster)) continue;
    snprintf(wall_items[wall_count].url, sizeof wall_items[wall_count].url, "%s",
             item->poster);
    wall_items[wall_count].phase = (float)wall_count * 0.83f;
    { int j;
      for (j = 0; j < previous_count; j++) {
        if (!strcmp(previous_items[j].url, wall_items[wall_count].url)) {
          wall_items[wall_count].phase = previous_items[j].phase;
          wall_fade[wall_count] = fades[j];
          break;
        }
      }
    }
    wall_count++;
  }
  same_set = wall_count == previous_count;
  if (same_set) {
    for (i = 0; i < wall_count; i++) {
      int j, found = 0;
      for (j = 0; j < previous_count; j++)
        if (!strcmp(wall_items[i].url, previous_items[j].url)) { found = 1; break; }
      if (!found) { same_set = 0; break; }
    }
  }
  if (!same_set) {
    cachearte_limpar_referencias_grupo(NV_CACHE_ARTE_GRUPO_PERFIL);
    for (i = 0; i < wall_count; i++) {
      // Retain the requested scale and quality variant; the wall belongs to the essential texture set.
      tex_cache_marcar_larg(NV_CACHE_ARTE_GRUPO_PERFIL, wall_items[i].url,
                           wall_request_width(), 1, 0);
    }
  }
}

static void update_wall_light_targets(void) {
  int m = profiles_count();
  int idx = focus >= 0 && focus < m ? focus : 0;
  int neighbor = m > 1 ? (idx + 1) % m : idx;
  const AccountProfile *p = m > 0 ? profiles_get(idx) : NULL;
  const AccountProfile *q = m > 0 ? profiles_get(neighbor) : NULL;
  float cr = 0.43f, cg = 0.18f, cb = 0.92f;
  float vr = cr, vg = cg, vb = cb;
  if (p && parse_color(p->color_hex, &cr, &cg, &cb)) readable_color(&cr, &cg, &cb);
  if (q && parse_color(q->color_hex, &vr, &vg, &vb)) readable_color(&vr, &vg, &vb);
  wall_light_target[0] = cr; wall_light_target[1] = cg; wall_light_target[2] = cb;
  wall_light_target[3] = vr; wall_light_target[4] = vg; wall_light_target[5] = vb;
}

static void init_wall_particles(void) {
  unsigned state = 0x1c7a3d29u;
  int i;
  for (i = 0; i < particle_count; i++) {
    unsigned r = wall_random(&state);
    wall_particles[i].x = (float)(r % 1920u);
    wall_particles[i].y = 96.0f + (float)((r >> 8) % 846u);
    wall_particles[i].depth = 0.34f + (float)((r >> 16) % 67u) * 0.01f;
    wall_particles[i].speed = 9.0f + wall_particles[i].depth * 25.0f;
    wall_particles[i].size = 2.2f + wall_particles[i].depth * 4.8f;
    wall_particles[i].phase = (float)(r % 1000u) * 0.001f;
    wall_particles[i].orbit = 18.0f + (float)((r >> 20) % 72u);
    wall_particles[i].angular = 0.18f + wall_particles[i].depth * 0.24f;
    wall_particles[i].color = (int)((r >> 27) % 4u);
  }
}

static void init_wall_burst(void) {
  unsigned state = 0x7d4a12c3u;
  int i;
  for (i = 0; i < burst_count; i++) {
    unsigned r = wall_random(&state);
    float angle = (float)i * 6.2831853f / (float)burst_count
                 + (float)(r % 100u) * 0.0062831853f;
    float speed = 150.0f + (float)((r >> 8) % 190u);
    wall_burst[i].x = NV_TELA_W * 0.5f + cosf(angle) * 8.0f;
    wall_burst[i].y = 332.0f + sinf(angle) * 8.0f;
    wall_burst[i].vx = cosf(angle) * speed;
    wall_burst[i].vy = sinf(angle) * speed * 0.68f;
    wall_burst[i].size = 3.0f + (float)((r >> 20) % 4u);
    wall_burst[i].color = (float)((r >> 26) % 4u);
  }
}

static void particle_color(int color, float *r, float *g, float *b) {
  static const float palette[][3] = {
    { 0.43f, 0.18f, 0.92f }, // Deep violet.
    { 0.68f, 0.30f, 1.00f }, // Amethyst.
    { 0.86f, 0.43f, 1.00f }, // Lilac.
    { 0.31f, 0.12f, 0.66f }  // Indigo.
  };
  *r = palette[color & 3][0];
  *g = palette[color & 3][1];
  *b = palette[color & 3][2];
}

static void draw_wall_particles(float alpha, int reduced_motion) {
  int i;
  for (i = 0; i < particle_count; i++) {
    const Particle *p = &wall_particles[i];
    float t = reduced_motion ? 0.0f : wall_time;
    float angle = p->phase * 6.2831853f + t * p->angular;
    float cycle = NV_TELA_W + p->orbit * 2.0f;
    float drift = fmodf(p->speed * t, cycle);
    float x = p->x + drift - p->orbit;
    float y;
    float field;
    float r, g, b;
    x += cosf(angle) * p->orbit;
    y = p->y + sinf(angle * 0.91f) * p->orbit * 0.72f
            + cosf(t * 0.24f + p->phase * 6.2831853f) * 18.0f;
    while (x < -p->orbit) x += cycle;
    while (x > NV_TELA_W + p->orbit) x -= cycle;
    // Keep particles away from the title, subtitle and avatars so the text stays prominent.
    field = 1.0f;
    if (fabsf(x - NV_TELA_W * 0.5f) < 500.0f && y > 112.0f && y < 714.0f)
      field = 0.30f;
    particle_color(p->color, &r, &g, &b);
    // Orbital trails stay vector-based, without blur or additional render targets.
    {
      float trail = 22.0f + p->depth * 42.0f;
      float start = x - trail;
      float curve = y + sinf(angle - 0.55f) * p->orbit * 0.18f;
      while (start < -trail) start += cycle;
      gfx_cor((GfxRect){ start, curve, trail, 1.2f + p->depth * 1.8f },
              0.28f, r, g, b, alpha * field * (0.12f + p->depth * 0.10f));
    }
    // A broad translucent halo adds depth around the small sharp particle core.
    gfx_cor((GfxRect){ x - p->size * 0.85f, y - p->size * 0.85f,
                       p->size * 1.7f, p->size * 1.7f },
            0.46f, r, g, b, alpha * field * (0.06f + p->depth * 0.08f));
    gfx_cor((GfxRect){ x, y, p->size, p->size * 0.68f },
            0.38f, r, g, b, alpha * field * (0.42f + p->depth * 0.30f));
  }
}

static void draw_wall_burst(float alpha, int reduced_motion) {
  float t = reduced_motion ? 0.0f : burst_time;
  int i;
  if (!reduced_motion && t > 2.6f) return;
  if (t > 2.6f) t = 2.6f;
  for (i = 0; i < burst_count; i++) {
    const PSBurst *p = &wall_burst[i];
    float x = p->x + p->vx * t;
    float y = p->y + p->vy * t + 42.0f * t * t;
    float r, g, b;
    float fade = reduced_motion ? 0.58f : 1.0f - t / 2.6f;
    particle_color((int)p->color, &r, &g, &b);
    if (x < -24.0f || x > NV_TELA_W + 24.0f || y < -24.0f || y > NV_TELA_H + 24.0f)
      continue;
    // A short vector burst fades after 2.6 seconds, then the orbital field takes over.
    gfx_cor((GfxRect){ x - p->size * 0.95f, y - p->size * 0.95f,
                       p->size * 1.9f, p->size * 1.9f },
            0.46f, r, g, b, alpha * fade * 0.13f);
    gfx_cor((GfxRect){ x, y, p->size, p->size * 0.74f },
            0.38f, r, g, b, alpha * fade * 0.84f);
  }
}

// Scale and speed create depth: nearby bands are larger, brighter and faster than distant ones.
static void draw_wall_band(float alpha, int reduced_motion, float y, int direction,
                               float offset, float scale, float velocity) {
  float cw = wall_card_width * scale, ch = wall_card_height * scale;
  float stride = cw + PS_WALL_GAP * scale;
  float cycle = stride * (float)wall_count;
  float displacement = fmodf((reduced_motion ? 0.0f : wall_time * velocity) + offset, cycle);
  int i;
  // Repeat the same covers in opposite directions; the fixed bound covers 1920 pixels without allocating another list.
  for (i = -2; i < 16; i++) {
    int pos = i % wall_count;
    float x, yy;
    GLuint tex;
    GfxRect card;
    if (pos < 0) pos += wall_count;
    x = direction > 0 ? (float)i * stride - displacement
                    : NV_TELA_W - (float)(i + 1) * stride + displacement;
    if (x > NV_TELA_W + cw || x < -cw) continue;
    yy = y + sinf(wall_time * 0.45f + wall_items[pos].phase) * 3.0f * scale;
    tex = wall_textures[pos];
    if (!tex || wall_fade[pos] <= 0.001f) continue;
    card = (GfxRect){ x, yy, cw, ch };
    gfx_tex_aspect_atual = tex_aspecto(wall_items[pos].url);
    // The card SDF clips rounded corners. The band's alpha controls cover brightness.
    gfx_rect(card, tex, GFX_CARD, 0, 0, 0, PS_WALL_RADIUS, 1, 1, 1,
             alpha * wall_fade[pos]);
    gfx_tex_aspect_atual = 0;
  }
}

static void draw_wall_covers(float alpha, int reduced_motion) {
  if (wall_count <= 0) return;
  // Draw three depth planes with different offsets, scales, opacity and speed, using the same poster set.
  draw_wall_band(alpha * 0.55f, reduced_motion,  64.0f,  1,   0.0f, 0.78f, 13.0f);
  // Dim the middle band behind the avatars to preserve profile and name contrast.
  draw_wall_band(alpha * 0.30f, reduced_motion,
                     64.0f + wall_card_height * 0.78f + 74.0f,
                     -1, 118.0f, 1.00f, 20.0f);
  draw_wall_band(alpha * 0.92f, reduced_motion,
                     NV_TELA_H - wall_card_height * 1.22f - 15.0f,
                     1, 244.0f, 1.22f, 31.0f);
}

// Profile-colored diffuse lights tint the wall; top and bottom gradients fade covers to black at the screen edges.
static void draw_wall_light(float alpha) {
  float cr = wall_light[0], cg = wall_light[1], cb = wall_light[2];
  float vr = wall_light[3], vg = wall_light[4], vb = wall_light[5];
  { GfxRect a = { -380.0f, 120.0f, 1500.0f, 1500.0f };
    GfxRect b = {  800.0f, -520.0f, 1500.0f, 1500.0f };
    gfx_rect(a, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, cr, cg, cb, 0.55f * alpha);
    gfx_rect(b, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, vr, vg, vb, 0.38f * alpha); }
  { GfxRect top = { 0, 0, NV_TELA_W, 250.0f };
    GfxRect base = { 0, NV_TELA_H - 230.0f, NV_TELA_W, 230.0f };
    gfx_rect(top, 0, GFX_VEU_TOPO,  0, 0, 0, 0, 0, 0, 0, 0.90f * alpha);
    gfx_rect(base, 0, GFX_VEU_BAIXO, 0, 0, 0, 0, 0, 0, 0, 0.90f * alpha); }
  // Keep the center free of an extra dark overlay; title shadow and avatar fill provide contrast.
}

static void draw_wall(float alpha, int reduced_motion) {
#ifdef NV_PROFILE_PICKER_TEST
  burst_drawn = 0;
#endif
  draw_wall_covers(alpha, reduced_motion);
  draw_wall_light(alpha);
  // Particles appear above the poster wall and below text and avatars.
  draw_wall_particles(alpha, reduced_motion);
  if (!reduced_motion) {
    draw_wall_burst(alpha, 0);
#ifdef NV_PROFILE_PICKER_TEST
    burst_drawn = 1;
#endif
  }
}

static int parse_color(const char *hex, float *r, float *g, float *b) {
  unsigned v = 0;
  if (!hex || hex[0] != '#' || strlen(hex) < 7) return 0;
  if (sscanf(hex + 1, "%6x", &v) != 1) return 0;
  *r = ((v >> 16) & 255) / 255.0f;
  *g = ((v >> 8) & 255) / 255.0f;
  *b = (v & 255) / 255.0f;
  return 1;
}

// Enforce a luminance floor for dark account avatar colors while preserving hue.
static void readable_color(float *r, float *g, float *b) {
  float lum = 0.2126f * *r + 0.7152f * *g + 0.0722f * *b;
  if (lum >= 0.16f) return;
  { float k = lum > 0.001f ? 0.16f / lum : 0.0f;
    if (k > 6.0f) k = 6.0f;
    *r = anim_clamp(*r * k + 0.10f, 0.0f, 1.0f);
    *g = anim_clamp(*g * k + 0.10f, 0.0f, 1.0f);
    *b = anim_clamp(*b * k + 0.10f, 0.0f, 1.0f); }
}

// Copy the first Unicode character, rather than the first UTF-8 byte.
static void first_character(const char *name, char *dst, size_t capacity) {
  size_t z = 1;
  if (capacity < 5) { if (capacity) dst[0] = 0; return; }
  if (!name || !name[0]) { dst[0] = '?'; dst[1] = 0; return; }
  while (z < 4 && (name[z] & 0xc0) == 0x80) z++;
  memcpy(dst, name, z);
  dst[z] = 0;
}

static float avatar_diameter(int m) {
  float usable_width = NV_TELA_W - 2.0f * NV_MARGEM_X;
  float d;
  if (m <= 0) return PS_AV_MAX;
  d = usable_width / ((float)m + PS_GAP_RATIO * (float)(m - 1));
  return anim_clamp(d, PS_AV_MIN, PS_AV_MAX);
}

static float avatar_gap(int m, float d) {
  float usable_width = NV_TELA_W - 2.0f * NV_MARGEM_X;
  float g = d * PS_GAP_RATIO;
  if (m <= 1) return g;
  { float remaining_gap = (usable_width - (float)m * d) / (float)(m - 1);
    // Stretch gaps when space remains and compress them to the minimum when space is scarce.
    float maximum_gap = d * PS_GAP_RATIO_MAX;
    if (remaining_gap > g) g = remaining_gap > maximum_gap ? maximum_gap : remaining_gap;
    else g = remaining_gap; }
  return g < PS_GAP_MIN ? PS_GAP_MIN : g;
}

// Background styles: movies, light and projector. Minimal GPU effects replace the movie wall with light.
static int background_mode(void) {
  int m = ajustes_ps_fundo();
  if (m == PS_FUNDO_FILMES && gfx_efeitos_minimos()) return PS_FUNDO_LUZ;
  return m;
}

static const char *wall_urls[PS_WALL_MAX];
static void build_scene(PSCena *c, int reduced_motion) {
  int m = profiles_count(), i;
  const AccountProfile *p = m > 0 && focus >= 0 && focus < m ? profiles_get(focus) : NULL;
  memcpy(c->luz, wall_light, sizeof c->luz);
  { float d = avatar_diameter(m), gap = avatar_gap(m, d), width = (float)m * d + (float)(m - 1) * gap;
    c->xFoco = m > 0 ? (NV_TELA_W - width) * 0.5f + (float)focus * (d + gap) + d * 0.5f
                     : NV_TELA_W * 0.5f; }
  c->perfil = p ? p->index : 0;
  c->semParede = p ? p->has_pin : 1;
  for (i = 0; i < wall_count && i < PS_WALL_MAX; i++) wall_urls[i] = wall_items[i].url;
  c->mural = wall_urls;
  c->muralN = wall_count;
  c->reduzida = reduced_motion;
  c->parado = pin_slot >= 0;
}

// Continue Watching card.
static void build_continue_card(ContinueCard *k) {
  const ProfileContent *c = &k->c;
  k->meta[0] = 0;
  if (c->is_series && c->season > 0 && c->episode > 0) {
    snprintf(k->meta, sizeof k->meta, i18n("S%dE%d"), c->season, c->episode);
    if (c->episode_name[0]) {
      size_t n = strlen(k->meta);
      snprintf(k->meta + n, sizeof k->meta - n, " \xc2\xb7 %s", c->episode_name);
    }
  }
  if (c->remaining_min >= 60 && c->remaining_min % 60)
    snprintf(k->remaining, sizeof k->remaining, i18n("%dh %dmin remaining"),
             c->remaining_min / 60, c->remaining_min % 60);
  else snprintf(k->remaining, sizeof k->remaining, i18n("%d min remaining"), c->remaining_min);
}

static void update_continue_cards(void) {
  int i, m = profiles_count(), enabled = ajustes_ps_continuar();
  // Disabling Continue Watching prevents card construction and poster requests; a setting change rebuilds the set.
  if (continue_count != m || enabled != continue_enabled) {
    memset(continue_cards, 0, sizeof continue_cards);
    if (enabled) {
      profile_content_register();
      for (i = 0; i < m && i < ACCOUNT_PROFILE_MAX; i++) {
        continue_cards[i].available = profile_content_from(profiles_get(i), &continue_cards[i].c);
        if (continue_cards[i].available) build_continue_card(&continue_cards[i]);
      }
    }
    continue_count = m;
    continue_enabled = enabled;
  }
  // Request textures during update only, for cards appearing or disappearing.
  for (i = 0; i < m && i < ACCOUNT_PROFILE_MAX; i++)
    continue_cards[i].tex = continue_cards[i].available && continue_cards[i].c.poster[0] && focus_animation[i] > 0.02f
      ? tex_obter_larg(continue_cards[i].c.poster, PS_CONT_POSTER_W) : 0;
}

static void draw_continue_card(int i, float cx, float top_y, float f, float a) {
  const ContinueCard *k = &continue_cards[i];
  GfxRect r;
  float af = (f > 1.0f ? 1.0f : f) * a, x, avail, yc, cr, cg, cb;
  TxtLinha meta, title, rest;
  if (!k->available || af < 0.01f) return;
  r.w = PS_CONT_W; r.h = PS_CONT_H;
  r.x = cx - r.w * 0.5f;
  if (r.x < 40.0f) r.x = 40.0f;
  if (r.x + r.w > NV_TELA_W - 40.0f) r.x = NV_TELA_W - 40.0f - r.w;
  r.y = top_y + (1.0f - (f > 1.0f ? 1.0f : f)) * 14.0f;   // Rise while receiving focus.
  plrui_material(r, PS_CONT_RADIUS, 0, af);
  // Use the established 108x156 poster, title typography, episode line and full-width remaining-time track.
  yc = r.y + r.h * 0.5f;
  { GfxRect poster = { r.x + 20.0f, yc - PS_CONT_POSTER_H * 0.5f, PS_CONT_POSTER_W, PS_CONT_POSTER_H };
    if (k->tex) {
      gfx_tex_aspect_atual = tex_aspecto(k->c.poster);
      gfx_card_forcar_cover_atual = 1.0f;
      gfx_rect(poster, k->tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 14.0f / PS_CONT_POSTER_H, 0, 0, 0, af);
      gfx_card_forcar_cover_atual = 0.0f;
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(poster, 14.0f / PS_CONT_POSTER_H, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                   NV_COR_ESQUELETO_B, af); }
  x = r.x + 20.0f + PS_CONT_POSTER_W + 28.0f;
  avail = r.x + r.w - 34.0f - x;
  ajustes_acento_marca(&cr, &cg, &cb);
  { TxtLinha kick = txt_linha_corta(TXT_CAPTION2, "Continue Watching",
                                    (int)(cr * 255.0f), (int)(cg * 255.0f), (int)(cb * 255.0f), 255, avail);
    float y = r.y + 26.0f;
    txt_desenhar_alpha(kick, x, y, af);
    y += (float)kick.h + 6.0f;
    title = txt_linha_corta(TXT_TITULO3, k->c.title, 244, 245, 248, 255, avail);
    txt_desenhar_alpha(title, x, y, af);
    y += (float)title.h + 2.0f;
    meta = k->meta[0] ? txt_linha_corta(TXT_CALLOUT, k->meta, 190, 193, 202, 255, avail) : (TxtLinha){ 0 };
    if (meta.w) txt_desenhar_alpha(meta, x, y, af); }
  rest = txt_linha_corta(TXT_CAPTION2, k->remaining, 200, 203, 212, 255, avail * 0.5f);
  ajustes_acento(&cr, &cg, &cb);
  { float pr = k->c.progress > 1.0f ? 1.0f : k->c.progress;
    float ty = r.y + r.h - 30.0f, tw = avail - (rest.w ? (float)rest.w + 18.0f : 0.0f);
    plrui_trilho((GfxRect){ x, ty - 3.0f, tw, 6.0f }, pr, cr, cg, cb, af);
    if (rest.w) txt_desenhar_alpha(rest, x + tw + 18.0f, ty - (float)rest.h * 0.5f, af); }
}

// Profile ambient artwork cross-fades over 0.45 seconds using cached 96x54 blur copies. Fall back to the profile avatar, then its color light; never substitute the movie wall (#295).
#define PS_AMB_FADE_S  0.45f
#define PS_AMB_VEIL     0.55f   // Blurred avatars are brighter than backdrops, so use a darker veil and a wash of profile color.
#define PS_AMB_AVATAR_VEIL  0.62f
#define PS_AMB_AVATAR_COLOR  0.22f
#define PS_AMB_WIDTH    480.0f
static int   ambient_current = -1, ambient_previous = -1;
static float ambient_fade = 1.0f;                       // Cross-fade from previous to current profile, zero to one.
static GLuint ambient_sources[ACCOUNT_PROFILE_MAX];       // Sharp texture requested during update.
static int   ambient_ready;                    // Blur copy is ready.

// Profile background artwork, falling back to its avatar.
static const char *ambient_url(int i) {
  const AccountProfile *p = i >= 0 && i < ACCOUNT_PROFILE_MAX ? profiles_get(i) : NULL;
  if (!p) return NULL;
  if (p->background_url[0]) return p->background_url;
  if (p->avatar_url[0]) return p->avatar_url;
  return NULL;
}
static int ambient_is_avatar(int i) {
  const AccountProfile *p = i >= 0 && i < ACCOUNT_PROFILE_MAX ? profiles_get(i) : NULL;
  return p && !p->background_url[0] && p->avatar_url[0];
}

static int ambient_has_url(int i) { return ambient_url(i) != NULL; }

// Cached blurred copy, or zero when absent, pending, or limited by the per-frame blur budget.
static GLuint ambient_blur(int i) {
  const char *u = ambient_url(i);
  if (!u || !ambient_sources[i]) return 0;
  return gfx_desfocado(ambient_sources[i], u);
}

// Draw one blurred layer and its avatar color wash.
static void ambient_layer(int i, GLuint t, float alpha) {
  GfxRect screen = { 0, 0, NV_TELA_W, NV_TELA_H };
  float r, g, b;
  const AccountProfile *p = profiles_get(i);
  // Force ambient artwork to cover the screen instead of letterboxing portrait avatars.
  gfx_tex_aspect_atual = tex_aspecto(ambient_url(i));
  gfx_card_forcar_cover_atual = ambient_is_avatar(i) ? 1.0f : 0.0f;
  gfx_rect(screen, t, GFX_CARD, 0, 0, 0, 0.0f, 1, 1, 1, alpha);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0;
  if (ambient_is_avatar(i) && p && parse_color(p->color_hex, &r, &g, &b)) {
    readable_color(&r, &g, &b);
    gfx_cor(screen, 0.0f, r, g, b, PS_AMB_AVATAR_COLOR * alpha);
  }
}

static void change_ambient(int next) {
  if (next == ambient_current) return;
  ambient_previous = ambient_current;
  ambient_current = next;
  ambient_fade = 0.0f;
}

static void update_ambient(float dt, int reduced_motion) {
  int i, m = profiles_count();
  if (ajustes_ps_fundo() != 2) { ambient_current = ambient_previous = -1; ambient_fade = 1.0f; return; }
  // Texture requests occur during update; draw only reads the ready texture ID.
  for (i = 0; i < ACCOUNT_PROFILE_MAX; i++) {
    const AccountProfile *p = i < m ? profiles_get(i) : NULL;
    const char *u = p ? ambient_url(i) : NULL;
    ambient_sources[i] = u ? tex_obter_larg_qualquer(u, PS_AMB_WIDTH) : 0;
  }
  if (ambient_current < 0) { ambient_current = ambient_previous = focus; ambient_fade = 1.0f; }
  else change_ambient(focus);
  if (reduced_motion) { ambient_fade = 1.0f; return; }
  // Start the cross-fade clock after the new artwork is ready, avoiding a jump at the end of its download.
  if (ambient_fade < 1.0f && (!ambient_has_url(ambient_current) || ambient_ready)) {
    ambient_fade += dt / PS_AMB_FADE_S;
    if (ambient_fade > 1.0f) ambient_fade = 1.0f;
  }
}

// Draw ambient artwork and return the fraction covering the screen for the fallback background.
static float draw_ambient(float a) {
  GfxRect screen = { 0, 0, NV_TELA_W, NV_TELA_H };
  float te = ambient_fade < 1.0f ? ambient_fade * ambient_fade * (3.0f - 2.0f * ambient_fade) : 1.0f;
  GLuint ta = ambient_previous != ambient_current ? ambient_blur(ambient_previous) : 0;
  GLuint tn = ambient_blur(ambient_current);
  // The previous image remains underneath a new image, or fades out when the new profile has no artwork.
  float pa = ta && te < 1.0f ? (tn ? 1.0f : 1.0f - te) : 0.0f;
  float na = tn ? te : 0.0f;
  float coverage = 1.0f - (1.0f - pa) * (1.0f - na);
  ambient_ready = tn != 0 || !ambient_has_url(ambient_current);
  if (pa > 0.0f) ambient_layer(ambient_previous, ta, a * pa);
  if (na > 0.0f) ambient_layer(ambient_current, tn, a * na);
  if (coverage > 0.0f) {
    // The veil follows the dominant layer to maintain brightness through the fade.
    float current_veil = ambient_is_avatar(ambient_current) ? PS_AMB_AVATAR_VEIL : PS_AMB_VEIL;
    float previous_veil = ambient_is_avatar(ambient_previous) ? PS_AMB_AVATAR_VEIL : PS_AMB_VEIL;
    float veil = (pa > 0.0f && na > 0.0f) ? previous_veil + (current_veil - previous_veil) * te
              : (na > 0.0f ? current_veil : previous_veil);
    gfx_cor(screen, 0.0f, 0.0f, 0.0f, 0.0f, veil * a * coverage);
  }
  return coverage;
}

void profile_picker_init(void) {
  int i;
  complete = wants_logout = wants_retry = 0;
  preparing = 0;
  pin_slot = -1;
  pin[0] = 0;
  pin_focus = PS_PIN_START;
  wrong_pin = 0;
  pin_network_error = 0;
  verifying = 0;
  entrance_animation = 0.0f;
  pin_animation = 0.0f;
  cachearte_limpar_referencias_grupo(NV_CACHE_ARTE_GRUPO_PERFIL);
  wall_count = 0;
  configure_wall();
  wall_revision = ~0u;
  wall_time = 0.0f;
  burst_time = 0.0f;
  wall_pressure = 0.0f;
  init_wall_particles();
  init_wall_burst();
  account_cancel_selection();
  selection_action = 0;
  // Focus starts at the saved active profile; the backend owns that rule.
  focus = profiles_suggested_index();
  continue_count = -1;
  ambient_current = ambient_previous = -1; ambient_fade = 1.0f; ambient_ready = 0;
  // Save the current profile's poster wall while its catalog is still in memory.
  psparede_registrar();
  psestilos_iniciar();
  for (i = 0; i < ACCOUNT_PROFILE_MAX; i++) focus_animation[i] = (i == focus) ? 1.0f : 0.0f;
  update_wall_light_targets();
  memcpy(wall_light, wall_light_target, sizeof wall_light);
  memcpy(wall_light_start, wall_light_target, sizeof wall_light_start);
  memcpy(wall_previous_light_target, wall_light_target, sizeof wall_previous_light_target);
  wall_light_time = 0.30f;
}

static void choose_profile(int i) {
  const AccountProfile *p = profiles_get(i);
  switch (profiles_action(i)) {
    case PROFILE_ACTION_PIN:
      pin_slot = i; pin[0] = 0; pin_focus = PS_PIN_START; wrong_pin = pin_network_error = 0;
      return;
    case PROFILE_ACTION_ENTER:
      if (p) { selection_action = account_select_profile(p->index, NULL); verifying = selection_action != 0; }
      return;
    case PROFILE_ACTION_RETRY:
      wants_retry = 1;
      return;
    default:
      return;
  }
}

// Dispatch verification asynchronously so the screen keeps drawing.
static void verify_pin(void) {
  const AccountProfile *p = profiles_get(pin_slot);
  if (!pin[0]) return;
  if (!p) { pin_network_error = 1; return; }
  selection_action = account_select_profile(p->index, pin);
  verifying = selection_action != 0;
  wrong_pin = pin_network_error = 0;
  /* Rust copied the action's secret before returning. Retain the masked digit
   * count for the original UI, and clear the digits after result/cancellation. */
}

// One digit from the pad or remote; the fourth digit submits automatically.
static void enter_pin_digit(int digit) {
  size_t z = strlen(pin);
  if (z >= PS_PIN_LEN) return;
  pin[z] = (char)('0' + digit); pin[z + 1] = 0;
  wrong_pin = pin_network_error = 0;
  if (z + 1 == PS_PIN_LEN) verify_pin();
}

static void pin_event(SDL_Keycode k) {
  if (verifying) {
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE) {
      account_cancel_selection(); selection_action = 0;
      verifying = 0; pin_network_error = 0; wrong_pin = 0;
    }
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE) {
    if (pin[0]) { pin[strlen(pin) - 1] = 0; wrong_pin = pin_network_error = 0; }
    else pin_slot = -1;
    return;
  }
  // Remote number keys type directly regardless of focus.
  if (k >= SDLK_0 && k <= SDLK_9) { enter_pin_digit((int)(k - SDLK_0)); return; }
  if (k >= SDLK_KP_1 && k <= SDLK_KP_9) { enter_pin_digit((int)(k - SDLK_KP_1) + 1); return; }
  if (k == SDLK_KP_0) { enter_pin_digit(0); return; }
  if (k == SDLK_BACKSPACE) { if (pin[0]) { pin[strlen(pin) - 1] = 0; wrong_pin = pin_network_error = 0; } return; }
  if (k == SDLK_LEFT) {
    if (pin_focus % PS_KEY_COLUMNS > 0 && pin_focus - 1 != PS_PIN_BLANK) pin_focus--;
    return;
  }
  if (k == SDLK_RIGHT) { if (pin_focus % PS_KEY_COLUMNS < PS_KEY_COLUMNS - 1) pin_focus++; return; }
  if (k == SDLK_UP)    { if (pin_focus >= PS_KEY_COLUMNS) pin_focus -= PS_KEY_COLUMNS; return; }
  if (k == SDLK_DOWN)  {
    if (pin_focus + PS_KEY_COLUMNS < PS_KEY_COLUMNS * PS_KEY_ROWS) {
      pin_focus += PS_KEY_COLUMNS;
      if (pin_focus == PS_PIN_BLANK) pin_focus = PS_PIN_ZERO;   // Seven moves down to zero.
    }
    return;
  }
  if (k != SDLK_RETURN && k != SDLK_KP_ENTER) return;

  if (pin_focus == PS_PIN_DELETE) { if (pin[0]) pin[strlen(pin) - 1] = 0; return; }
  if (pin_focus == PS_PIN_BLANK) return;
  enter_pin_digit(pin_focus == PS_PIN_ZERO ? 0 : pin_focus + 1);
}

static void focus_profile(int slot, int index) {
  const AccountProfile *p = profiles_get(slot);
  if (!preparing && pin_slot < 0 && p && p->index == index) focus = slot;
}
static void activate_profile(int slot, int index) {
  const AccountProfile *p = profiles_get(slot);
  if (!preparing && pin_slot < 0 && p && p->index == index) {
    focus = slot;
    choose_profile(slot);
  }
}
static void focus_pin_key(int key, int b) {
  (void)b;
  if (pin_slot >= 0 && !verifying && !preparing &&
      key >= 0 && key < PS_KEY_COLUMNS * PS_KEY_ROWS && key != PS_PIN_BLANK)
    pin_focus = key;
}
static void retry_profiles(int a, int b) {
  (void)a; (void)b;
  if (!preparing && !profiles_count() && sync_estado() == SYNC_FALHOU) wants_retry = 1;
}

void profile_picker_event(const SDL_Event *e) {
  SDL_Keycode k;
  int m = profiles_count();
  if (e->type != SDL_KEYDOWN) return;
  // Ignore input while the selected profile's Home is being prepared; the app owns its timeout.
  if (preparing) return;
  k = e->key.keysym.sym;
  if (pin_slot >= 0) { pin_event(k); return; }

  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) { wants_logout = 1; return; }
  // OK selects a visible profile, or retries when the list has not loaded.
  if (m == 0 && sync_estado() == SYNC_FALHOU &&
      (k == SDLK_RETURN || k == SDLK_KP_ENTER)) { wants_retry = 1; return; }
  if (k == SDLK_RIGHT) { if (focus < m - 1) focus++; }
  else if (k == SDLK_LEFT) { if (focus > 0) focus--; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) choose_profile(focus);
}

void profile_picker_update(float dt, Uint32 now) {
  int i, reduced_motion = ajustes_animacoes_reduzidas();
  float raw_delta = dt;
  (void)now;

  // Measure the frame interval before clamping to detect sustained pressure; a long resume interval resets the accumulator.
  if (raw_delta < 0.0f || raw_delta >= 0.25f) wall_pressure = 0.0f;
  else if (!reduced_motion && raw_delta > (1.0f / 30.0f)) {
    wall_pressure += raw_delta;
    if (wall_pressure >= 1.0f && !wall_auto_compact) {
      wall_auto_compact = 1;
      particle_count = 20;
      burst_count = 18;
      init_wall_particles();
      init_wall_burst();
    }
  } else {
    wall_pressure -= raw_delta * 2.0f;
    if (wall_pressure < 0.0f) wall_pressure = 0.0f;
  }
  // Clamp delta once for every fade, spring, focus, PIN animation and effect.
  if (dt < 0.0f) dt = 0.0f;
  if (dt > 0.05f) dt = 0.05f;

  update_ambient(dt, reduced_motion);
  rebuild_wall();
  update_continue_cards();
  // Request wall textures during update through the shared Home texture cache.
  for (i = 0; i < wall_count; i++) {
    // Revalidate the texture each update to keep it warm and replace evicted IDs. Retain a smaller texture during promotion.
    wall_textures[i] = tex_obter_larg_qualquer(wall_items[i].url,
                                               wall_request_width());
    if (wall_textures[i]) {
      if (reduced_motion) wall_fade[i] = 1.0f;
      else if (wall_fade[i] < 1.0f) {
        wall_fade[i] += dt / 0.30f;
        if (wall_fade[i] > 1.0f) wall_fade[i] = 1.0f;
      }
    }
  }
  // Advance visual clocks only while this screen is active and the PIN modal is closed. Reduced motion retains a static wall and particles.
  if (reduced_motion) {
    wall_time = 0.0f;
    burst_time = 0.0f;
    wall_pressure = 0.0f;
  }
  else if (pin_slot < 0) {
    wall_time += dt;
    burst_time += dt;
  }

  update_wall_light_targets();
  { PSCena scene;
    int mode = background_mode();
    if (mode == PS_FUNDO_FILMES || mode == PS_FUNDO_LUZ || mode == PS_FUNDO_PROJETOR) {
      build_scene(&scene, reduced_motion);
      psestilos_atualizar(dt, &scene, mode);
    } else if (mode == PS_FUNDO_ARTE) {
      // Color light behind profiles without artwork or avatar.
      build_scene(&scene, reduced_motion);
      psestilos_atualizar(dt, &scene, PS_FUNDO_LUZ);
    } }
  { int changed = 0;
    for (i = 0; i < 6; i++)
      if (fabsf(wall_light_target[i] - wall_previous_light_target[i]) > 0.0001f) { changed = 1; break; }
    if (changed) {
      memcpy(wall_light_start, wall_light, sizeof wall_light_start);
      memcpy(wall_previous_light_target, wall_light_target, sizeof wall_previous_light_target);
      wall_light_time = 0.0f;
    }
  }
  if (reduced_motion) {
    memcpy(wall_light, wall_light_target, sizeof wall_light);
    memcpy(wall_light_start, wall_light_target, sizeof wall_light_start);
    memcpy(wall_previous_light_target, wall_light_target, sizeof wall_previous_light_target);
    wall_light_time = 0.30f;
  } else {
    float t;
    wall_light_time += dt;
    t = anim_clamp(wall_light_time / 0.30f, 0.0f, 1.0f);
    for (i = 0; i < 6; i++)
      wall_light[i] = wall_light_start[i] + (wall_light_target[i] - wall_light_start[i]) * t;
  }

  entrance_animation = anim_reduzida(anim_mola(entrance_animation, 1.0f, dt, NV_MOLA_TELA),
                              1.0f, reduced_motion);
  pin_animation = anim_reduzida(anim_mola(pin_animation, pin_slot >= 0 ? 1.0f : 0.0f, dt, NV_MOLA_TELA),
                          pin_slot >= 0 ? 1.0f : 0.0f, reduced_motion);
  for (i = 0; i < ACCOUNT_PROFILE_MAX; i++) {
    float target = (i == focus && pin_slot < 0) ? 1.0f : 0.0f;
    focus_animation[i] = anim_mola(focus_animation[i], target, dt,
                            target > focus_animation[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    if (reduced_motion) focus_animation[i] = target;
  }

  if (pin_shake > 0.0f) { pin_shake -= dt; if (pin_shake < 0.0f) pin_shake = 0.0f; }
  { int result = account_selection_result(selection_action);
  if (verifying && result) {
    verifying = 0;
    if (result == 1) {
      // Apply remaining C profile-dependent UI hooks on the drawing thread after Rust commits selection.
      account_apply_profile();
      memset(pin, 0, sizeof pin);
      pin_slot = -1;
      complete = 1;
    } else if (result == -2 || result == -3) {
      pin_network_error = 1;
      memset(pin, 0, sizeof pin);
    } else {
      wrong_pin = 1;
      memset(pin, 0, sizeof pin);
      pin_shake = reduced_motion ? 0.0f : 0.40f;
    }
  }
  }
  if (focus >= profiles_count()) focus = profiles_count() > 0 ? profiles_count() - 1 : 0;

  // Do not dismiss an empty picker while profile loading is still running; zero profiles during an in-flight request is not a confirmed single-profile account.
  if (sync_estado() == SYNC_RODANDO && profiles_count() == 0) return;

  // After successful sync, an empty or single unlocked list may continue directly. A manually opened picker still offers profile selection regardless of the launch confirmation flag.
  if (sync_estado() == SYNC_PRONTO && profiles_without_choice() && pin_slot < 0) complete = 1;
}

void profile_picker_prepare(int enabled, Uint32 now) {
  preparing = enabled;
  preparation_started = now;
}
int profile_picker_preparing(void) { return preparing; }

int profile_picker_wants_logout(void) { int v=wants_logout; wants_logout=0; return v; }
int profile_picker_wants_retry(void) { int v=wants_retry; wants_retry=0; return v; }
void profile_picker_keep_active(void) {
  // Back confirms the saved eligible profile so Home and trailers leave the selection state together.
  complete = 1;
}

// Drawing.

// Avatar layers: dark socket, account color, optional image. The socket preserves consistent dimming over every background.
static void draw_avatar(GfxRect a, const AccountProfile *p, float f, float alpha) {
  float cr = 0.12f, cg = 0.53f, cb = 0.90f;
  float brightness = 0.55f + 0.45f * f;
  GLuint image;
  parse_color(p->color_hex, &cr, &cg, &cb);
  readable_color(&cr, &cg, &cb);
  gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, 0.09f, 0.09f, 0.10f, alpha);
  gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, brightness * alpha);
  image = p->avatar_url[0] ? tex_obter_larg(p->avatar_url, a.w) : 0;
  if (image) {
    gfx_tex_aspect_atual = tex_aspecto(p->avatar_url);
    gfx_rect(a, image, GFX_AVATAR, 0, 0, 0, 0, 1, 1, 1, brightness * alpha);
    gfx_tex_aspect_atual = 0;
  } else {
    char initial[8];
    TxtLinha l;
    first_character(p->name, initial, sizeof initial);
    l = txt_linha(a.w >= 230.0f ? TXT_TITULO1 : TXT_TITULO2, initial, 255, 255, 255, 255);
    txt_desenhar_alpha(l, a.x + (a.w - l.w) * 0.5f, a.y + (a.h - l.h) * 0.5f,
                       (0.80f + 0.20f * f) * alpha);
  }
}

// Glow behind the focused avatar.
static void draw_halo(GfxRect a, const AccountProfile *p, float f) {
  float cr = 0.12f, cg = 0.53f, cb = 0.90f;
  if (f <= 0.02f) return;
  parse_color(p->color_hex, &cr, &cg, &cb);
  readable_color(&cr, &cg, &cb);
  // Use one diffuse radial profile-colored light, twice the avatar size, only when focused.
  { float growth = a.w * 1.1f;
    GfxRect r = { a.x - growth * 0.5f, a.y - growth * 0.5f, a.w + growth, a.h + growth };
    gfx_rect(r, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, cr, cg, cb, 0.45f * f);
    return; }
}

// Show the player's twelve-dot loading ring over the selected avatar. Reduced motion keeps dots stationary and pulses brightness.
static void draw_spinner(GfxRect av, Uint32 now, float alpha, int reduced_motion) {
  float cx = av.x + av.w * 0.5f, cy = av.y + av.h * 0.5f;
  float radius = av.w * 0.24f, dot = av.w * 0.05f;
  float t = (float)(now - preparation_started);
  float rotation_angle = reduced_motion ? 0.0f : t * 0.006f;
  float pulse = reduced_motion ? 0.75f + 0.25f * sinf(t * 0.004f) : 1.0f;
  float ar, ag, ab;
  int k;
  if (dot < 6.0f) dot = 6.0f;
  gfx_rect(av, 0, GFX_DISCO, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.70f * alpha);
  ajustes_acento(&ar, &ag, &ab);
  for (k = 0; k < 12; k++) {
    float angle = k * 6.2831853f / 12.0f + rotation_angle;
    float br = .18f + .82f * k / 11.0f;
    GfxRect pt = { cx + cosf(angle) * radius - dot * 0.5f,
                   cy + sinf(angle) * radius - dot * 0.5f, dot, dot };
    gfx_cor(pt, .5f, ar, ag, ab, br * pulse * alpha);
  }
}

static void draw_background(void) {
  GfxRect screen = { 0, 0, NV_TELA_W, NV_TELA_H };
  // The wall uses true black underneath. With the wall disabled, reuse the striped login background.
  if (ajustes_ps_fundo() == 1 && login_draw_background(1.0f)) return;
  gfx_cor(screen, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
}

static void draw_pin(void) {
  static const char *PIN_LABELS[PS_KEY_COLUMNS * PS_KEY_ROWS] =
    { "1","2","3", "4","5","6", "7","8","9", "","0","←" };
  const AccountProfile *p = profiles_get(pin_slot);
  float width = PS_KEY_COLUMNS * PS_KEY + (PS_KEY_COLUMNS - 1) * PS_KEY_GAP;
  float x0 = (NV_TELA_W - width) * 0.5f;
  float y0 = 540.0f;
  float a = pin_animation;
  int i;
  size_t n = strlen(pin), dot_count;
  // The PIN modal consumes background touches so they cannot select another profile.
  ponteiro_camada();
  ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);

  { GfxRect screen = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(screen, 0.0f, 0.02f, 0.02f, 0.025f, 0.88f * a); }
  if (!p) return;

  // Show the profile identity above the keypad so the user knows which lock is being opened.
  { GfxRect av = { (NV_TELA_W - 132.0f) * 0.5f, 176.0f, 132.0f, 132.0f };
    draw_avatar(av, p, 1.0f, a); }

  { char t[128];
    TxtLinha l;
    snprintf(t, sizeof t, i18n("PIN for %s"), p->name[0] ? p->name : i18n("profile"));
    l = txt_linha(TXT_TITULO3, t, 255, 255, 255, 255);
    txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, 344.0f, a); }

  // Mask digits with round dots rather than asterisks or readable numbers.
  dot_count = n > 4 ? n : 4;
  if (dot_count > PS_PIN_MAX) dot_count = PS_PIN_MAX;
  { float total = (float)dot_count * PS_DOT_STEP - (PS_DOT_STEP - PS_DOT);
    float px = (NV_TELA_W - total) * 0.5f;
    // Wrong PIN: dots shake and settle with a decaying sine over 0.4 seconds.
    if (pin_shake > 0.0f) px += sinf(pin_shake * 50.0f) * 18.0f * (pin_shake / 0.40f);
    size_t k;
    for (k = 0; k < dot_count; k++) {
      GfxRect d = { px + (float)k * PS_DOT_STEP, 434.0f, PS_DOT, PS_DOT };
      gfx_rect(d, 0, GFX_DISCO, 0, 0, 0, 0, 1, 1, 1, (k < n ? 0.96f : 0.20f) * a);
    } }

  { const char *message = NULL;
    int cr = 236, cg = 108, cb = 108;
    if (verifying)   { message = "Verifying…"; cr = 200; cg = 202; cb = 210; }
    else if (pin_network_error)  { message = "Unable to connect. Try again."; cg = 150; cb = 150; }
    else if (wrong_pin){ message = "Incorrect PIN"; }
    if (message) {
      TxtLinha l = txt_linha(TXT_BODY, message, cr, cg, cb, 255);
      txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, 486.0f, a);
    } }

  for (i = 0; i < PS_KEY_COLUMNS * PS_KEY_ROWS; i++) {
    int col = i % PS_KEY_COLUMNS, row = i / PS_KEY_COLUMNS;
    GfxRect r = { x0 + col * (PS_KEY + PS_KEY_GAP),
                  y0 + row * (PS_KEY + PS_KEY_GAP), PS_KEY, PS_KEY };
    int f = (i == pin_focus && !verifying);
    if (i == PS_PIN_BLANK) continue;
    if (pin_slot >= 0 && !verifying && !preparing)
      ponteiro_alvo(r.x, r.y, r.w, r.h, focus_pin_key, NULL, i, pin_slot);
    TxtLinha l;
    // Focused keypad keys use a dark surface, white text and an outer four-pixel ring.
    if (f) {
      gfx_cor(r, NV_RAIO_PILL, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, a);
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      gfx_anel_fora(r, NV_RAIO_PILL, 0.0f, NV_ANEL_FOCO, ar, ag, ab, a);
    } else {
      gfx_cor(r, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.09f * a);
    }
    l = txt_linha(TXT_TITULO3, PIN_LABELS[i], f ? 255 : 214, f ? 255 : 216, f ? 255 : 224, 255);
    txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a);
  }
}

void profile_picker_draw(Uint32 now) {
  int i, m = profiles_count();
  float d, gap, width, x, rise, a;
  int reduced_motion = ajustes_animacoes_reduzidas();

  ponteiro_camada();
  ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);

  draw_background();

  // The entrance spring raises the screen slightly, matching the app's other transitions.
  rise = (1.0f - entrance_animation) * 28.0f;
  a = entrance_animation;

  // Keep posters at low opacity so profiles dominate. Respect the background setting.
  { int mode = background_mode();
    if (mode == PS_FUNDO_FILMES || mode == PS_FUNDO_LUZ || mode == PS_FUNDO_PROJETOR) {
      PSCena scene;
      build_scene(&scene, reduced_motion);
      psestilos_desenhar(&scene, mode, a * (pin_slot >= 0 ? 0.30f : 1.0f));
    } else {
      float coverage = mode == PS_FUNDO_ARTE ? draw_ambient(a) : 0.0f;
      // Where profile artwork is absent or still loading, show the profile color light instead of the movie wall (#295).
      if (mode == PS_FUNDO_ARTE && coverage < 0.999f) {
        PSCena scene;
        build_scene(&scene, reduced_motion);
        psestilos_desenhar(&scene, PS_FUNDO_LUZ,
                           a * (1.0f - coverage) * (pin_slot >= 0 ? 0.30f : 1.0f));
      } else if (mode != PS_FUNDO_LISTRAS && mode != PS_FUNDO_ARTE && coverage < 0.999f)
        draw_wall(a * (1.0f - coverage) * (pin_slot >= 0 ? 0.30f : 1.0f), reduced_motion);
    } }

  { TxtLinha t = txt_linha(TXT_TITULO1, "Who's watching?", 255, 255, 255, 255);
    TxtLinha shadow = txt_linha(TXT_TITULO1, "Who's watching?", 0, 0, 0, 230);
    txt_desenhar_alpha(shadow, (NV_TELA_W - shadow.w) * 0.5f + 2.0f,
                       PS_TITLE_Y + rise + 3.0f, a * 0.78f);
    txt_desenhar_alpha(t, (NV_TELA_W - t.w) * 0.5f, PS_TITLE_Y + rise, a); }

  // Keep the corner free of branding, as in the cleaned UI.

  // Explain that profiles are loading instead of leaving an empty row under the title.
  if (m == 0) {
    const char *msg = sync_estado() == SYNC_FALHOU
      ? "Unable to load profiles. Press OK to retry"
      : "Loading your account profiles…";
    TxtLinha e = txt_linha(TXT_BODY, msg, 176, 179, 190, 255);
    if (!preparing && sync_estado() == SYNC_FALHOU)
      ponteiro_alvo((NV_TELA_W - e.w) * 0.5f, 450.0f + rise,
                    (float)e.w, (float)e.h + 40.0f, NULL, retry_profiles, 0, 0);
    txt_desenhar_alpha(e, (NV_TELA_W - e.w) * 0.5f, 470.0f + rise, a);
    return;
  }

  { TxtLinha s = txt_linha(TXT_CALLOUT,
                           "Each profile has its own list and its own progress.",
                           166, 169, 180, 255);
    TxtLinha shadow = txt_linha(TXT_CALLOUT,
                                "Each profile has its own list and its own progress.",
                                0, 0, 0, 230);
    txt_desenhar_alpha(shadow, (NV_TELA_W - shadow.w) * 0.5f + 2.0f,
                       PS_SUB_Y + rise + 3.0f, a * 0.78f);
    txt_desenhar_alpha(s, (NV_TELA_W - s.w) * 0.5f, PS_SUB_Y + rise, a); }

  d = avatar_diameter(m);
  gap = avatar_gap(m, d);
  width = (float)m * d + (float)(m - 1) * gap;
  x = (NV_TELA_W - width) * 0.5f;

  for (i = 0; i < m; i++) {
    const AccountProfile *p = profiles_get(i);
    float f = focus_animation[i];
    float px = x + (float)i * (d + gap);
    float growth = d * NV_FOCO_ESCALA_P * f;
    float y = PS_ROW_Y + rise;
    GfxRect av = { px - growth * 0.5f, y - growth * 0.5f - NV_FOCO_LIFT * f,
                   d + growth, d + growth };
    TxtLinha name, shadow;
    int c;
    if (!p) continue;

    draw_halo(av, p, f);
    // Draw the focus ring as a larger concentric white disc to preserve uniform thickness at every size.
    if (f > 0.02f) {
      float e = NV_ANEL_FOCO * f;
      gfx_rect((GfxRect){ av.x - e, av.y - e, av.w + e * 2, av.h + e * 2 },
               0, GFX_DISCO, 0, 0, 0, 0, 0.97f, 0.97f, 0.99f, f * a);
    }
    // Animate only the focused avatar while the PIN modal is closed; the GIF subsystem holds one active animation.
    draw_avatar(av, p, f, a);
    if (preparing && i == focus) draw_spinner(av, now, a, reduced_motion);

    // Use brightness 176 for unfocused labels to keep them readable from the sofa.
    c = 176 + (int)(79.0f * f);
    name = txt_linha_corta(d >= 230.0f ? TXT_TITULO3 : TXT_HEADLINE, p->name,
                           c, c, c + 6 > 255 ? 255 : c + 6, 255, d + gap * 0.9f);
    if (!preparing && pin_slot < 0)
      ponteiro_alvo(av.x, av.y, av.w,
                    av.h + PS_NAME_GAP + (float)name.h + PS_BADGE_GAP + 32.0f,
                    focus_profile, activate_profile, i, p->index);
    shadow = txt_linha_corta(d >= 230.0f ? TXT_TITULO3 : TXT_HEADLINE, p->name,
                             0, 0, 0, 220, d + gap * 0.9f);
    txt_desenhar_alpha(shadow, px + (d - shadow.w) * 0.5f + 2.0f,
                       y + d + PS_NAME_GAP + 3.0f - NV_FOCO_LIFT * f * 0.5f,
                       a * 0.90f);
    txt_desenhar_alpha(name, px + (d - name.w) * 0.5f,
                       y + d + PS_NAME_GAP - NV_FOCO_LIFT * f * 0.5f, a);

    if (p->has_pin) {
      // Draw a PIN text pill because the bundled font does not contain the lock emoji.
      TxtLinha label = txt_linha(TXT_CAPTION, "PIN", 226, 228, 236, 255);
      float sy = y + d + PS_NAME_GAP + (float)name.h + PS_BADGE_GAP
                 - NV_FOCO_LIFT * f * 0.5f;
      GfxRect pill = { px + (d - ((float)label.w + 32.0f)) * 0.5f, sy,
                         (float)label.w + 32.0f, (float)label.h + 10.0f };
      gfx_cor(pill, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, (0.10f + 0.10f * f) * a);
      txt_desenhar_alpha(label, pill.x + 16.0f, pill.y + 5.0f, a);
    }
    // Show Continue Watching only for unlocked profiles, with the PIN and preparation overlays closed.
    if (!preparing && pin_slot < 0 && f > 0.02f) {
      float cy = PS_ROW_Y + d + PS_NAME_GAP + 44.0f + 28.0f - NV_FOCO_LIFT * f * 0.5f;
      draw_continue_card(i, px + d * 0.5f, (cy > PS_CONT_Y_MAX ? PS_CONT_Y_MAX : cy) + rise, f, a);
    }
  }

  // Keep the footer clear of hints; profile preparation text represents state and stays visible.
  if (preparing) {
    TxtLinha l = txt_linha(TXT_ILHA_APOIO, i18n("Preparing your profile…"), 209, 207, 204, 255);
    float panel_width = (float)l.w + PS_PILL_PAD * 2.0f;
    GfxRect panel = { (NV_TELA_W - panel_width) * 0.5f, PS_HINT_Y, panel_width, PS_PILL_H };
    plrui_material(panel, PS_PILL_H * 0.5f, 0, a);
    txt_desenhar_alpha(l, panel.x + PS_PILL_PAD, panel.y + (panel.h - (float)l.h) * 0.5f, a);
  }

  if (pin_animation > 0.004f) draw_pin();
}

#ifdef NV_PROFILE_PICKER_TEST
void profile_picker_test_state(ProfilePickerTestState *e) {
  int i;
  if (!e) return;
  memset(e, 0, sizeof *e);
  for (i = 0; i < 8; i++) e->focus[i] = focus_animation[i];
  e->pin = pin_animation;
  memcpy(e->light, wall_light, sizeof e->light);
  memcpy(e->light_target, wall_light_target, sizeof e->light_target);
  memcpy(e->fade, wall_fade, sizeof e->fade);
  e->wall_time = wall_time;
  e->burst_time = burst_time;
  e->wall_count = wall_count;
  e->particles = particle_count;
  e->burst = burst_count;
#ifdef NV_PROFILE_PICKER_TEST
  e->burst_drawn = burst_drawn;
#endif
  for (i = 0; i < 8; i++) e->continue_available[i] = continue_cards[i].available;
  e->ambient_fade = ambient_fade; e->ambient_current = ambient_current; e->ambient_previous = ambient_previous;
  e->ambient_source = !ambient_has_url(ambient_current) ? 0 : ambient_is_avatar(ambient_current) ? 2 : 1;
  e->pin_slot = pin_slot; e->pin_focus = pin_focus; e->verifying = verifying;
  e->pin_digits = (int)strlen(pin); e->complete = complete;
}
void profile_picker_test_key(int key) { SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = key; profile_picker_event(&event); }
void profile_picker_test_activate(int slot, int id) { activate_profile(slot, id); }
void profile_picker_test_focus_pin(int key) { focus_pin_key(key, 0); }
#endif

int profile_picker_complete(void) { return complete; }
