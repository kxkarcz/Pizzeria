#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <signal.h> 
#include "globals.h"

struct shared_data *shm_data = NULL;
int sem_id = -1;
volatile int force_end_day = 0;
volatile int sem_removed = 0;
volatile int memory_removed = 0;
volatile sig_atomic_t cleaning_in_progress = 0;

void cleanup_shared_memory_and_semaphores() {
    lock_semaphore();
    if (shm_data != NULL && shm_data->cleanup_done) {
        unlock_semaphore();
        return;
    }
    if (!memory_removed) {
        int shm_id = shmget(SHM_KEY, sizeof(struct shared_data), 0666);
        if (shm_id != -1) {
            if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
                perror("Błąd przy usuwaniu pamięci współdzielonej");
            } else {
                printf("Pamięć współdzielona usunięta\n");
                memory_removed = 1;
            }
        } else {
            printf("Pamięć współdzielona już nie istnieje lub nie można jej odnaleźć.\n");
        }
    }
    if (!sem_removed && sem_id != -1) {
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            perror("Błąd przy usuwaniu semaforów");
        } else {
            printf("Semafory usunięte\n");
            sem_removed = 1;
        }
    }
    if (shm_data != NULL) {
        shm_data->cleanup_done = 1;
    }
    unlock_semaphore();
}

void terminate_all_processes() {
    if (shm_data != NULL) {
        for (int i = 0; i < shm_data->client_count; i++) {
            if (shm_data->client_pids[i] > 0) {
                printf("[System] Wysyłam SIGTERM do procesu PID: %d\n", shm_data->client_pids[i]);
                kill(shm_data->client_pids[i], SIGTERM);
                shm_data->client_pids[i] = -1; // Oznacz jako usunięty
            }
        }
    }
}
void remove_pid_from_list(pid_t pid) {
    if (shm_data != NULL) {
        for (int i = 0; i < shm_data->client_count; i++) {
            if (shm_data->client_pids[i] == pid) {
                shm_data->client_pids[i] = -1; // Oznacz jako usunięty
                break;
            }
        }
    }
}
void signal_handler(int signo) {
    printf("Otrzymano sygnał: %d, rozpoczynam czyszczenie...\n", signo);
    terminate_all_processes();
    cleanup_shared_memory_and_semaphores();
    exit(0);
}

void setup_shared_memory_and_semaphores() {
    int shm_id = shmget(SHM_KEY, sizeof(struct shared_data), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Błąd przy tworzeniu pamięci współdzielonej");
        exit(EXIT_FAILURE);
    }

    shm_data = (struct shared_data *)shmat(shm_id, NULL, 0);
    if (shm_data == (void *)-1) {
        perror("Błąd przy dołączaniu pamięci współdzielonej");
        exit(EXIT_FAILURE);
    }

    memset(shm_data, 0, sizeof(struct shared_data));

    shm_data->queues.small_groups.queue_front = 0;
    shm_data->queues.small_groups.queue_rear = 0;
    shm_data->queues.large_groups.queue_front = 0;
    shm_data->queues.large_groups.queue_rear = 0;

    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Błąd przy tworzeniu semaforów");
        exit(EXIT_FAILURE);
    }

    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Błąd przy inicjalizacji semafora");
        exit(EXIT_FAILURE);
    }
}

void lock_semaphore() {
    if (sem_removed) {
        return;
    }

    if (sem_id == -1) {
        fprintf(stderr, "[Error] Nieprawidłowy identyfikator semafora (sem_id == -1). Blokowanie przerwane.\n");
        return;
    }

    struct sembuf sops = {0, -1, 0};
    if (semop(sem_id, &sops, 1) == -1) {
        if (errno == EINVAL || errno == EIDRM) {
            sem_removed = 1;
        } else {
            perror("[Error] Nieoczekiwany błąd podczas blokowania semafora");
        }
    }
}

void unlock_semaphore() {
    if (sem_removed) {
        return;
    }

    if (sem_id == -1) {
        fprintf(stderr, "[Error] Nieprawidłowy identyfikator semafora (sem_id == -1). Odblokowywanie przerwane.\n");
        return;
    }

    struct sembuf sops = {0, 1, 0};
    if (semop(sem_id, &sops, 1) == -1) {
        if (errno == EINVAL || errno == EIDRM) {
            sem_removed = 1;
        } else {
            perror("[Error] Nieoczekiwany błąd podczas odblokowywania semafora");
        }
    }
}
