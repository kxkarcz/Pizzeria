#ifndef PIZZERIA_BOSS_H
#define PIZZERIA_BOSS_H

#include <sys/types.h>
#include "pizza.h"

void boss_function(int group_size, pthread_t thread_id);
void update_sales_and_earnings(Pizza* selected_pizzas, int* selected_sizes, int group_size);

#endif // PIZZERIA_BOSS_H