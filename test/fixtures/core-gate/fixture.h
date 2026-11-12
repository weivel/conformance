#ifndef FIXTURE_H
#define FIXTURE_H

#ifdef _WIN32
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT
#endif

FIXTURE_EXPORT void *fixture_create(unsigned long size);
FIXTURE_EXPORT void fixture_destroy(void *fixture);
FIXTURE_EXPORT void weivel_mutex_lock(void);
FIXTURE_EXPORT void weivel_mutex_unlock(void);
FIXTURE_EXPORT long fixture_plant(void);

#endif
