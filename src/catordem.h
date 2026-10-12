/* Compatibility names for unmigrated C row consumers. Implementation is Rust. */
#ifndef NV_CATORDEM_H
#define NV_CATORDEM_H
/* Retained C renderer allocation sizes, not Rust domain limits. */
#define CATORD_MAX 2048
#define CATORD_CHAVE 192
int catordem_ler(const char *body);
unsigned catordem_revisao(void);
int catordem_tem_ordem(void);
int catordem_n(void);
/* Thread-local owned text, valid until this thread requests another key. */
const char *catordem_chave(int index);
int catordem_oculta(const char *key, const char *provider_key);
/* Remote known rows first, then all unknown local rows in their original order. */
int catordem_unir(const char *const *local, int count, int *output, int capacity);
int catordem_tem_ocultar_nao_lancados(void);
int catordem_ocultar_nao_lancados(void);
int catordem_tem_ocultar_sublinhado(void);
int catordem_ocultar_sublinhado(void);
void catordem_esquecer(void);
#endif
