// Retained title release text: dates, persistence, legacy migration and profiles.
#include "../src/releaseinfo.c"
#include "ajustes.h"
#include <assert.h>

static void restart(void) {
  count = 0;
  loaded_profile = -1;
  releaseinfo_init();
}

static void phrase(const ReleaseRecord *r, const char *today, const char *expected) {
  char actual[256];
  assert(format_record(r, today, actual, sizeof actual));
  if (strcmp(actual, expected)) {
    fprintf(stderr, "release text: [%s], expected [%s]\n", actual, expected);
    abort();
  }
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char actual[256], expected[256], date[80], *body;
  ReleaseRecord r = { .status = RI_RETURNING, .season = 2, .episode = 5,
                      .next_date = "2026-09-16" };
  // Refuse to write until the isolated test directory has been verified.
  assert(dir && strstr(dir, "nuvio-releaseinfo-"));
  data_init(dir);
  assert(!strcmp(data_dir(), dir));
  ajustes_dir(dir);
  assert(perfis_ativo() == 1);

  assert(days_from("2027-01-01", "2026-12-31") == 1);
  assert(days_from("2028-03-01", "2028-02-28") == 2);
  assert(days_from("2026-03-01", "2026-02-28") == 1);
  assert(days_from("", "2026-09-16") == RELEASE_NO_DATE);
  snprintf(expected, sizeof expected, i18n("Próximo episódio T%dE%d · %s"), 2, 5, i18n("hoje"));
  phrase(&r, "2026-09-16", expected);
  snprintf(expected, sizeof expected, i18n("Próximo episódio T%dE%d · %s"), 2, 5, i18n("amanhã"));
  phrase(&r, "2026-09-15", expected);
  snprintf(expected, sizeof expected, i18n("T%dE%d foi ao ar em %s"), 2, 5, i18n("ontem"));
  phrase(&r, "2026-09-17", expected);
  snprintf(date, sizeof date, i18n("em %d dias"), 3);
  snprintf(expected, sizeof expected, i18n("Próximo episódio T%dE%d · %s"), 2, 5, date);
  phrase(&r, "2026-09-13", expected);
  desc_data_extenso(r.next_date, date, sizeof date);
  snprintf(expected, sizeof expected, i18n("Próximo episódio T%dE%d · %s"), 2, 5, date);
  phrase(&r, "2026-08-01", expected);

  r.next_date[0] = 0;
  phrase(&r, "2026-09-16", i18n("Sem data anunciada para o próximo episódio"));
  r.status = RI_PRODUCTION;
  phrase(&r, "2026-09-16", i18n("Em produção · sem data anunciada"));
  r.status = RI_CANCELLED;
  phrase(&r, "2026-09-16", i18n("Série cancelada"));
  r.status = RI_ENDED;
  phrase(&r, "2026-09-16", i18n("Série encerrada"));
  strcpy(r.last_date, "2026-09-15");
  snprintf(expected, sizeof expected, i18n("Série encerrada · último episódio em %s"), i18n("ontem"));
  phrase(&r, "2026-09-16", expected);
  r.status = RI_UNKNOWN;
  assert(!format_record(&r, "2026-09-16", actual, sizeof actual) && !actual[0]);

  // A previous calendar cache still supplies title text after migration.
  assert(data_write("agenda-p1.txt",
    "tt10986410\tTed Lasso\tposter.jpg\t1\t2\t5\tEpisode title\t2026-09-16\t2026-09-09\t123\tSynopsis\tstandard\tNetwork\t30\t2\tComedy\tart.jpg\n"));
  releaseinfo_init();
  assert(count == 1 && cache[0].season == 2 && cache[0].episode == 5);
  assert(!strcmp(cache[0].next_date, "2026-09-16"));
  assert(!data_read("agenda-p1.txt"));
  body = data_read("release-info-p1.txt");
  assert(body && strstr(body, "tt10986410\t1\t2\t5\t2026-09-16\t2026-09-09\t123\n"));
  assert(!strstr(body, "Synopsis") && !strstr(body, "poster.jpg"));
  free(body);
  restart();
  assert(releaseinfo_phrase("tt10986410:2:5", actual, sizeof actual));
  assert(!releaseinfo_phrase("tt0000000", actual, sizeof actual) && !actual[0]);

  // A partial enrichment cannot erase a known date; a complete no-date result can.
  releaseinfo_record("tt10986410", NULL, 0, 0, NULL, NULL);
  assert(!strcmp(cache[0].next_date, "2026-09-16"));
  releaseinfo_record("tt10986410", "Ended", 0, 0, "", "2026-09-15");
  assert(!cache[0].next_date[0] && !cache[0].season && !cache[0].episode);
  assert(cache[0].status == RI_ENDED);
  restart();
  assert(cache[0].status == RI_ENDED && !cache[0].next_date[0]);

  perfis_definir_ativo(2);
  assert(!releaseinfo_phrase("tt10986410", actual, sizeof actual));
  releaseinfo_record("tt1520211", "Canceled", 0, 0, "", "");
  assert(releaseinfo_phrase("tt1520211", actual, sizeof actual));
  assert(!strcmp(actual, i18n("Série cancelada")));
  perfis_definir_ativo(1);
  assert(releaseinfo_phrase("tt10986410", actual, sizeof actual));
  assert(!releaseinfo_phrase("tt1520211", actual, sizeof actual));

  releaseinfo_forget();
  assert(!data_read("release-info-p1.txt") && !data_read("release-info-p2.txt"));
  assert(!releaseinfo_phrase("tt10986410", actual, sizeof actual));
  puts("releaseinfo: ok");
  return 0;
}
