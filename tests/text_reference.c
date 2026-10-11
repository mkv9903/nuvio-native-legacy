// Compile the retained C algorithms under separate names, even in Rust tests.
#undef NV_RUST_CORE
#define nv_dobra_estilizada reference_fold
#define nv_limpar_texto reference_cleanup
#define nv_aparar_separadores reference_trim
#define busca_normalizar reference_normalize
#define busca_codepoints reference_count
#define busca_apagar_ultimo reference_delete
#include "../src/dobra.c"
#include "../src/limpa.c"
#include "../src/buscanorm.c"
