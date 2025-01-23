#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "logging.h"

int total_tables = 0;
int table_sizes[MAX_TABLES];

void configure_tables(int num_1_person, int num_2_person, int num_3_person, int num_4_person) {
    total_tables = 0;
    for (int i = 0; i < num_1_person; i++) table_sizes[total_tables++] = 1;
    for (int i = 0; i < num_2_person; i++) table_sizes[total_tables++] = 2;
    for (int i = 0; i < num_3_person; i++) table_sizes[total_tables++] = 3;
    for (int i = 0; i < num_4_person; i++) table_sizes[total_tables++] = 4;

    if (lock_semaphore() == -1) {
        log_message("[ERROR] Nie udało się zablokować semafora");
        return;
    }
    for (int i = 0; i < total_tables; i++) {
        shm_data->table_occupancy[i] = 0;
        memset(shm_data->group_at_table[i], 0, MAX_GROUP_NAME_SIZE);
        log_message("[System] Stolik %d: %d-osobowy\n", i, table_sizes[i]);
    }
    if (unlock_semaphore() == -1) {
        log_message("[ERROR] Nie udało się odblokować semafora");
    }

    log_message("[System] Skonfigurowano %d stolików.\n", total_tables);
}

void initialize_tables() {
    if (lock_semaphore() == -1) {
        log_message("[ERROR] Nie udało się zablokować semafora");
        return;
    }
    for (int i = 0; i < total_tables; i++) {
        shm_data->table_occupancy[i] = 0;
        memset(shm_data->group_at_table[i], 0, sizeof(shm_data->group_at_table[i]));
    }
    if (unlock_semaphore() == -1) {
        log_message("[ERROR] Nie udało się odblokować semafora");
    }
    log_message("[System] Stoliki zostały zainicjalizowane.\n");
}