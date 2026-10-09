#include "clock.h"
#include <string.h>

// Greater than this difference between sample and prediction is a jump (seek,
// source switch, resume): history is no longer valid.
#define CLOCK_JUMP     0.75
// Prediction never exceeds this beyond the last sample: if pipeline stopped
// reporting position (buffering), subtitles stop right after.
#define CLOCK_CEIL     0.35
// Tolerated backoff without following: samples fluctuate tens of ms; stepping
// backwards due to this would make text jitter. More than this is an actual jump.
#define CLOCK_BACKOFF  0.25

void clock_reset(Clock *r) { memset(r, 0, sizeof *r); }

static double rate_of(const Clock *r) { return r->rate > 0.0 ? r->rate : 1.0; }

void clock_rate(Clock *r, double rate) {
  if (rate <= 0.0) rate = 1.0;
  if (rate == rate_of(r)) return;
  r->rate = rate;
  r->n = r->next = 0;
}

static double max_off(const Clock *r) {
  int i;
  double m = r->off[0];
  for (i = 1; i < r->n; i++)
    if (r->off[i] > m) m = r->off[i];
  return m;
}

void clock_sample(Clock *r, double pos_sec, double now, int playing) {
  int is_new = !r->has_sample || pos_sec != r->last_pos;
  if (!playing) {
    // Paused: position matches pipeline directly without prediction, and history drops —
    // upon resuming, offset is different.
    r->playing = 0;
    r->n = r->next = 0;
    r->last_pos = pos_sec;
    r->last_now = now;
    r->has_sample = 1;
    r->output = pos_sec;
    return;
  }
  if (!r->playing) {
    r->playing = 1;
    r->n = r->next = 0;
    is_new = 1;
  }
  if (!is_new) return;
  {
    double off = pos_sec - now * rate_of(r);
    if (r->n && (off - max_off(r) > CLOCK_JUMP || max_off(r) - off > CLOCK_JUMP)) {
      r->n = r->next = 0;
      r->output = pos_sec;
    } else if (r->has_sample && pos_sec < r->last_pos - CLOCK_BACKOFF) {
      r->n = r->next = 0;
      r->output = pos_sec;
    }
    r->off[r->next] = off;
    r->next = (r->next + 1) % CLOCK_SAMPLES;
    if (r->n < CLOCK_SAMPLES) r->n++;
  }
  r->last_pos = pos_sec;
  r->last_now = now;
  r->has_sample = 1;
}

double clock_read(Clock *r, double now) {
  double v;
  if (!r->has_sample) return 0.0;
  if (!r->playing || !r->n) {
    r->output = r->last_pos;
    return r->last_pos;
  }
  v = now * rate_of(r) + max_off(r);
  // Ceiling is in CLOCK time: at 2x, 350 ms of clock time is 700 ms of video.
  if (v > r->last_pos + CLOCK_CEIL * rate_of(r)) v = r->last_pos + CLOCK_CEIL * rate_of(r);
  if (v < r->last_pos - CLOCK_BACKOFF) v = r->last_pos;
  // Monotonic between jumps: small backoff (sample arriving early fell out of window)
  // holds value steady instead of rewinding.
  if (v < r->output && r->output - v < CLOCK_BACKOFF) v = r->output;
  r->output = v;
  return v;
}

// Backward-compatibility wrappers
void relogio_zerar(Relogio *r) { clock_reset(r); }
void relogio_amostra(Relogio *r, double posSeg, double agora, int tocando) {
  clock_sample(r, posSeg, agora, tocando);
}
double relogio_ler(Relogio *r, double agora) { return clock_read(r, agora); }
void relogio_taxa(Relogio *r, double taxa) { clock_rate(r, taxa); }
