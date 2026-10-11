// Keep the unchanged C reader available as an independent compatibility oracle.
#undef NV_RUST_CORE
#define js_fim reference_json_end
#define js_tem reference_json_has
#define js_cadeia reference_json_string
#define js_texto reference_json_text
#define js_texto_linhas reference_json_lines
#define js_num reference_json_number
#define js_array reference_json_array
#define js_prox reference_json_next
#define js_raiz_array reference_json_root_array
#define js_bruto reference_json_raw
#define js_ms_iso reference_json_iso_ms
#define js_texto_raiz reference_json_root_text
#define js_texto_raiz_em reference_json_root_text_range
#include "../src/js.c"
