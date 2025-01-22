#ifndef GLOBALS_H
#define GLOBALS_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define SHM_KEY 1234
#define SEM_KEY 5678
#define MAX_TABLES 50
#define MAX_QUEUE_SIZE 50
#define MAX_GROUP_NAME_SIZE 20
#define MAX_CLIENTS 100
#define MAX_GROUPS_PER_TABLE 4

// Struktura reprezentująca grupę oczekującą w kolejce
typedef struct {
    int group_size;
    char group_name[MAX_GROUP_NAME_SIZE];
    int time_waited; // Czas oczekiwania
    time_t last_message_time;
} QueueEntry;

// Struktura reprezentująca kolejkę priorytetową
typedef struct {
    QueueEntry queue[MAX_QUEUE_SIZE];
    int queue_front;
    int queue_rear;
} PriorityQueue;

typedef struct {
    PriorityQueue queue;
} SharedQueues;



typedef struct shared_data {
    int cleanup_done;
    int small_pizza_sales[5];
    int large_pizza_sales[5];
    double total_earnings;
    int fire_signal;
    int end_of_day;
    int evacuation;

    int table_occupancy[MAX_TABLES];
    char group_at_table[MAX_TABLES][MAX_GROUPS_PER_TABLE][MAX_GROUP_NAME_SIZE];
    int group_count_at_table[MAX_TABLES];
    int group_sizes_at_table[MAX_TABLES][MAX_GROUPS_PER_TABLE];
    pid_t client_pids[MAX_CLIENTS];
    int client_count;

    SharedQueues queues; // Kolejki priorytetowe
} shared_data;

// Deklaracje zmiennych globalnych
extern struct shared_data *shm_data;// Wskaźnik do danych współdzielonych
extern int sem_id;
extern int total_tables;
extern int table_sizes[MAX_TABLES];
extern volatile int sem_removed;
extern volatile int memory_removed;
volatile sig_atomic_t cleaning_in_progress;
void setup_shared_memory_and_semaphores();
void lock_semaphore();
void unlock_semaphore();
void cleanup_shared_memory_and_semaphores();
void signal_handler(int signum);
#endif // GLOBALS_H