#ifndef BOSS_H
#define BOSS_H

#include "pizza.h"
#include "globals.h"

void add_to_priority_queue(PriorityQueue* queue, int group_size, const char* group_name, int time_waited);
QueueEntry remove_from_queue();
int is_queue_empty();
int find_table_for_group(int group_size, const char* group_name);
void process_order(int group_size, const char* group_name);
void handle_queue();
void update_sales_and_earnings(Pizza* selected_pizzas, int* selected_sizes, int group_size);

#endif // BOSS_H
