#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "pizza.h"
#include "globals.h"
#include "boss.h"
#include "logging.h"

#define MAX_QUEUE_SIZE 50

void add_to_priority_queue(PriorityQueue* queue, int group_size, const char* group_name, int time_waited) {
    srand(time(NULL));
    if ((queue->queue_rear + 1) % MAX_QUEUE_SIZE == queue->queue_front) {
        log_message("[Szef] Kolejka jest pełna! %s opuszcza pizzerię.\n", group_name);
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

int reserve_table_for_group(int group_size) {
    int table_id = -1;

    lock_semaphore();

    for (int i = 0; i < total_tables; i++) {
        if (shm_data->table_occupancy[i] == 0 && table_sizes[i] >= group_size) {
            if (table_id == -1 || table_sizes[i] < table_sizes[table_id]) {
                table_id = i;
            }
        }
    }

    if (table_id != -1) {
        shm_data->table_occupancy[table_id] = -group_size; // Rezerwacja oznaczona wartością ujemną
    }

    unlock_semaphore();
    return table_id;
}

void seat_group(int table_id, int group_size, const char* group_name) {
    lock_semaphore();

    if (shm_data->table_occupancy[table_id] == -group_size) {
        shm_data->table_occupancy[table_id] = group_size;
        shm_data->group_count_at_table[table_id] = 1;
        strncpy(shm_data->group_at_table[table_id][0], group_name, MAX_GROUP_NAME_SIZE - 1);
        shm_data->group_at_table[table_id][0][MAX_GROUP_NAME_SIZE - 1] = '\0';

        log_message("[Szef] Grupa PID: %d (%s) - %d osobowa usiadła przy stoliku %d-%d osobowym.\n",
                    getpid(), group_name, group_size, table_id, table_sizes[table_id]);
    } else {
        log_message("[Szef] Błąd: stolik %d nie był poprawnie zarezerwowany.\n", table_id);
    }

    unlock_semaphore();
}

void process_order(int group_size, const char* group_name) {
    Pizza selected_pizzas[group_size];
    int selected_sizes[group_size];

    select_random_pizzas(group_size, selected_pizzas, selected_sizes);
    double total_cost = calculate_total_cost(selected_pizzas, selected_sizes, group_size);

    log_message("[Szef] %s zamawia pizze:\n", group_name);
    for (int i = 0; i < group_size; i++) {
        log_message("  - %s (%s) - %.2f zł\n", selected_pizzas[i].name,
                    selected_sizes[i] == 0 ? "mała" : "duża",
                    selected_sizes[i] == 0 ? selected_pizzas[i].small_price : selected_pizzas[i].large_price);
    }
    log_message("[Szef] %s płaci: %.2f zł\n", group_name, total_cost);

    update_sales_and_earnings(selected_pizzas, selected_sizes, group_size);
}

static void handle_queue_entry(QueueEntry entry) {
    int table_id = reserve_table_for_group(entry.group_size);

    if (table_id == -1) {
        log_message("[Szef] Brak dostępnych stolików dla %s. Grupa wraca do kolejki.\n", entry.group_name);
        add_to_priority_queue(&shm_data->queues.queue, entry.group_size, entry.group_name, entry.time_waited + 1);
        return;
    }

    log_message("[Klient] Grupa %s (PID %d) składa zamówienie.\n", entry.group_name, getpid());
    process_order(entry.group_size, entry.group_name);
    log_message("[Szef] Zamówienie dla %s zostało przyjęte.\n", entry.group_name);

    seat_group(table_id, entry.group_size, entry.group_name);

    log_message("[Szef] Grupę %s ostatecznie posadzono i rozpoczęła konsumpcję.\n", entry.group_name);
}

void handle_queue() {
    while (1) {
        lock_semaphore();
        if (shm_data->end_of_day) {
            unlock_semaphore();
            log_message("[Szef] Koniec dnia. Przerywam obsługę kolejki.\n");
            break;
        }
        unlock_semaphore();

        if (!is_queue_empty(&shm_data->queues.queue)) {
            QueueEntry entry = remove_from_queue(&shm_data->queues.queue);
            handle_queue_entry(entry);
        }

        usleep(100000);
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