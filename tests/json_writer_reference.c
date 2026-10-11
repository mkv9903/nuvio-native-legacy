// The unchanged C writer is an independent byte/state/allocation oracle.
#undef NV_RUST_CORE
#define jsw_iniciar reference_writer_init
#define jsw_livre reference_writer_free
#define jsw_texto_final reference_writer_finish
#define jsw_obj_ini reference_writer_object_open
#define jsw_obj_fim reference_writer_object_close
#define jsw_arr_ini reference_writer_array_open
#define jsw_arr_fim reference_writer_array_close
#define jsw_chave reference_writer_key
#define jsw_str reference_writer_string
#define jsw_int reference_writer_integer
#define jsw_bool reference_writer_boolean
#define jsw_nulo reference_writer_null
#define jsw_bruto reference_writer_raw
#define jsw_cs reference_writer_key_string
#define jsw_ci reference_writer_key_integer
#define jsw_cb reference_writer_key_boolean
#include "../src/jsw.c"
