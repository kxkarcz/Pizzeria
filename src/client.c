#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "globals.h"
#include "boss.h"

void client_function(int group_size) {
    disable_signal_handling();
    atexit(cleanup_shared_memory);

    char group_name[MAX_GROUP_NAME_SIZE];
    snprintf(group_name, sizeof(group_name), "Grupa_%d", getpid());
    printf("[Klient] %s (%d osoby) zgłasza się do szefa.\n", group_name, group_size);

    lock_semaphore();
    if (group_size <= 2) {
        add_to_priority_queue(&shm_data->queues.small_groups, group_size, group_name, 0);
    } else {
        add_to_priority_queue(&shm_data->queues.large_groups, group_size, group_name, 0);
    }
    unlock_semaphore();

    int table_id = -1;

    while (table_id == -1) {
        lock_semaphore();
        for (int i = 0; i < total_tables; i++) {
            if (strcmp(shm_data->group_at_table[i], group_name) == 0) {
                table_id = i;
                break;
            }
        }
        unlock_semaphore();

        if (table_id == -1) {
            usleep(500000); // Odczekaj 0.5 sekundy przed kolejną próbą
        }
    }

    printf("[Klient] %s siada przy stoliku %d (%d-osobowy).\n", group_name, table_id, table_sizes[table_id]);


    sleep(rand() % 6 + 5); // Symulacja jedzenia pizzy prze 5-10 sekund

    // Zwolnienie stolika po zakończeniu posiłku
    lock_semaphore();
    shm_data->table_occupancy[table_id] -= group_size;
    if (shm_data->table_occupancy[table_id] == 0) {
        memset(shm_data->group_at_table[table_id], 0, sizeof(shm_data->group_at_table[table_id]));
    }
    unlock_semaphore();

    printf("[Klient] %s zwalnia stolik %d i wychodzi.\n", group_name, table_id);
}