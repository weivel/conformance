#include "fixture.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static SRWLOCK elsewhere_lock = SRWLOCK_INIT;

long fixture_plant(void) {
    AcquireSRWLockExclusive(&elsewhere_lock);
    ReleaseSRWLockExclusive(&elsewhere_lock);
    return 0;
}

#else

#include <pthread.h>

static pthread_mutex_t elsewhere_lock = PTHREAD_MUTEX_INITIALIZER;

long fixture_plant(void) {
    pthread_mutex_lock(&elsewhere_lock);
    return pthread_mutex_unlock(&elsewhere_lock);
}

#endif
