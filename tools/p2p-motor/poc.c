// PoC of embedded P2P engine (nuvio-engine + libtorrent) outside app.
//
//   poc <infoHash> [folder] [seconds] [cacheMB]
//
// Adds torrent via magnet (with public trackers), waits for metadata,
// selects largest video file, prepares stream and prints local URL
// (http://127.0.0.1:P/...) played with HTTP Range. Every 2 s prints
// peers, transfer rate, memory cache and disk cache. Exits after `seconds` (0 = never).
#include "nuvio_engine/nuvio_engine.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_flag;
static void handle_signal(int s) { (void)s; stop_flag = 1; }

static double now_sec(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static int is_video(const char *n) {
  const char *e = strrchr(n, '.');
  return e && (!strcasecmp(e, ".mp4") || !strcasecmp(e, ".mkv") ||
               !strcasecmp(e, ".webm") || !strcasecmp(e, ".avi") ||
               !strcasecmp(e, ".mov") || !strcasecmp(e, ".m4v"));
}

int main(int argc, char **argv) {
  const char *hash = argc > 1 ? argv[1] : "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c";
  const char *folder = argc > 2 ? argv[2] : "/tmp/nvp2p";
  int limit = argc > 3 ? atoi(argv[3]) : 0;
  int cacheMB = argc > 4 ? atoi(argv[4]) : 512;
  char data_dir[600], cache_dir[600], magnet[2000], tid[65] = "";
  nuvio_engine_config cfg;
  nuvio_engine *m = NULL;
  nuvio_engine_torrent_request req;
  nuvio_engine_event ev;
  uint64_t rid;
  double t0 = now_sec(), tMeta = 0, tUrl = 0, last_stat = 0;
  int st, prepared = 0;

  signal(SIGINT, handle_signal); signal(SIGTERM, handle_signal);
  snprintf(data_dir, sizeof data_dir, "%s/data", folder);
  snprintf(cache_dir, sizeof cache_dir, "%s/cache", folder);
  nuvio_engine_config_init(&cfg);
  cfg.data_directory = data_dir;
  cfg.cache_directory = cache_dir;
  cfg.memory_cache_capacity_bytes = 16ull << 20;     // TV: 16 MB memory cache
  cfg.disk_cache_capacity_bytes = (uint64_t)cacheMB << 20;
  cfg.upload_mode = NUVIO_ENGINE_UPLOAD_LIMITED;
  cfg.upload_limit_bytes_per_second = 256 * 1024;
  cfg.torrent_profile = NUVIO_ENGINE_TORRENT_PROFILE_BALANCED;
  st = nuvio_engine_create(&cfg, &m);
  if (st) { fprintf(stderr, "create: %s\n", nuvio_engine_status_message(st)); return 1; }
  fprintf(stderr, "[poc] engine %s / %s\n", nuvio_engine_version_string(),
          nuvio_engine_protocol_backend_version());

  snprintf(magnet, sizeof magnet,
           "magnet:?xt=urn:btih:%s"
           "&tr=udp%%3A%%2F%%2Ftracker.opentrackr.org%%3A1337%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Fopen.stealth.si%%3A80%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Ftracker.torrent.eu.org%%3A451%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Fexodus.desync.com%%3A6969%%2Fannounce"
           "&tr=udp%%3A%%2F%%2Ftracker.leechers-paradise.org%%3A6969", hash);
  nuvio_engine_torrent_request_init(&req);
  req.magnet_uri = magnet;
  req.source_type = NUVIO_ENGINE_TORRENT_SOURCE_MAGNET;
  st = nuvio_engine_add_torrent(m, &req, &rid);
  if (st) { fprintf(stderr, "add: %s\n", nuvio_engine_status_message(st)); return 1; }

  while (!stop_flag && (!limit || now_sec() - t0 < limit)) {
    nuvio_engine_event_init(&ev);
    while (nuvio_engine_poll_event(m, &ev) == NUVIO_ENGINE_STATUS_OK) {
      if (ev.type == NUVIO_ENGINE_EVENT_TORRENT_ADDED) {
        snprintf(tid, sizeof tid, "%s", ev.torrent_id);
        fprintf(stderr, "[poc] %.1fs added %s\n", now_sec() - t0, tid);
      } else if (ev.type == NUVIO_ENGINE_EVENT_TORRENT_METADATA_READY && !prepared) {
        size_t n = 0, i, best_idx = (size_t)-1;
        uint64_t max_size = 0;
        nuvio_engine_stream_request sr;
        tMeta = now_sec() - t0;
        snprintf(tid, sizeof tid, "%s", ev.torrent_id);
        nuvio_engine_get_file_count(m, tid, &n);
        for (i = 0; i < n; i++) {
          nuvio_engine_file f;
          nuvio_engine_file_init(&f);
          if (nuvio_engine_get_file(m, tid, i, &f)) continue;
          fprintf(stderr, "[poc]   file %zu %8.1f MB %s\n", i, f.size / 1048576.0, f.path);
          if (is_video(f.path) && f.size > max_size) { max_size = f.size; best_idx = i; }
        }
        fprintf(stderr, "[poc] %.1fs metadata, %zu files, chosen %zu\n", tMeta, n, best_idx);
        if (best_idx == (size_t)-1) return 2;
        nuvio_engine_stream_request_init(&sr);
        sr.torrent_id = tid;
        sr.file_index = (uint32_t)best_idx;
        st = nuvio_engine_prepare_stream(m, &sr, &rid);
        if (st) { fprintf(stderr, "prepare: %s\n", nuvio_engine_status_message(st)); return 1; }
        prepared = 1;
      } else if (ev.type == NUVIO_ENGINE_EVENT_STREAM_PREPARED) {
        tUrl = now_sec() - t0;
        fprintf(stderr, "[poc] %.1fs stream ready (%.1f MB)\n", tUrl, ev.file_size / 1048576.0);
        printf("%s\n", ev.stream_url);
        fflush(stdout);
      } else if (ev.type == NUVIO_ENGINE_EVENT_TORRENT_ERROR) {
        fprintf(stderr, "[poc] error: %s\n", ev.message);
      } else if (ev.type == NUVIO_ENGINE_EVENT_DISK_CACHE_RECLAIMED) {
        fprintf(stderr, "[poc] disk cache reclaimed: %s\n", ev.message);
      }
      nuvio_engine_event_init(&ev);
    }
    if (now_sec() - last_stat >= 2) {
      nuvio_engine_stats s;
      last_stat = now_sec();
      nuvio_engine_stats_init(&s);
      nuvio_engine_get_stats(m, &s);
      fprintf(stderr, "[poc] %5.1fs peers %u (seeds %u) down %6.0f KB/s up %5.0f KB/s total %6.1f MB"
              " mem %4.1f/%4.1f MB disk %6.1f/%6.1f MB http %u\n",
              now_sec() - t0, s.connected_peers, s.connected_seeds,
              s.download_rate_bytes_per_second / 1024.0, s.upload_rate_bytes_per_second / 1024.0,
              s.total_payload_download_bytes / 1048576.0,
              s.memory_cache_used_bytes / 1048576.0, s.memory_cache_capacity_bytes / 1048576.0,
              s.disk_cache_used_bytes / 1048576.0, s.disk_cache_capacity_bytes / 1048576.0,
              s.active_http_requests);
    }
    usleep(50 * 1000);
  }
  fprintf(stderr, "[poc] end: metadata %.1fs, url %.1fs\n", tMeta, tUrl);
  nuvio_engine_destroy(m);
  return 0;
}
