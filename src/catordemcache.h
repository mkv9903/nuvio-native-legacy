#ifndef NV_CATORD_CACHE_H
#define NV_CATORD_CACHE_H
/* Fresh versioned Rust JSON, scoped to this test app/account/profile. */
int catordem_cache_carregar(int profile, const char *account);
int catordem_cache_gravar(int profile, const char *account, const char *body);
void catordem_cache_esquecer(void);
#endif
