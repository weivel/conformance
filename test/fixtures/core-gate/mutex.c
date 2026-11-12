#include "fixture.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static SRWLOCK fixture_lock = SRWLOCK_INIT;

void weivel_mutex_lock(void) {
    AcquireSRWLockExclusive(&fixture_lock);
}

void weivel_mutex_unlock(void) {
    ReleaseSRWLockExclusive(&fixture_lock);
}

#else

#include <pthread.h>

static pthread_mutex_t fixture_lock = PTHREAD_MUTEX_INITIALIZER;

void weivel_mutex_lock(void) {
    pthread_mutex_lock(&fixture_lock);
}

void weivel_mutex_unlock(void) {
    pthread_mutex_unlock(&fixture_lock);
}

#endif
