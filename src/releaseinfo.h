#ifndef NV_RELEASEINFO_H
#define NV_RELEASEINFO_H
#include <stddef.h>

// Per-profile release metadata for title details and information popups.
// Records come from normal title enrichment; this module makes no network requests.
void releaseinfo_init(void);
void releaseinfo_record(const char *imdb, const char *status, int season, int episode,
                        const char *next_date, const char *last_date);
int releaseinfo_phrase(const char *imdb, char *dst, size_t size);
void releaseinfo_forget(void);

#endif
