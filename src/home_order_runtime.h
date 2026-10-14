#ifndef NV_HOME_ORDER_RUNTIME_H
#define NV_HOME_ORDER_RUNTIME_H
#include "rust_account.h"
void home_order_runtime_init(RustAccount *account, const char *directory);
void home_order_runtime_step(int allow_periodic);
void home_order_runtime_refresh(void);
void home_order_runtime_logout(void);
#endif
