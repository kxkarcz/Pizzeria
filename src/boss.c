#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "pizza.h"
#include "globals.h"
#include "boss.h"

#define MAX_QUEUE_SIZE 50

void add_to_priority_queue(PriorityQueue* queue, int group_size, const char* group_name, int time_waited) {
    srand(time(NULL));
    if ((queue->queue_rear + 1) % MAX_QUEUE_SIZE == queue->queue_front) {
        printf("[Szef] Kolejka jest pełna! %s opuszcza pizzerię.\n", group_name);
        return;
    }

    int idx = queue->queue_rear;
    queue->queue[idx].group_size = group_size;
    strncpy(queue->queue[idx].group_name, group_name, MAX_GROUP_NAME_SIZE - 1);
    queue->queue[idx].group_name[MAX_GROUP_NAME_SIZE - 1] = '\0';
    queue->queue[idx].time_waited = time_waited;
    queue->queue[idx].last_message_time = time(NULL);

    queue->queue_rear = (queue->queue_rear + 1) % MAX_QUEUE_SIZE;
}

int is_queue_empty(PriorityQueue* queue) {
    return queue->queue_front == queue->queue_rear;
}

QueueEntry remove_from_queue(PriorityQueue* queue) {
    if (queue->queue_front == queue->queue_rear) {
        fprintf(stderr, "[Szef] Próba usunięcia grupy z pustej kolejki.\n");
        exit(EXIT_FAILURE);
    }
    QueueEntry entry = queue->queue[queue->queue_front];
    queue->queue_front = (queue->queue_front + 1) % MAX_QUEUE_SIZE;
    return entry;
}

int find_table_for_group(int group_size, const char* group_name) {
    lock_semaphore();
    int table_id = -1;

    // Znajdź pusty stolik
    for (int i = 0; i < total_tables; i++) {
        if (shm_data->table_occupancy[i] == 0 && table_sizes[i] >= group_size) {
            table_id = i;
            shm_data->table_occupancy[i] = group_size;
            shm_data->group_count_at_table[i] = 1; // Pierwsza grupa przy stoliku
            strncpy(shm_data->group_at_table[i][0], group_name, MAX_GROUP_NAME_SIZE - 1);
            shm_data->group_at_table[i][0][MAX_GROUP_NAME_SIZE - 1] = '\0';

            printf("[Szef] Grupa PID: %d - %d osobowa usiadła przy stoliku %d-%d osobowym.\n",
                   getpid(), group_size, table_id, table_sizes[i]);
            unlock_semaphore();
            return table_id;
        }
    }

    // Znajdź stolik, do którego grupa może dołączyć
    for (int i = 0; i < total_tables; i++) {
        if (shm_data->table_occupancy[i] > 0 &&
            shm_data->table_occupancy[i] + group_size <= table_sizes[i] &&
            shm_data->group_count_at_table[i] < MAX_GROUPS_PER_TABLE) {

            table_id = i;
            shm_data->table_occupancy[i] += group_size;

            int group_index = shm_data->group_count_at_table[i];
            strncpy(shm_data->group_at_table[i][group_index], group_name, MAX_GROUP_NAME_SIZE - 1);
            shm_data->group_at_table[i][group_index][MAX_GROUP_NAME_SIZE - 1] = '\0';
            shm_data->group_count_at_table[i]++;

            printf("[Szef] Grupa PID: %d - %d osobowa usiadła przy stoliku %d-%d osobowym razem z grupą/grupami: ",
                   getpid(), group_size, table_id, table_sizes[i]);

            for (int j = 0; j < group_index; j++) {
                printf("%s%s", shm_data->group_at_table[i][j],
                       (j == group_index - 1) ? "" : ", ");
            }
            printf(".\n");

            unlock_semaphore();
            return table_id;
        }
    }

    unlock_semaphore();
    return table_id;
}

void process_order(int group_size, const char* group_name) {
    Pizza selected_pizzas[group_size];
    int selected_sizes[group_size];

    select_random_pizzas(group_size, selected_pizzas, selected_sizes);
    double total_cost = calculate_total_cost(selected_pizzas, selected_sizes, group_size);

    printf("[Szef] %s zamawia pizze:\n", group_name);
    for (int i = 0; i < group_size; i++) {
        printf("  - %s (%s) - %.2f zł\n", selected_pizzas[i].name,
               selected_sizes[i] == 0 ? "mała" : "duża",
               selected_sizes[i] == 0 ? selected_pizzas[i].small_price : selected_pizzas[i].large_price);
    }
    printf("[Szef] %s płaci: %.2f zł\n", group_name, total_cost);

    update_sales_and_earnings(selected_pizzas, selected_sizes, group_size);
}

void handle_queue() {
    while (!is_queue_empty(&shm_data->queues.small_groups) || !is_queue_empty(&shm_data->queues.large_groups)) {
        lock_semaphore();
        if (shm_data->end_of_day) {
            printf("[Szef] Koniec dnia. Przerywam obsługę kolejki.\n");
            unlock_semaphore();
            break;
        }
        unlock_semaphore();

        PriorityQueue* current_queue = !is_queue_empty(&shm_data->queues.large_groups)
                                       ? &shm_data->queues.large_groups
                                       : &shm_data->queues.small_groups;

        QueueEntry entry = remove_from_queue(current_queue);

        int table_id = find_table_for_group(entry.group_size, entry.group_name);

        if (table_id != -1) {
            process_order(entry.group_size, entry.group_name);
            printf("[Szef] Zamówienie dla %s zostało zrealizowane.\n", entry.group_name);
        } else {
            entry.time_waited++;
            add_to_priority_queue(current_queue, entry.group_size, entry.group_name, entry.time_waited);
            usleep(100000);
        }
    }
}

void update_sales_and_earnings(Pizza *selected_pizzas, int *selected_sizes, int group_size) {
    int small_sales[5] = {0};
    int large_sales[5] = {0};
    double total_earnings = 0.0;

    for (int i = 0; i < group_size; i++) {
        if (selected_sizes[i] == 0) {
            small_sales[selected_pizzas[i].id]++;
            total_earnings += selected_pizzas[i].small_price;
        } else {
            large_sales[selected_pizzas[i].id]++;
            total_earnings += selected_pizzas[i].large_price;
        }
    }

    lock_semaphore();
    for (int i = 0; i < 5; i++) {
        shm_data->small_pizza_sales[i] += small_sales[i];
        shm_data->large_pizza_sales[i] += large_sales[i];
    }
    shm_data->total_earnings += total_earnings;
    unlock_semaphore();
}