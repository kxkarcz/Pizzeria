#include "tables.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>

int table_occupancy[TABLE_TYPES][MAX_TABLES];
int table_group_size[TABLE_TYPES][MAX_TABLES];
pthread_t table_threads[TABLE_TYPES][MAX_TABLES][MAX_GROUPS];
int table_thread_count[TABLE_TYPES][MAX_TABLES];
sem_t* table_semaphores[TABLE_TYPES];

void initialize_tables() {
    for (int i = 0; i < TABLE_TYPES; i++) {
        for (int j = 0; j < MAX_TABLES; j++) {
            table_occupancy[i][j] = 0;
            table_group_size[i][j] = 0;
            table_thread_count[i][j] = 0;
        }
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/table_sem_%d", i);
        table_semaphores[i] = sem_open(sem_name, O_CREAT, 0644, 1);
        if (table_semaphores[i] == SEM_FAILED) {
            perror("Błąd przy otwieraniu semafora (sem_open)");
            exit(EXIT_FAILURE);
        }
    }
}

int locate_table_for_group(int group_size, pthread_t thread_id) {
    int table_id = -1;

    // 1. Sprawdzamy stolik dokładnie pasujący do grupy
    for (int i = 0; i < TABLE_TYPES; i++) {
        if (sem_wait(table_semaphores[i]) != 0) {
            perror("Błąd przy oczekiwaniu na semafor (sem_wait)");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < MAX_TABLES; j++) {
            if (table_occupancy[i][j] == 0 && table_group_size[i][j] == 0) {
                if (i + 1 == group_size) {
                    table_occupancy[i][j] = group_size;
                    table_group_size[i][j] = group_size;
                    table_threads[i][j][table_thread_count[i][j]++] = thread_id;
                    table_id = i * MAX_TABLES + j;

                    log_event("Wątek %lu: Grupa %d-osobowa usiadła przy stoliku %d. Zajęte miejsca: %d/%d.\n",
                              (unsigned long)thread_id, group_size, table_id, table_occupancy[i][j], i + 1);
                    break;
                }
            }
        }

        if (sem_post(table_semaphores[i]) != 0) {
            perror("Błąd przy zwalnianiu semafora (sem_post)");
            exit(EXIT_FAILURE);
        }

        if (table_id != -1) {
            break;
        }
    }

    // 2. Sprawdzamy stolik, który ma już grupę o tej samej wielkości, ale stolik nie jest jeszcze pełny
    if (table_id == -1) {
        for (int i = 0; i < TABLE_TYPES; i++) {
            if (sem_wait(table_semaphores[i]) != 0) {
                perror("Błąd przy oczekiwaniu na semafor (sem_wait)");
                exit(EXIT_FAILURE);
            }

            for (int j = 0; j < MAX_TABLES; j++) {
                if (table_occupancy[i][j] > 0 && table_group_size[i][j] == group_size) {
                    if (table_occupancy[i][j] + group_size <= i + 1) {
                        table_occupancy[i][j] += group_size;
                        table_group_size[i][j] = group_size;
                        table_threads[i][j][table_thread_count[i][j]++] = thread_id;
                        table_id = i * MAX_TABLES + j;

                        log_event("Wątek %lu: Grupa %d-osobowa usiadła przy stoliku %d. Zajęte miejsca: %d/%d.\n",
                                  (unsigned long)thread_id, group_size, table_id, table_occupancy[i][j], i + 1);
                        log_event("Grupa siedzi wraz z grupami: ");
                        for (int k = 0; k < table_thread_count[i][j] - 1; k++) {
                            log_event("%d-osobowymi:%lu ", table_group_size[i][j], (unsigned long)table_threads[i][j][k]);
                        }
                        log_event("\n");
                        break;
                    }
                }
            }

            if (sem_post(table_semaphores[i]) != 0) {
                perror("Błąd przy zwalnianiu semafora (sem_post)");
                exit(EXIT_FAILURE);
            }

            if (table_id != -1) {
                break;
            }
        }
    }

    // 3. Jeśli nie ma stolika o dokładnej wielkości ani częściowo zajętego, szukamy większego stolika
    if (table_id == -1) {
        for (int i = group_size; i < TABLE_TYPES; i++) {
            if (sem_wait(table_semaphores[i]) != 0) {
                perror("Błąd przy oczekiwaniu na semafor (sem_wait)");
                exit(EXIT_FAILURE);
            }

            for (int j = 0; j < MAX_TABLES; j++) {
                // Szukamy stolika, który jest wystarczająco duży i całkowicie wolny
                if (table_occupancy[i][j] == 0) {
                    table_occupancy[i][j] = group_size;
                    table_group_size[i][j] = group_size;
                    table_threads[i][j][table_thread_count[i][j]++] = thread_id;
                    table_id = i * MAX_TABLES + j;

                    log_event("Wątek %lu: Grupa %d-osobowa usiadła przy stoliku %d. Zajęte miejsca: %d/%d.\n",
                              (unsigned long)thread_id, group_size, table_id, table_occupancy[i][j], i + 1);
                    break;
                }
            }

            if (sem_post(table_semaphores[i]) != 0) {
                perror("Błąd przy zwalnianiu semafora (sem_post)");
                exit(EXIT_FAILURE);
            }

            if (table_id != -1) {
                break;
            }
        }
    }

    return table_id;
}

void free_table(int group_size, int table_id, pthread_t thread_id) {
    int table_type = table_id / MAX_TABLES;
    int table_index = table_id % MAX_TABLES;

    if (sem_wait(table_semaphores[table_type]) != 0) {
        perror("Błąd przy oczekiwaniu na semafor (sem_wait)");
        exit(EXIT_FAILURE);
    }

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

    if (sem_post(table_semaphores[table_type]) != 0) {
        perror("Błąd przy zwalnianiu semafora (sem_post)");
        exit(EXIT_FAILURE);
    }
}