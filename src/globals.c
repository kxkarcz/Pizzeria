#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include "globals.h"

struct shared_data *shm_data = NULL;
int sem_id = -1;
volatile int force_end_day = 0;
volatile int sem_removed = 0;
volatile int memory_removed = 0;

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

void cleanup_shared_memory_and_semaphores() {
    if (!memory_removed) {
        if (shm_data != NULL) {
            if (shmdt(shm_data) == -1) {
                perror("Błąd przy odłączaniu pamięci współdzielonej");
            }
            shm_data = NULL;
        }

        int shm_id = shmget(SHM_KEY, sizeof(struct shared_data), 0666);
        if (shm_id != -1) {
            if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
                perror("Błąd przy usuwaniu pamięci współdzielonej");
            } else {
                memory_removed = 1;
            }
        }
    }

    if (!sem_removed && sem_id != -1) {
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            perror("Błąd przy usuwaniu semaforów");
        } else {
            sem_removed = 1;
        }
        sem_id = -1;
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

    int sem_val = semctl(sem_id, 0, GETVAL);
    if (sem_val == -1) {
        if (errno == EINVAL || errno == EIDRM) {
            sem_removed = 1;
            return;
        }
        perror("[Error] Nie udało się pobrać wartości semafora przed blokowaniem");
        return;
    }

    struct sembuf sops = {0, -1, 0};
    if (semop(sem_id, &sops, 1) == -1) {
        if (errno == EINVAL || errno == EIDRM) {
            sem_removed = 1;
        } else {
            perror("[Error] Nieoczekiwany błąd podczas blokowania semafora");
        }
        return;
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
