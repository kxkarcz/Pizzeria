#ifndef PIZZERIA_GLOBALS_H
#define PIZZERIA_GLOBALS_H

#include <pthread.h>

extern int small_pizza_sales[5];
extern int large_pizza_sales[5];
extern double total_earnings;
extern int fire_signal;
extern pthread_cond_t fire_alarm;
extern pthread_mutex_t lock;
extern int end_of_day;
extern int evacuation;

#endif // PIZZERIA_GLOBALS_H