/* The retained source fixtures use real in-memory latency statistics. Their
 * C statistics file persistence is outside the cases under test. */
#include <stddef.h>
char *dados_ler(const char *name) { (void)name; return NULL; }
int dados_gravar_leve(const char *name, const char *body) { (void)name; (void)body; return 1; }
