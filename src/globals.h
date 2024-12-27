#ifndef PIZZERIA_GLOBALS_H
#define PIZZERIA_GLOBALS_H

#include <pthread.h>

extern int fire_signal;
extern pthread_cond_t fire_alarm;
extern pthread_mutex_t lock;

#endif // PIZZERIA_GLOBALS_H