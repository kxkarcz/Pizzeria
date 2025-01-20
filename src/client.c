#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "globals.h"
#include "boss.h"
#include "logging.h"

void handle_fire_signal(int signum) {
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
        log_message("[Klient] Grupa %d opuszcza lokal z powodu ograniczeń.\n", getpid());
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
        log_message("[Klient] Koniec dnia. Grupa %d opuszcza lokal.\n", getpid());
        unlock_semaphore();
        exit(EXIT_FAILURE);
    }

    if (group_size <= 3) {
        add_to_priority_queue(&shm_data->queues.queue, group_size, group_name, 0);
    }
    unlock_semaphore();
    log_message("[Klient] Grupa o PID %d (%d osoby) zgłosiła się do kolejki szefa.\n", getpid(), group_size);

    int table_assigned = -1;
    while (1) {
        lock_semaphore();
        if (shm_data->end_of_day) {
            log_message("[Klient] Koniec dnia. Grupa o PID %d opuszcza lokal.\n", getpid());
            unlock_semaphore();
            exit(0);
        }

        for (int i = 0; i < total_tables; i++) {
            if (strcmp(shm_data->group_at_table[i], group_name) == 0) {
                table_assigned = i;
                break;
            }
        }
        unlock_semaphore();

        if (table_assigned != -1) {
            log_message("[Klient] Grupa o PID %d usiadła przy stoliku %d.\n", getpid(), table_assigned);
            break;
        }

        usleep(500000);
    }
    sleep(rand() % 5 + 5);
    lock_semaphore();
    if (table_assigned != -1) {
        shm_data->table_occupancy[table_assigned] -= group_size;
        if (shm_data->table_occupancy[table_assigned] == 0) {
            memset(shm_data->group_at_table[table_assigned], 0, MAX_GROUP_NAME_SIZE);
        }
    }
    unlock_semaphore();

    log_message("[Klient] Grupa o PID %d zwalnia stolik i wychodzi z lokalu.\n", getpid());
    exit(0);
}