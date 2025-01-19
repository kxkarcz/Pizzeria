#ifndef BOSS_H
#define BOSS_H

#include "globals.h"
#include "pizza.h"

void add_to_priority_queue(PriorityQueue* queue, int group_size, const char* group_name, int time_waited);
int is_queue_empty(PriorityQueue* queue);
QueueEntry remove_from_queue(PriorityQueue* queue);
void handle_queue();
void update_sales_and_earnings(Pizza *selected_pizzas, int *selected_sizes, int group_size);

#endif // BOSS_H