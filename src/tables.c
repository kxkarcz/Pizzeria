//
// Created by Klaudia Karczmarczyk on 27/12/2024.
//

#include "tables.h"
#include <semaphore.h>

void initialize_tables() {
    table_status[0] = 3; // liczba stolików 1-osobowych
    table_status[1] = 2; // liczba stolików 2-osobowych
    table_status[2] = 3; // liczba stolików 3-osobowych
    table_status[3] = 5; // liczba stolików 4-osobowych
    for (int i = 0; i < TABLE_TYPES; i++) {
        sem_init(&table_semaphores[i], 0, 1);
    }
}

int locate_table_for_group(int group_size) {
    sem_wait(&table_semaphores[group_size - 1]);
    int result = 0;
    if (table_status[group_size - 1] > 0) {
        table_status[group_size - 1]--;
        result = 1; // Sukces
    }
    sem_post(&table_semaphores[group_size - 1]);
    return result; // Brak stolików
}

void free_table(int table_size) {
    sem_wait(&table_semaphores[table_size - 1]);
    table_status[table_size - 1]++;
    sem_post(&table_semaphores[table_size - 1]);
}