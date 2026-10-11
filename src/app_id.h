#ifndef NV_APP_ID_H
#define NV_APP_ID_H
/* Package metadata and the runtime must be built with the same identity. */
#define NV_APP_ID_PRODUCTION "space.mkv.nuvio.native"
#ifndef NV_APP_ID
#define NV_APP_ID NV_APP_ID_PRODUCTION
#endif
#define NV_LS_MEDIA_CLIENT "com.webos.media.client." NV_APP_ID
#endif
