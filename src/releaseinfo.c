#include "releaseinfo.h"
#include "data.h"
#include "perfis.h"
#include "descoberta.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define RELEASE_CACHE_MAX 200
#define RELEASE_NO_DATE (-100000)
// Values also identify statuses in the previous release metadata cache.
typedef enum { RI_UNKNOWN, RI_RETURNING, RI_PRODUCTION, RI_ENDED, RI_CANCELLED } ReleaseStatus;
typedef struct {
  char imdb[24];
  int status, season, episode;
  char next_date[12], last_date[12];
  long long updated;
} ReleaseRecord;
static ReleaseRecord cache[RELEASE_CACHE_MAX];
static int count, loaded_profile = -1;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int ehIso(const char *s) {
  int i;
  if (!s || strlen(s) < 10) return 0;
  for (i = 0; i < 10; i++) {
    if (i == 4 || i == 7) { if (s[i] != '-') return 0; }
    else if (s[i] < '0' || s[i] > '9') return 0;
  }
  return 1;
}

static long diasCivis(int a, int m, int d) {
  long y = a;
  long era, yoe, doy, doe;
  y -= m <= 2;
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = y - era * 400;                                    // [0, 399]
  doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;   // [0, 365]
  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0, 146096]
  return era * 146097 + doe - 719468;
}

static long diasDeIso(const char *s) {
  int a, m, d;
  if (!ehIso(s)) return 0;
  a = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
  m = (s[5]-'0')*10 + (s[6]-'0');
  d = (s[8]-'0')*10 + (s[9]-'0');
  if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return diasCivis(a, m, d);
}

static int prefixoIgual(const char *s, const char *p) {
  size_t i;
  for (i = 0; p[i]; i++) {
    char a = s[i], b = p[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return 0;
  }
  return 1;
}

static ReleaseStatus classify_status(const char *cru) {
  if (!cru || !cru[0]) return RI_UNKNOWN;
  // As duas grafias: o TMDB escreve "Returning Series" e "Canceled"; o Trakt
  // escreve tudo em minusculas e usa "canceled" tambem. "Cancelled" com dois
  // eles aparece em corpos antigos do TMDB — as duas entram.
  if (prefixoIgual(cru, "returning")) return RI_RETURNING;
  if (prefixoIgual(cru, "continuing")) return RI_RETURNING;   // grafia do Trakt
  if (prefixoIgual(cru, "cancel"))    return RI_CANCELLED;
  if (prefixoIgual(cru, "ended"))     return RI_ENDED;
  if (prefixoIgual(cru, "in produc")) return RI_PRODUCTION;
  if (prefixoIgual(cru, "planned"))   return RI_PRODUCTION;
  if (prefixoIgual(cru, "upcoming"))  return RI_PRODUCTION;
  if (prefixoIgual(cru, "pilot"))     return RI_PRODUCTION;
  return RI_UNKNOWN;
}

static const char *campo(const char *p, char *dst, size_t tam) {
  size_t i = 0;
  if (!tam) return NULL;
  if (!p) { dst[0] = 0; return NULL; }
  while (p[i] && p[i] != '\t' && p[i] != '\n') i++;
  { size_t n = i < tam - 1 ? i : tam - 1;
    memcpy(dst, p, n); dst[n] = 0; }
  if (p[i] == '\t') return p + i + 1;
  return NULL;
}

static void serieDe(char *dst, size_t tam, const char *imdb) {
  size_t i = 0;
  if (!tam) return;
  if (!imdb) { dst[0] = 0; return; }
  while (imdb[i] && imdb[i] != ':' && i + 1 < tam) { dst[i] = imdb[i]; i++; }
  dst[i] = 0;
}

static int days_from(const char *iso, const char *today) {
  long a = diasDeIso(iso), b = diasDeIso(today);
  return (!a || !b) ? RELEASE_NO_DATE : (int)(a - b);
}

static void date_label(const char *iso, const char *today, char *dst, size_t tam) {
  int d;
  if (!dst || !tam) return;
  dst[0] = 0;
  d = days_from(iso, today);
  if (d == RELEASE_NO_DATE) return;
  // Perto = palavra; longe = a data por extenso. O corte em 7 dias e o mesmo
  // do painel de episodios do web: dentro da semana o dia da semana nao ajuda
  // numa TV (ninguem confere o calendario), a CONTAGEM ajuda.
  if (d == 0)      snprintf(dst, tam, "%s", i18n("hoje"));
  else if (d == 1) snprintf(dst, tam, "%s", i18n("amanhã"));
  else if (d > 1 && d <= 7) snprintf(dst, tam, i18n("em %d dias"), d);
  else if (d == -1) snprintf(dst, tam, "%s", i18n("ontem"));
  // Data por extenso — a MESMA funcao do resto do app (descoberta.c), que ja
  // traduz o mes e a ordem das palavras. Nao ha segundo formatador de data
  // aqui de proposito: dois divergem, e divergem em silencio.
  else desc_data_extenso(iso, dst, tam);
}

static void filename(char *dst, size_t size, const char *base, int profile) {
  if (profile <= 0) snprintf(dst, size, "%s.txt", base);
  else snprintf(dst, size, "%s-p%d.txt", base, profile);
}

static int save_cache(void) {
  char name[64], *body;
  size_t size = (size_t)count * 120 + 1, used = 0;
  int i, ok;
  body = malloc(size);
  if (!body) return 0;
  body[0] = 0;
  for (i = 0; i < count; i++) {
    const ReleaseRecord *r = &cache[i];
    used += (size_t)snprintf(body + used, size - used, "%s\t%d\t%d\t%d\t%s\t%s\t%lld\n",
                            r->imdb, r->status, r->season, r->episode,
                            r->next_date, r->last_date, r->updated);
  }
  filename(name, sizeof name, "release-info", loaded_profile);
  ok = data_write_light(name, body);
  free(body);
  return ok;
}

// Read only the metadata fields from the old cache during migration.
// No calendar, reminders, episode history or background fetches remain.
static void load_cache(int profile) {
  char name[64], legacy_name[64], *body;
  const char *p;
  int legacy = 0;
  filename(name, sizeof name, "release-info", profile);
  body = data_read(name);
  if (!body) {
    filename(legacy_name, sizeof legacy_name, "agenda", profile);
    body = data_read(legacy_name);
    legacy = body != NULL;
  }
  count = 0;
  if (!body) return;
  for (p = body; *p && count < RELEASE_CACHE_MAX;) {
    ReleaseRecord r = {0};
    char number[24], skip[512];
    const char *q = campo(p, r.imdb, sizeof r.imdb);
    if (legacy) { q = campo(q, skip, sizeof skip); q = campo(q, skip, sizeof skip); }
    q = campo(q, number, sizeof number); r.status = atoi(number);
    q = campo(q, number, sizeof number); r.season = atoi(number);
    q = campo(q, number, sizeof number); r.episode = atoi(number);
    if (legacy) q = campo(q, skip, sizeof skip);
    q = campo(q, r.next_date, sizeof r.next_date);
    q = campo(q, r.last_date, sizeof r.last_date);
    campo(q, number, sizeof number); r.updated = atoll(number);
    if (!ehIso(r.next_date)) r.next_date[0] = 0;
    if (!ehIso(r.last_date)) r.last_date[0] = 0;
    if (r.imdb[0]) cache[count++] = r;
    while (*p && *p != '\n') p++;
    if (*p) p++;
  }
  free(body);
  if (legacy && save_cache()) data_delete(legacy_name);
}

static void load_profile_locked(void) {
  int profile = perfis_ativo();
  if (profile == loaded_profile) return;
  loaded_profile = profile;
  load_cache(profile);
}

void releaseinfo_init(void) {
  pthread_mutex_lock(&mutex);
  load_profile_locked();
  pthread_mutex_unlock(&mutex);
}

static int record_index(const char *id) {
  int i;
  for (i = 0; i < count; i++) if (!strcmp(cache[i].imdb, id)) return i;
  return -1;
}

void releaseinfo_record(const char *imdb, const char *status, int season, int episode,
                        const char *next_date, const char *last_date) {
  char id[24];
  ReleaseRecord *r;
  int index, i;
  serieDe(id, sizeof id, imdb);
  if (!id[0]) return;
  pthread_mutex_lock(&mutex);
  load_profile_locked();
  index = record_index(id);
  if (index < 0) {
    if (count < RELEASE_CACHE_MAX) index = count++;
    else {
      index = 0;
      for (i = 1; i < count; i++) if (cache[i].updated < cache[index].updated) index = i;
    }
    memset(&cache[index], 0, sizeof cache[index]);
    snprintf(cache[index].imdb, sizeof cache[index].imdb, "%s", id);
  }
  r = &cache[index];
  if (status && status[0]) r->status = classify_status(status);
  if (ehIso(next_date)) {
    snprintf(r->next_date, sizeof r->next_date, "%.10s", next_date);
    r->season = season; r->episode = episode;
  } else if (status && status[0]) {
    r->next_date[0] = 0; r->season = r->episode = 0;
  }
  if (ehIso(last_date)) snprintf(r->last_date, sizeof r->last_date, "%.10s", last_date);
  r->updated = (long long)time(NULL);
  save_cache();
  pthread_mutex_unlock(&mutex);
}

static int format_record(const ReleaseRecord *r, const char *today, char *dst, size_t tam) {
  char quando[64];
  if (!dst || !tam) return 0;
  dst[0] = 0;
  if (!r) return 0;

  if (r->next_date[0]) {
    int d = days_from(r->next_date, today);
    date_label(r->next_date, today, quando, sizeof quando);
    if (d == RELEASE_NO_DATE || !quando[0]) return 0;
    if (r->season > 0 && r->episode > 0) {
      // O T/E tambem e traduzido: em ingles a abreviacao e S/E. Mesma regra do
      // rotulo "Retomar T2E3" do botao primario (detail.c).
      if (d < 0) snprintf(dst, tam, i18n("T%dE%d foi ao ar em %s"),
                          r->season, r->episode, quando);
      else snprintf(dst, tam, i18n("Próximo episódio T%dE%d · %s"),
                    r->season, r->episode, quando);
    } else {
      snprintf(dst, tam, i18n("Próximo episódio · %s"), quando);
    }
    return 1;
  }

  // Sem data. So ha o que dizer quando a SITUACAO e conhecida — e ai a frase e
  // sobre a serie ter acabado, nunca uma data inventada.
  switch (r->status) {
    case RI_ENDED:
      if (r->last_date[0]) {
        date_label(r->last_date, today, quando, sizeof quando);
        snprintf(dst, tam, i18n("Série encerrada · último episódio em %s"), quando);
      } else snprintf(dst, tam, "%s", i18n("Série encerrada"));
      return 1;
    case RI_CANCELLED:
      if (r->last_date[0]) {
        date_label(r->last_date, today, quando, sizeof quando);
        snprintf(dst, tam, i18n("Série cancelada · último episódio em %s"), quando);
      } else snprintf(dst, tam, "%s", i18n("Série cancelada"));
      return 1;
    case RI_RETURNING:
      snprintf(dst, tam, "%s", i18n("Sem data anunciada para o próximo episódio"));
      return 1;
    case RI_PRODUCTION:
      snprintf(dst, tam, "%s", i18n("Em produção · sem data anunciada"));
      return 1;
    default:
      return 0;
  }
}

int releaseinfo_phrase(const char *imdb, char *dst, size_t size) {
  ReleaseRecord record;
  char id[24], today[16];
  int index;
  time_t now = time(NULL);
  struct tm local;
  if (!dst || !size) return 0;
  dst[0] = 0;
  serieDe(id, sizeof id, imdb);
  if (!id[0] || !localtime_r(&now, &local)) return 0;
  if (!strftime(today, sizeof today, "%Y-%m-%d", &local)) return 0;
  pthread_mutex_lock(&mutex);
  load_profile_locked();
  index = record_index(id);
  if (index >= 0) record = cache[index];
  pthread_mutex_unlock(&mutex);
  return index >= 0 ? format_record(&record, today, dst, size) : 0;
}

void releaseinfo_forget(void) {
  char name[64];
  int profile;
  pthread_mutex_lock(&mutex);
  count = 0; loaded_profile = -1;
  for (profile = 0; profile <= ACCOUNT_PROFILE_MAX; profile++) {
    filename(name, sizeof name, "release-info", profile); data_delete(name);
    filename(name, sizeof name, "agenda", profile); data_delete(name);
  }
  pthread_mutex_unlock(&mutex);
}
