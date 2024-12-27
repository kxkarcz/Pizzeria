#ifndef PIZZERIA_TABLES_H
#define PIZZERIA_TABLES_H

#include <pthread.h>
#include <semaphore.h>

#define TABLE_TYPES 4 //X1, X2, X3, X4
#define MAX_TABLES 1 //maksymalna ilość stolików danego typu
#define MAX_GROUPS 4 //maksymalna ilość grup przy stoliku

int table_occupancy[TABLE_TYPES][MAX_TABLES];
int table_group_size[TABLE_TYPES][MAX_TABLES];
pthread_t table_threads[TABLE_TYPES][MAX_TABLES][MAX_GROUPS];
int table_thread_count[TABLE_TYPES][MAX_TABLES];
sem_t table_semaphores[TABLE_TYPES];

void initialize_tables();
int locate_table_for_group(int group_size, pthread_t thread_id);
void free_table(int group_size, int table_id, pthread_t thread_id);

#endif // PIZZERIA_TABLES_H