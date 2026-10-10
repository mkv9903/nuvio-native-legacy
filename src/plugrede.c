// Ver plugrede.h.
#include "plugrede.h"
#include <stdlib.h>
#include <string.h>


unsigned plugrede_capacidades(void) {
  return rede_pedido_capacidades() ? (PR_CAP_REDE | PR_CAP_PRAZO | PR_CAP_ABORTAR |
                                      PR_CAP_REDIRECT | PR_CAP_CAB_LIVRES) : 0;
}
int plugrede_pedir(const RedePedido *q, RedeResposta *r) { return rede_pedir(q, r); }
