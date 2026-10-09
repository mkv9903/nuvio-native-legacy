#include "net_epoch.h"
#include "streamfit.h"
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t current_sequence, current_epoch;

uint64_t net_epoch_current(void) {
  uint64_t v;
  pthread_mutex_lock(&lock);
  v = current_epoch;
  pthread_mutex_unlock(&lock);
  return v;
}

void net_epoch_observe(uint64_t seq, int known) {
  pthread_mutex_lock(&lock);
  if (seq && (seq > current_sequence || (seq == current_sequence && !known))) {
    current_sequence = seq;
    current_epoch = known ? seq : 0;
    /* Publish invalidation in the same critical section as the epoch: a
     * worker cannot read a new epoch while the engine still has the old one. */
    streamfit_rede(current_epoch);
  }
  pthread_mutex_unlock(&lock);
}

#ifdef NV_ANDROID
#include <jni.h>
JNIEXPORT void JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeRedeAlterou(JNIEnv *env,
    jobject act, jlong seq, jboolean conhecida) {
  (void)env; (void)act;
  if (seq > 0) net_epoch_observe((uint64_t)seq, conhecida == JNI_TRUE);
}
#endif

// Backward-compatibility shims
uint64_t redemarca_atual(void) { return net_epoch_current(); }
void redemarca_observar(uint64_t seq, int conhecida) { net_epoch_observe(seq, conhecida); }
