#include "globals.h"

int small_pizza_sales[5] = {0};
int large_pizza_sales[5] = {0};
double total_earnings = 0.0;
int fire_signal = 0;
int end_of_day = 0;
int evacuation = 0;
pthread_cond_t fire_alarm = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
