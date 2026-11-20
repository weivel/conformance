#include "run.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static SRWLOCK run_lock = SRWLOCK_INIT;

void stub_lock(void) {
    AcquireSRWLockExclusive(&run_lock);
}

void stub_unlock(void) {
    ReleaseSRWLockExclusive(&run_lock);
}
#else
#include <pthread.h>

static pthread_mutex_t run_lock = PTHREAD_MUTEX_INITIALIZER;

void stub_lock(void) {
    pthread_mutex_lock(&run_lock);
}

void stub_unlock(void) {
    pthread_mutex_unlock(&run_lock);
}
#endif
