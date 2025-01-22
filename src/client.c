#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "globals.h"
#include "boss.h"
#include "logging.h"

void handle_fire_signal() {
    log_message("[Klient] Grupa o PID %d otrzymała sygnał pożarowy! Opuszczam lokal.\n", getpid());
    exit(0);
}
void client_signal_handler(int signo) {
    log_message("[Klient] Grupa o PID %d otrzymała sygnał %d. Kończę działanie...\n", getpid(), signo);
    exit(0);
}

void cleanup_client_pid() {
    if (!shm_data || sem_removed) return;

    lock_semaphore();
    for (int i = 0; i < shm_data->client_count; i++) {
        if (shm_data->client_pids[i] == getpid()) {
            shm_data->client_pids[i] = shm_data->client_pids[shm_data->client_count - 1];
            shm_data->client_count--;
            break;
        }
    }
    unlock_semaphore();
}

void client_function(int group_size) {
    signal(SIGUSR1, handle_fire_signal);
    signal(SIGTERM, client_signal_handler);
    signal(SIGINT, client_signal_handler);
    atexit(cleanup_client_pid);

    lock_semaphore();
    if (!shm_data || sem_removed || shm_data->client_count >= MAX_CLIENTS || shm_data->end_of_day) {
        unlock_semaphore();
        exit(EXIT_FAILURE);
    }

    shm_data->client_pids[shm_data->client_count] = getpid();
    shm_data->client_count++;
    unlock_semaphore();

    char group_name[MAX_GROUP_NAME_SIZE];
    snprintf(group_name, MAX_GROUP_NAME_SIZE, "Grupa_%d", getpid());

    lock_semaphore();
    if (shm_data->end_of_day) {
        unlock_semaphore();
        exit(EXIT_FAILURE);
    }

    add_to_priority_queue(&shm_data->queues.queue, group_size, group_name, 0);
    unlock_semaphore();

    int table_assigned = -1;
    while (1) {
        lock_semaphore();
        if (shm_data->end_of_day) {
            unlock_semaphore();
            exit(0);
        }

        for (int i = 0; i < total_tables; i++) {
            for (int j = 0; j < shm_data->group_count_at_table[i]; j++) {
                if (strcmp(shm_data->group_at_table[i][j], group_name) == 0) {
                    table_assigned = i;
                    break;
                }
            }
            if (table_assigned != -1) {
                break;
            }
        }
        unlock_semaphore();

        if (table_assigned != -1) {
            lock_semaphore();
            if (shm_data->group_count_at_table[table_assigned] > 1) {
                log_message("[Klient] Grupa o PID %d usiadła przy stoliku %d. Siedzimy z:\n", getpid(), table_assigned);
                for (int j = 0; j < shm_data->group_count_at_table[table_assigned]; j++) {
                    if (strcmp(shm_data->group_at_table[table_assigned][j], group_name) != 0) {
                        log_message("  - %s\n", shm_data->group_at_table[table_assigned][j]);
                    }
                }
            } else {
                log_message("[Klient] Grupa o PID %d usiadła przy stoliku %d.\n", getpid(), table_assigned);
            }
            unlock_semaphore();
            break;
        }

    }

    // Symulacja jedzenia
    //sleep(rand() % 5 + 5);
    lock_semaphore();
    if (table_assigned != -1) {
        for (int i = 0; i < shm_data->group_count_at_table[table_assigned]; i++) {
            if (strcmp(shm_data->group_at_table[table_assigned][i], group_name) == 0) {
                shm_data->table_occupancy[table_assigned] -= group_size;

                // Usuwanie grupy z listy
                for (int k = i; k < shm_data->group_count_at_table[table_assigned] - 1; k++) {
                    shm_data->group_sizes_at_table[table_assigned][k] = shm_data->group_sizes_at_table[table_assigned][k + 1];
                    strncpy(shm_data->group_at_table[table_assigned][k], shm_data->group_at_table[table_assigned][k + 1], MAX_GROUP_NAME_SIZE);
                }
                shm_data->group_count_at_table[table_assigned]--;

                // Jeśli stolik jest pusty, resetujemy jego status
                if (shm_data->table_occupancy[table_assigned] == 0) {
                    memset(shm_data->group_at_table[table_assigned], 0, sizeof(shm_data->group_at_table[table_assigned]));
                    shm_data->group_count_at_table[table_assigned] = 0;
                }
                break;
            }
        }
    }
    unlock_semaphore();

    log_message("[Klient] Grupa o PID %d zwalnia stolik i wychodzi z lokalu.\n", getpid());
    exit(0);
}
