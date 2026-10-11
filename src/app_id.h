#ifndef NV_APP_ID_H
#define NV_APP_ID_H
/* Package metadata and the runtime must be built with the same identity. */
#define NV_APP_ID_PRODUCTION "space.mkv.nuvio.native"
#define NV_APP_ID_RUST_TEST "space.mkv.nuvio.rusttest"
#ifndef NV_APP_ID
#ifdef NV_RUST_CORE
#define NV_APP_ID NV_APP_ID_RUST_TEST
#else
#define NV_APP_ID NV_APP_ID_PRODUCTION
#endif
#endif
#ifdef NV_RUST_CORE
#define NV_APP_NAME "Nuvio Rust Test"
#define NV_DIAG_PREFIX "/tmp/" NV_APP_ID
#define NV_SCREENSHOT_TEMP NV_DIAG_PREFIX "-shot.tmp"
#else
#define NV_APP_NAME "Nuvio Native"
#define NV_DIAG_PREFIX "/tmp/nuvio"
#define NV_SCREENSHOT_TEMP "/tmp/.nuvio-shot.tmp"
#endif
#define NV_LS_MEDIA_CLIENT "com.webos.media.client." NV_APP_ID
#endif
