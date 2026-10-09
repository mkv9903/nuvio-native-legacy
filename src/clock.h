/* INTERPOLATED video clock (#92). LG's pipeline only reports position in
 * currentTime event, every ~200 ms (measured on C9 on 23/09: 1732 intervals,
 * average 200.9 ms, 197..247). Drawing at 60 fps using the last received number
 * steps in increments and is, on average, 100 ms behind.
 *
 * Each sample (position, monotonic time received) gives an estimate of
 * offset between monotonic clock and video: off = pos - now.
 * Delivery delay only ADDS to arrival time, so the least-delayed sample
 * is the one with LARGEST offset — the clock uses the maximum of recent samples. */
#ifndef NV_CLOCK_H
#define NV_CLOCK_H

#define CLOCK_SAMPLES 12

typedef struct {
  double off[CLOCK_SAMPLES];
  int    n;
  union { int next; int prox; };
  union { double last_pos; double ultPos; };
  union { double last_now; double ultAgora; };
  union { double output; double saida; };
  union { int playing; int tocando; };
  union { int has_sample; int temAmostra; };
  union { double rate; double taxa; };
} Clock;

void   clock_reset(Clock *r);
/* Call every frame: pos_sec is pipeline position (can repeat), now in monotonic seconds.
 * New sample is detected by change in pos_sec. */
void   clock_sample(Clock *r, double pos_sec, double now, int playing);
double clock_read(Clock *r, double now);
/* SPEED (#202). At 1.5x video advances 1.5 s per clock second: with offset
 * counted at 1x, between two samples prediction moved too slow (at 2x, up to
 * 200 ms behind) and at 0.75x maximum of old samples pushed it up to ceiling ahead.
 * With rate, off = pos - now*rate. Changing rate clears history. */
void   clock_rate(Clock *r, double rate);

// Backward-compatibility shims
#define RELOGIO_AMOSTRAS CLOCK_SAMPLES
typedef Clock Relogio;

void   relogio_zerar(Relogio *r);
void   relogio_amostra(Relogio *r, double posSeg, double agora, int tocando);
double relogio_ler(Relogio *r, double agora);
void   relogio_taxa(Relogio *r, double taxa);

#endif
