#ifndef TABLES_H
#define TABLES_H

#include <pthread.h>
#include <semaphore.h>

#define TABLE_TYPES 3
#define MAX_TABLES 10
#define MAX_GROUPS 5

extern int table_occupancy[TABLE_TYPES][MAX_TABLES];
extern int table_group_size[TABLE_TYPES][MAX_TABLES];
extern pthread_t table_threads[TABLE_TYPES][MAX_TABLES][MAX_GROUPS];
extern int table_thread_count[TABLE_TYPES][MAX_TABLES];
extern sem_t* table_semaphores[TABLE_TYPES];

void initialize_tables();
int locate_table_for_group(int group_size, pthread_t thread_id);
void free_table(int group_size, int table_id, pthread_t thread_id);

#endif // TABLES_H