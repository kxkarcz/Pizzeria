#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "globals.h"
#include "logging.h"
#include "pizzeria.h"

// Globalne zmienne
struct shared_data *shm_data = NULL;
int sem_id = -1;
volatile int sem_removed = 0;
volatile int memory_removed = 0;
volatile sig_atomic_t cleaning_in_progress = 0;

/**
 * @brief Czyści pamięć współdzieloną i usuwa semafory.
 */
void cleanup_shared_memory_and_semaphores() {
    if (cleaning_in_progress) {
        return;
    }
    cleaning_in_progress = 1;

    while (shm_data && shm_data->current_clients > 0) {
        usleep(100000);
    }

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
        } else if (errno == ENOENT) {
            memory_removed = 1;
        }
    }

    if (!sem_removed && sem_id != -1) {
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            if (errno != EINVAL) {
                perror("Błąd przy usuwaniu semaforów");
            }
        } else {
            sem_removed = 1;
        }
        sem_id = -1;
    }
}

/**
 * @brief Obsługuje sygnały systemowe.
 *
 * @param signo Numer sygnału.
 */
void signal_handler(int signo) {
    if (lock_semaphore() == -1) {
        perror("[ERROR] Nie udało się zablokować semafora");
        return;
    }

    if (signo == SIGINT || signo == SIGTERM) {
        log_message("[Pizzeria] Otrzymano sygnał %s. Kończenie programu...\n",
                    signo == SIGINT ? "SIGINT" : "SIGTERM");
        shm_data->end_of_day = 1;
    }

    if (unlock_semaphore() == -1) {
        perror("[ERROR] Nie udało się odblokować semafora");
    }

    while (wait(NULL) > 0);

    if (!shm_data->summary_done) {
        int current_day = read_last_day() + 1;
        display_and_save_summary(current_day);
        shm_data->summary_done = 1;
    }
    cleanup_shared_memory_and_semaphores();
    exit(0);
}

/**
 * @brief Inicjalizuje pamięć współdzieloną i semafory.
 */
void setup_shared_memory_and_semaphores() {
    int shm_id = shmget(SHM_KEY, sizeof(struct shared_data), IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id == -1) {
        if (errno == EEXIST) {
            shm_id = shmget(SHM_KEY, sizeof(struct shared_data), 0666);
            if (shm_id != -1) {
                if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
                    perror("Błąd przy usuwaniu istniejącej pamięci współdzielonej");
                    exit(EXIT_FAILURE);
                }
            }

            shm_id = shmget(SHM_KEY, sizeof(struct shared_data), IPC_CREAT | 0666);
            if (shm_id == -1) {
                perror("Błąd przy ponownym tworzeniu pamięci współdzielonej");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Błąd przy tworzeniu pamięci współdzielonej");
            exit(EXIT_FAILURE);
        }
    }

    shm_data = (struct shared_data *)shmat(shm_id, NULL, 0);
    if (shm_data == (void *)-1) {
        perror("Błąd przy dołączaniu pamięci współdzielonej");
        exit(EXIT_FAILURE);
    }

    memset(shm_data, 0, sizeof(struct shared_data));
    memset(shm_data->client_pids, 0, sizeof(shm_data->client_pids));
    shm_data->end_of_day = 0;
    shm_data->current_clients = 0;
    shm_data->summary_done = 0;

    sem_id = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id == -1) {
        if (errno == EEXIST) {
            sem_id = semget(SEM_KEY, 1, 0666);
            if (sem_id != -1) {
                if (semctl(sem_id, 0, IPC_RMID) == -1) {
                    perror("Błąd przy usuwaniu istniejących semaforów");
                    exit(EXIT_FAILURE);
                }
            }

            sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
            if (sem_id == -1) {
                perror("Błąd przy ponownym tworzeniu semaforów");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Błąd przy tworzeniu semaforów");
            exit(EXIT_FAILURE);
        }
    }

    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Błąd przy inicjalizacji semafora");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Blokuje semafor.
 *
 * @return int Zwraca 0 w przypadku sukcesu, -1 w przypadku błędu.
 */
int lock_semaphore() {
    if (sem_removed) {
        log_message("[ERROR] Semafor został usunięty.\n");
        return -1;
    }

    if (sem_id == -1) {
        log_message("[ERROR] Semafor nie jest dostępny.\n");
        return -1;
    }

    struct sembuf sops = {0, -1, 0};
    int max_attempts = 10;
    int attempts = 0;

    while (semop(sem_id, &sops, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        if (errno == EINVAL || errno == EIDRM) {
            sem_removed = 1;
            log_message("[ERROR] Semafor został usunięty lub jest nieważny.\n");
            return -1;
        }

        attempts++;
        log_message("[ERROR] Nie udało się zablokować semafora. Próba %d.\n", attempts);
        if (attempts >= max_attempts) {
            errno = ETIMEDOUT;
            return -1;
        }
        struct timespec ts = {0, 100000000}; // 100 ms
        nanosleep(&ts, NULL);
    }

    return 0;
}

/**
 * @brief Odblokowuje semafor.
 *
 * @return int Zwraca 0 w przypadku sukcesu, -1 w przypadku błędu.
 */
int unlock_semaphore() {
    if (sem_removed) {
        log_message("[ERROR] Próba odblokowania usuniętego semafora.\n");
        return -1;
    }

    if (sem_id == -1) {
        log_message("[ERROR] Semafor nie jest dostępny.\n");
        return -1;
    }

    struct sembuf sops = {0, 1, 0};
    while (semop(sem_id, &sops, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        if (errno == EINVAL || errno == EIDRM) {
            sem_removed = 1;
            log_message("[ERROR] Semafor został usunięty lub jest nieważny podczas odblokowywania.\n");
            return -1;
        }
    }

    return 0;
}