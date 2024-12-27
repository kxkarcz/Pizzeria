#include "tables.h"
#include <stdio.h>

void initialize_tables() {
    for (int i = 0; i < TABLE_TYPES; i++) {
        for (int j = 0; j < MAX_TABLES; j++) {
            table_occupancy[i][j] = 0;
            table_group_size[i][j] = 0;
            table_thread_count[i][j] = 0;
        }
        sem_init(&table_semaphores[i], 0, 1);
    }
}

int locate_table_for_group(int group_size, pthread_t thread_id) {
    int table_id = -1;

    for (int i = group_size - 1; i < TABLE_TYPES; i++) {
        sem_wait(&table_semaphores[i]);

        for (int j = 0; j < MAX_TABLES; j++) {
            if (table_occupancy[i][j] == 0 || (table_group_size[i][j] == group_size && table_occupancy[i][j] + group_size <= i + 1)) {
                table_occupancy[i][j] += group_size;
                table_group_size[i][j] = group_size;
                table_threads[i][j][table_thread_count[i][j]++] = thread_id;
                table_id = i * MAX_TABLES + j;

                printf("Klient usiadł przy stoliku %d. Zajęte miejsca: %d/%d.\n", table_id, table_occupancy[i][j], i + 1);

                if (table_thread_count[i][j] > 1) {
                    printf("Klient siedzi wraz z grupami: ");
                    for (int k = 0; k < table_thread_count[i][j] - 1; k++) {
                        printf("%d-osobowymi:%lu ", table_group_size[i][j], (unsigned long)table_threads[i][j][k]);
                    }
                    printf("\n");
                }
                break;
            }
        }

        sem_post(&table_semaphores[i]);

        if (table_id != -1) {
            break;
        }
    }

    if (table_id == -1) {
        printf("Wątek %lu: Brak dostępnych stolików dla grupy %d-osobowej.\n", (unsigned long)thread_id, group_size);
    }

    return table_id;
}

void free_table(int group_size, int table_id, pthread_t thread_id) {
    int table_type = table_id / MAX_TABLES;
    int table_index = table_id % MAX_TABLES;

    sem_wait(&table_semaphores[table_type]);

    table_occupancy[table_type][table_index] -= group_size;

    if (table_occupancy[table_type][table_index] == 0) {
        table_group_size[table_type][table_index] = 0;
        table_thread_count[table_type][table_index] = 0;
    } else {
        for (int i = 0; i < table_thread_count[table_type][table_index]; i++) {
            if (table_threads[table_type][table_index][i] == thread_id) {
                for (int j = i; j < table_thread_count[table_type][table_index] - 1; j++) {
                    table_threads[table_type][table_index][j] = table_threads[table_type][table_index][j + 1];
                }
                table_thread_count[table_type][table_index]--;
                break;
            }
        }
    }

    sem_post(&table_semaphores[table_type]);
}