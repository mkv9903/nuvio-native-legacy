#include "app_id.h"
#include "data.h"
#include "log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  char expected[1024];
  char *content;
  assert(argc == 3);
  data_init(argv[1]);
#ifdef NV_RUST_CORE
  assert(strcmp(NV_APP_ID, "space.mkv.nuvio.rusttest") == 0);
  assert(strcmp(NV_APP_NAME, "Nuvio Rust Test") == 0);
  assert(strcmp(NV_DIAG_PREFIX, "/tmp/space.mkv.nuvio.rusttest") == 0);
  assert(strcmp(NV_LS_MEDIA_CLIENT,
                "com.webos.media.client.space.mkv.nuvio.rusttest") == 0);
  snprintf(expected, sizeof expected, "%s/%s", argv[1], NV_APP_ID);
  assert(strcmp(data_dir(), expected) == 0);
  assert(data_read("production-only.txt") == NULL);
  assert(data_write("settings.txt", "isolated test settings\n"));
  content = data_read("settings.txt");
  assert(content && strcmp(content, "isolated test settings\n") == 0);
  free(content);
#else
  assert(strcmp(NV_APP_ID, "space.mkv.nuvio.native") == 0);
  assert(strcmp(NV_DIAG_PREFIX, "/tmp/nuvio") == 0);
  assert(strcmp(data_dir(), argv[1]) == 0);
  content = data_read("settings.txt");
  assert(content && strcmp(content, "production sentinel\n") == 0);
  free(content);
#endif
  if (strcmp(argv[2], "terminal") == 0) assert(log_file_path() == NULL);
  else assert(log_file_path() && strcmp(log_file_path(), argv[2]) == 0);
  return 0;
}
