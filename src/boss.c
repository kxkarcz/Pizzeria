#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
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
        errno = EINVAL;
        perror("[Szef] Próba usunięcia grupy z pustej kolejki");
        exit(EXIT_FAILURE);
    }
    QueueEntry entry = queue->queue[queue->queue_front];
    queue->queue_front = (queue->queue_front + 1) % MAX_QUEUE_SIZE;
    return entry;
}

int reserve_table_for_group(int group_size, const char* group_name) {
    int table_id = -1;
    int addable_table_id = -1;
    int larger_empty_table_id = -1;

    lock_semaphore();

    for (int i = 0; i < total_tables; i++) {
        // Pusty stolik idealnie dopasowany
        if (shm_data->table_occupancy[i] == 0 && table_sizes[i] == group_size) {
            table_id = i;
            break;
        }

        // Stolik częściowo zajęty, pasujący do grupy
        if (shm_data->table_occupancy[i] > 0 &&
            shm_data->table_occupancy[i] + group_size <= table_sizes[i]) {
            // Sprawdzamy, czy wszystkie grupy przy tym stoliku mają ten sam rozmiar
            int can_add = 1;
            for (int j = 0; j < shm_data->group_count_at_table[i]; j++) {
                if (shm_data->group_sizes_at_table[i][j] != group_size) {
                    can_add = 0;
                    break;
                }
            }
            if (can_add) {
                addable_table_id = i;
            }
        }

        // Większy pusty stolik
        if (shm_data->table_occupancy[i] == 0 &&
            table_sizes[i] > group_size &&
            larger_empty_table_id == -1) {
            larger_empty_table_id = i;
        }
    }

    if (table_id == -1) {
        table_id = addable_table_id;
    }
    if (table_id == -1) {
        table_id = larger_empty_table_id;
    }

    // Rezerwacja
    if (table_id != -1) {
        shm_data->table_occupancy[table_id] += group_size;
        int group_index = shm_data->group_count_at_table[table_id]++;
        shm_data->group_sizes_at_table[table_id][group_index] = group_size;
        strncpy(shm_data->group_at_table[table_id][group_index], group_name, MAX_GROUP_NAME_SIZE - 1);
        shm_data->group_at_table[table_id][group_index][MAX_GROUP_NAME_SIZE - 1] = '\0';
    }

    unlock_semaphore();
    return table_id;
}

void seat_group(int table_id, int group_size, const char* group_name) {
    lock_semaphore();

    if (table_id != -1) {
        log_message("[Szef] Grupa %s - %d osobowa została przypisana do stolika %d\n",
                    group_name, group_size, table_id);
    } else {
        log_message("[Szef] Błąd: Nie można przypisać grupy %s do żadnego stolika.\n", group_name);
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
    int table_id = reserve_table_for_group(entry.group_size, entry.group_name);

    if (table_id == -1) {
        add_to_priority_queue(&shm_data->queues.queue, entry.group_size, entry.group_name, entry.time_waited + 1);
        return;
    }

    log_message("[Klient] Grupa %s (PID %d) - %d osobowa składa zamówienie.\n", entry.group_name, getpid(), entry.group_size);
    process_order(entry.group_size, entry.group_name);
    log_message("[Szef] Zamówienie dla %s zostało przyjęte.\n", entry.group_name);

    seat_group(table_id, entry.group_size, entry.group_name);

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