#ifndef NUVIO_TEST_ADDON_CONTEXT_H
#define NUVIO_TEST_ADDON_CONTEXT_H
/* Retained C algorithm fixtures configure the real Rust addon capability. */
void test_addons_set(const char *json);
void test_addons_release(void);
#endif
