// "ENDS AT HH:MM" OF PLAYER AND PAUSE SCREEN (issue #213).
//
// The report was "Ends at 1", always "1", with top clock being correct.
// strftime was correct; the BUFFER was not: player formatted sentence in end[32], and
// Russian translation "Заканчивается в %s" already occupies 30 bytes in UTF-8 (Cyrillic is
// 2 bytes per letter). Leaving 1 byte for time and NUL: "Заканчивается в 1" —
// and "1" is the first digit of any hour between 10:00 and 19:59. Greek
// ("Τελειώνει στις %s") truncated identically. Pause used end[40] and barely passed.
// tests/relogiofim.c sweeps all 30 languages.
//
// Time is 24 h ("%H:%M"), like top clock: defect was truncation, not format.
// CLOCK_END_MAX also accommodates a 12 h time ("11:59 PM") in the longest translation,
// if format ever changes.
#ifndef NV_CLOCKEND_H
#define NV_CLOCKEND_H

#include "idioma.h"
#include <stdio.h>
#include <time.h>

#define CLOCK_END_MAX 128

// `end_buf` with CLOCK_END_MAX bytes. `remaining` in seconds (negative counts as 0).
static inline void clock_end(char *end_buf, size_t size, time_t now, double remaining) {
  time_t t2 = now + (time_t)(remaining > 0.0 ? remaining : 0.0);
  struct tm lf;
  char h2[16];
  localtime_r(&t2, &lf);
  strftime(h2, sizeof h2, "%H:%M", &lf);
  // i18n IN FORMAT: assembled sentence does not match key (issue #12).
  // Accented "à" is written as \xc3\xa0, with literal split in two.
  snprintf(end_buf, size, i18n("Termina \xc3\xa0" "s %s"), h2);
}

// SAME PHRASE IN PLAYER ISLAND PILL (plrilha.c): "termina às 22:41",
// lowercase after time, as in Glass UI mockup. Same buffer.
static inline void clock_end_island(char *end_buf, size_t size, time_t now, double remaining) {
  time_t t2 = now + (time_t)(remaining > 0.0 ? remaining : 0.0);
  struct tm lf;
  char h2[16];
  localtime_r(&t2, &lf);
  strftime(h2, sizeof h2, "%H:%M", &lf);
  snprintf(end_buf, size, i18n("termina \xc3\xa0" "s %s"), h2);
}

// Backward-compatibility shims
#define RELOGIO_FIM_MAX CLOCK_END_MAX
static inline void relogio_fim(char *fim, size_t tam, time_t agora, double falta) {
  clock_end(fim, tam, agora, falta);
}
static inline void relogio_fim_ilha(char *fim, size_t tam, time_t agora, double falta) {
  clock_end_island(fim, tam, agora, falta);
}

#endif
