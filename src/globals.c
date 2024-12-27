#include "globals.h"

int fire_signal = 0;
pthread_cond_t fire_alarm = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
