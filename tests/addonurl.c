// Add-on URL integration regressions (#201, #202).
// A 1500-byte configured URL must survive installation, export, account/file
// reload and stream-request construction. Oversized URLs must be rejected,
// with no truncated request or configured URL in the log. Query configuration
// and encoded IDs must reach the request unchanged. Neighboring services are
// test doubles; the shell runner checks rejection messages and URL privacy.
// Run with either the C reference or the real Rust helpers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "addons.h"
#include "addonstats.h"
#include "fontecache.h"

// Service doubles retain the existing C API names during migration.
static char requested_url[8192];      // Last URL handed to the network double.
static int  request_count, oversized_request_count;
// Keep URL checks independent of latency history and statistics persistence.
void addonstats_registrar(const char *name, unsigned ms, int responded) {
  (void)name; (void)ms; (void)responded;
}
int addonstats_segunda_s(const char *name, int default_seconds) {
  (void)name; return default_seconds;
}
void addonstats_salvar(void) {}
const char *rede_ultimo_erro(void) { return ""; }
char *rede_baixar(const char *url, int s) {
  (void)s;
  request_count++;
  if (strstr(url, "large.test")) oversized_request_count++;
  snprintf(requested_url, sizeof requested_url, "%s", url);
  return strdup("{\"streams\":[{\"url\":\"https://x/a.mp4\"}]}");
}
void ondever_pedir(const char *id, int series, long tmdb) { (void)id; (void)series; (void)tmdb; }
int stream_extrair(const char *json, const char *provider, Stream **output) {
  int n = 0; const char *p = json;
  (void)provider;
  while ((p = strstr(p, "\"url\"")) != NULL) { n++; p += 5; }
  *output = n ? calloc((size_t)n, sizeof(Stream)) : NULL;
  return n;
}
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
void stream_definir_lista(const Stream *l, int n) { (void)l; (void)n; }
void stream_definir_lista_idade(const Stream *l, int n, Uint32 age) {
  (void)age; stream_definir_lista(l, n); }
void stream_lista_acrescentar(const Stream *l, int n, int o) { (void)l; (void)n; (void)o; }
void stream_invalidar(const char *p) { (void)p; }
int stream_n(void) { return 0; }
int stream_lista_do_alvo(const char *id) { (void)id; return 0; }
Uint32 SDL_GetTicks(void) { return 1000; }
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
void debrid_definir_episodio(int t, int e) { (void)t; (void)e; }
void debrid_nova_busca(void) {}
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int month, const char *default_name) { (void)month; return default_name; }
// Return only the host: configured paths may carry provider credentials.
const char *rede_url_publica(const char *url, char *dst, unsigned capacity) {
  const char *p = url ? strstr(url, "://") : NULL;
  p = p ? p + 3 : (url ? url : "");
  snprintf(dst, capacity, "%.*s", (int)strcspn(p, "/"), p); return dst; }
void marco(const char *s) { (void)s; }
int  fontecache_pegar(const char *id, const char *type, Stream **l, int *n) {
  (void)id; (void)type; *l = NULL; *n = 0; return FC_NADA; }
void fontecache_guardar(const char *id, const char *type, const Stream *l, int n) {
  (void)id; (void)type; (void)l; (void)n; }
void fontecache_ceder(void) {}
void fontecache_avancar(void) {}
unsigned fontecache_vod_geracao(void) { return 0; }
void fontecache_vod_limpar(void) {}
void fontecache_vod_apagar(const char *id, const char *type, const char *origin,
                          const FontecacheEscopo *scope) {
  (void)id; (void)type; (void)origin; (void)scope;
}
void fontecache_vod_guardar(const char *id, const char *type, const char *origin,
                           const FontecacheEscopo *scope,
                           const Stream *l, int n, Uint32 when) {
  (void)id; (void)type; (void)origin; (void)scope; (void)l; (void)n; (void)when;
}
int fontecache_vod_pegar(const char *id, const char *type, const char *origin,
                        const FontecacheEscopo *scope,
                        Stream **l, int *n, Uint32 *age) {
  (void)id; (void)type; (void)origin; (void)scope;
  *l = NULL; *n = 0; *age = 0; return FC_NADA;
}

// Integration scenarios.
static int failures;
static void check_value(const char *description, long actual, long expected) {
  if (actual == expected) return;
  printf("FAIL %s: got %ld, expected %ld\n", description, actual, expected);
  failures++;
}
// Report lengths and the first differing offset without printing either URL.
static void check_url(const char *description, const char *actual, const char *expected) {
  size_t i = 0;
  if (!strcmp(actual, expected)) return;
  while (actual[i] && actual[i] == expected[i]) i++;
  printf("FAIL %s: %lu bytes (expected %lu), first difference at %lu\n", description,
         (unsigned long)strlen(actual), (unsigned long)strlen(expected), (unsigned long)i);
  failures++;
}

// Synthetic configuration with nonuniform padding detects truncation or shifts.
// SECRET lets the shell runner detect an accidental configured-URL log leak.
static void make_url(char *dst, size_t total, const char *host) {
  size_t k = (size_t)snprintf(dst, total + 1, "https://%s/SECRET", host), i = 0;
  while (k < total - 14) dst[k++] = (char)('a' + (int)((i++ * 7) % 26));
  snprintf(dst + k, 15, "/manifest.json");
}

static void request_streams(const char *id) {
  requested_url[0] = 0;
  addons_buscar(id, "movie");
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
}

int main(void) {
  static char long_url[1501], oversized_url[2101], base[1501], expected[1700];
  static AddonRemoto account_list[2];
  char dir[] = "/tmp/nuvio-addonurl-XXXXXX";
  char file_path[600];
  FILE *f;

  make_url(long_url, 1500, "long.test");
  make_url(oversized_url, 2100, "large.test");
  snprintf(base, sizeof base, "%.*s", 1500 - 14, long_url);   // Without /manifest.json.
  check_value("long URL has 1500 bytes", (long)strlen(long_url), 1500);
  check_value("oversized URL exceeds the limit", strlen(oversized_url) >= NV_ADDON_URL_MAX, 1);

  // Installation.
  check_value("long configured URL installs", addons_adicionar("Long", long_url), 1);
  check_value("one add-on in the list", addons_n(), 1);
  check_url("stored base", addons_base(0), base);

  // Export the same base used by disk/account synchronization.
  check_value("one exported add-on", addons_exportar(account_list, 2), 1);
  check_url("exported base", account_list[0].url, base);

  // Reload from the account list.
  addons_esquecer();
  check_value("list cleared", addons_n(), 0);
  check_value("account list applied", addons_definir_lista(account_list, 1), 1);
  check_url("base reloaded from account", addons_base(0), base);
  // Reapplying an identical complete base must not change the list.
  check_value("identical list is not reapplied", addons_definir_lista(account_list, 1), 0);

  // Construct a real service request through the mocked network.
  snprintf(expected, sizeof expected, "%s/stream/movie/tt0111161.json", base);
  request_streams("tt0111161");
  check_value("request completed with a source", addons_estado(), ADD_PRONTO);
  check_url("stream request", requested_url, expected);

  // Disk reload includes a valid 1500-byte URL and one oversized entry.
  if (!mkdtemp(dir)) { printf("FAIL: cannot create temporary directory\n"); return 1; }
  snprintf(file_path, sizeof file_path, "%s/addons.txt", dir);
  f = fopen(file_path, "w");
  if (!f) { printf("FAIL: cannot create temporary file\n"); return 1; }
  fprintf(f, "Large\t%s\t1\t1\t0\n", oversized_url);
  fprintf(f, "Long\t%s\t1\t1\t0\n", long_url);
  fprintf(f, "Short\thttps://short.test/manifest.json\n");
  fclose(f);
  check_value("disk reload retains two valid entries", addons_carregar(dir), 2);
  check_url("base reloaded from disk", addons_base(0), base);
  check_url("entry after the oversized line is preserved", addons_base(1), "https://short.test");
  unlink(file_path); rmdir(dir);

  // Oversized configurations are rejected, never requested in truncated form.
  check_value("oversized URL does not install", addons_adicionar("Large", oversized_url), 0);
  check_value("list still contains two entries", addons_n(), 2);
  request_count = oversized_request_count = 0;
  request_streams("tt0068646");
  check_value("both valid add-ons were requested", request_count, 2);
  check_value("no request used the oversized URL", oversized_request_count, 0);

  // Query preservation and path-ID encoding regressions (#202).
  { char u[256];
    int w;
    // Base normalization retains existing canonicalization behavior.
    nv_addon_base("  https://x.test/a/manifest.json?  ", u, sizeof u);
    check_url("outer whitespace, manifest and empty query removed", u, "https://x.test/a");
    nv_addon_base("stremio://s.test/abc/Manifest.json", u, sizeof u);
    check_url("scheme conversion and case-insensitive manifest removal", u, "https://s.test/abc");
    nv_addon_base("https://q.test/cfg/manifest.json?ver=3&k=a%2Fb", u, sizeof u);
    check_url("base query preserved", u, "https://q.test/cfg?ver=3&k=a%2Fb");
    nv_addon_base("https://q.test/cfg//?x=1", u, sizeof u);
    check_url("trailing path slashes before query removed", u, "https://q.test/cfg?x=1");
    nv_addon_base("https://q.test/cfg/manifest.json?very-long", u, 20);
    check_url("oversized normalized base is empty", u, "");
    // Resource path must precede the base query.
    w = nv_addon_url(u, sizeof u, "https://q.test/cfg?ver=3", "/stream/%s/%s.json", "movie", "tt1");
    check_url("resource path before query", u, "https://q.test/cfg/stream/movie/tt1.json?ver=3");
    check_value("return value equals required length", w, (long)strlen(u));
    w = nv_addon_url(u, sizeof u, "https://q.test/cfg", "/manifest.json");
    check_url("request without query unchanged", u, "https://q.test/cfg/manifest.json");
    w = nv_addon_url(u, 30, "https://q.test/cfg?ver=3", "/stream/%s/%s.json", "movie", "tt1");
    check_value("oversized request reports required length", w >= 30, 1);
    check_url("oversized request is empty", u, "");
    // Encode bytes outside pchar; preserve colons and ordinary IDs.
    nv_addon_id(u, sizeof u, "tt0111161:1:2");
    check_url("episode ID unchanged", u, "tt0111161:1:2");
    nv_addon_id(u, sizeof u, "iptv:Canal Um/HD#1?a%\xc3\xa7");
    check_url("ID with space, slash, fragment/query markers, percent and UTF-8", u, "iptv:Canal%20Um%2FHD%231%3Fa%25%C3%A7");
    check_value("oversized encoded ID rejected", nv_addon_id(u, 6, "abc def"), 0); }

  // Check the same rules through the actual C add-on service.
  addons_esquecer();
  check_value("add-on with query installs", addons_adicionar("Query", "https://q.test/cfg/manifest.json?ver=3"), 1);
  check_url("stored base includes query", addons_base(0), "https://q.test/cfg?ver=3");
  request_streams("tt0111161");
  check_url("service request places path before query", requested_url, "https://q.test/cfg/stream/movie/tt0111161.json?ver=3");
  check_value("manifest suffix does not create a distinct add-on",
           addons_adicionar("Query", "https://q.test/cfg?ver=3"), 0);
  addons_esquecer();
  check_value("stremio-scheme add-on installs", addons_adicionar("Channel", "stremio://c.test/x/manifest.json"), 1);
  request_streams("iptv:Canal Um");
  check_url("service request uses encoded ID and HTTPS", requested_url, "https://c.test/x/stream/movie/iptv:Canal%20Um.json");

  printf(failures ? "addonurl: FAIL (%d)\n" : "addonurl: ok\n", failures);
  return failures ? 1 : 0;
}
