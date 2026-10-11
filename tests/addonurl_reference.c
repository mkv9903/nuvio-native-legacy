/* Compile the unchanged C fallback as the behavioral oracle, even when the
 * harness itself uses NV_RUST_CORE. Keep it in a separate translation unit. */
#undef NV_RUST_CORE
#include "addonurl.h"

void reference_addon_base(const char *input, char *output, size_t capacity) {
  nv_addon_base(input, output, capacity);
}

int reference_addon_id(char *output, size_t capacity, const char *input) {
  return nv_addon_id(output, capacity, input);
}

int reference_addon_request(char *output, size_t capacity, const char *base, const char *path) {
  return nv_addon_url(output, capacity, base, "%s", path);
}

int reference_addon_formatted(char *output, size_t capacity, const char *base,
                             const char *id, int separator, int episode) {
  return nv_addon_url(output, capacity, base, "/stream/%s%c%d.json", id, separator, episode);
}
