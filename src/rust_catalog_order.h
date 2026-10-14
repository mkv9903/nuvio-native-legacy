#ifndef NV_RUST_CATALOG_ORDER_H
#define NV_RUST_CATALOG_ORDER_H
#include <stddef.h>
#include <stdint.h>
#include "rust_account.h"
typedef struct RustHomeOrderController RustHomeOrderController;
RustHomeOrderController *nuvio_home_order_create(const RustAccount *account, const char *directory);
void nuvio_home_order_destroy(RustHomeOrderController *controller);
void nuvio_home_order_step(const RustHomeOrderController *controller, int allow_periodic);
void nuvio_home_order_refresh(const RustHomeOrderController *controller);
void nuvio_home_order_logout(const RustHomeOrderController *controller);
typedef struct RustHomeOrderSnapshot RustHomeOrderSnapshot;
RustHomeOrderSnapshot *nuvio_home_order_snapshot(void);
uint64_t nuvio_home_order_snapshot_revision(const RustHomeOrderSnapshot *snapshot);
const char *nuvio_home_order_snapshot_key(const RustHomeOrderSnapshot *snapshot, size_t index);
void nuvio_home_order_snapshot_destroy(RustHomeOrderSnapshot *snapshot);
int nuvio_home_order_receive(const char *body);
uint64_t nuvio_home_order_revision(void);
size_t nuvio_home_order_count(void);
int nuvio_home_order_configured(void);
/* Owned text; release using nuvio_text_destroy. */
char *nuvio_home_order_key(size_t index);
int nuvio_home_order_hidden(const char *key, const char *provider_key);
size_t nuvio_home_order_union(const char *const *local, size_t count, int *output, size_t capacity);
/* -1: absent; 0: false; 1: true. */
int nuvio_home_order_preference(int underline);
void nuvio_home_order_reset(void);
#endif
