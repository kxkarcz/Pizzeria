#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pizza.h"
#include "globals.h"
#include "boss.h"

#define MAX_QUEUE_SIZE 50

void add_to_priority_queue(PriorityQueue* queue, int group_size, const char* group_name, int time_waited) {
    if ((queue->queue_rear + 1) % MAX_QUEUE_SIZE == queue->queue_front) {
        printf("[Szef] Kolejka jest pełna! %s opuszcza pizzerię.\n", group_name);
        return;
    }

    // Dodanie grupy na koniec kolejki
    int idx = queue->queue_rear;
    queue->queue[idx].group_size = group_size;
    strncpy(queue->queue[idx].group_name, group_name, MAX_GROUP_NAME_SIZE - 1);
    queue->queue[idx].group_name[MAX_GROUP_NAME_SIZE - 1] = '\0';
    queue->queue[idx].time_waited = time_waited;

    queue->queue_rear = (queue->queue_rear + 1) % MAX_QUEUE_SIZE;
}

// Sprawdzanie, czy kolejka jest pusta
int is_queue_empty(PriorityQueue* queue) {
    return queue->queue_front == queue->queue_rear;
}

// Usuwanie grupy z kolejki
QueueEntry remove_from_queue(PriorityQueue* queue) {
    if (queue->queue_front == queue->queue_rear) {
        fprintf(stderr, "[Szef] Próba usunięcia grupy z pustej kolejki.\n");
        exit(EXIT_FAILURE);
    }
    QueueEntry entry = queue->queue[queue->queue_front];
    queue->queue_front = (queue->queue_front + 1) % MAX_QUEUE_SIZE;
    return entry;
}
// Znalezienie odpowiedniego stolika dla grupy
int find_table_for_group(int group_size, const char* group_name) {
    lock_semaphore();
    int table_id = -1;

    // 1. Idealny stolik
    for (int i = 0; i < total_tables; i++) {
        if (shm_data->table_occupancy[i] == 0 && table_sizes[i] == group_size) {
            table_id = i;
            shm_data->table_occupancy[i] = group_size;
            strncpy(shm_data->group_at_table[i], group_name, sizeof(shm_data->group_at_table[i]) - 1);
            shm_data->group_at_table[i][sizeof(shm_data->group_at_table[i]) - 1] = '\0';
            break;
        }
    }

    // 2. Współdzielenie stolika
    if (table_id == -1) {
        for (int i = 0; i < total_tables; i++) {
            if (shm_data->table_occupancy[i] > 0 &&
                shm_data->table_occupancy[i] + group_size <= table_sizes[i] &&
                strcmp(shm_data->group_at_table[i], group_name) != 0) {
                table_id = i;
                shm_data->table_occupancy[i] += group_size;
                if (strlen(shm_data->group_at_table[i]) + strlen(group_name) + 3 < sizeof(shm_data->group_at_table[i])) {
                    strncat(shm_data->group_at_table[i], ", ", sizeof(shm_data->group_at_table[i]) - strlen(shm_data->group_at_table[i]) - 1);
                    strncat(shm_data->group_at_table[i], group_name, sizeof(shm_data->group_at_table[i]) - strlen(shm_data->group_at_table[i]) - 1);
                }
                break;
            }
        }
    }

    unlock_semaphore();
    return table_id;
}

// Obsługa zamówienia grupy
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

// Obsługa kolejki
void handle_queue() {
    lock_semaphore();
    SharedQueues* queues = &shm_data->queues;

    while (!is_queue_empty(&queues->small_groups) || !is_queue_empty(&queues->large_groups)) {
        PriorityQueue* current_queue;

        // Obsługa grup dużych z priorytetem
        if (!is_queue_empty(&queues->large_groups)) {
            current_queue = &queues->large_groups;
        } else {
            current_queue = &queues->small_groups;
        }

        QueueEntry entry = remove_from_queue(current_queue);
        printf("[Szef] Obsługa grupy %s (%d osoby) z kolejki.\n", entry.group_name, entry.group_size);

        int table_id = find_table_for_group(entry.group_size, entry.group_name);

        if (table_id != -1) {
            process_order(entry.group_size, entry.group_name);
            printf("[Szef] Zamówienie dla %s zostało zrealizowane.\n", entry.group_name);
        } else {
            printf("[Szef] Brak miejsca dla %s (%d osoby). Ponowne dodanie do kolejki.\n", entry.group_name, entry.group_size);
            entry.time_waited++;
            add_to_priority_queue(current_queue, entry.group_size, entry.group_name, entry.time_waited);
        }
    }
    unlock_semaphore();
}

// Aktualizacja danych o sprzedaży i zarobkach
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