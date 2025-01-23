#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "globals.h"
#include "logging.h"

static void handle_fire_signal(int signo) {
    if (lock_semaphore() == -1) {
        perror("[ERROR] Nie udało się zablokować semafora");
        return;
    }
    if (shm_data == NULL || sem_removed) {
        if (unlock_semaphore() == -1) {
            perror("[ERROR] Nie udało się odblokować semafora");
        }
        log_message("[Strażak] Pamięć współdzielona lub semafory nie są dostępne. Ignoruję sygnał pożarowy.\n");
        return;
    }

    if (shm_data->fire_signal) {
        log_message("[Strażak] Sygnał pożarowy już został obsłużony. Ignoruję kolejny sygnał.\n");
        if (unlock_semaphore() == -1) {
            perror("[ERROR] Nie udało się odblokować semafora");
        }
        return;
    }

    log_message("[Strażak] Sygnał pożarowy odebrany! Rozpoczynam ewakuację.\n");
    shm_data->fire_signal = 1;
    shm_data->evacuation = 1;
    shm_data->end_of_day = 1;

    log_message("[Strażak] Wysyłam sygnał SIGUSR1 do wszystkich procesów potomnych.\n");
    if (kill(-getpgrp(), SIGUSR1) == -1) {
        perror("[Strażak] Nie udało się wysłać sygnału SIGUSR1");
    }

    shm_data->queues.queue.queue_front = 0;
    shm_data->queues.queue.queue_rear = 0;

    if (unlock_semaphore() == -1) {
        perror("[ERROR] Nie udało się odblokować semafora");
    }
}

void firefighter_process() {
    log_message("[Strażak] Proces strażaka uruchomiony. PID: %d. Oczekuję na sygnały.\n", getpid());

    // Ustawienie obsługi sygnału
    struct sigaction sa;
    sa.sa_handler = handle_fire_signal;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    while (1) {
        pause();

        while (waitpid(-1, NULL, WNOHANG) > 0) {
        }

        if (sem_removed) {
            log_message("[Strażak] Semafor został usunięty. Zamykam proces strażaka.\n");
            break;
        }

        if (shm_data == NULL) {
            log_message("[Strażak] Pamięć współdzielona nie istnieje. Zamykam proces strażaka.\n");
            break;
        }

        if (lock_semaphore() == -1) {
            perror("[ERROR] Nie udało się zablokować semafora");
            break;
        }
        if (shm_data->end_of_day) {
            log_message("[Strażak] Koniec dnia, zamykam proces strażaka.\n");

            log_message("[Strażak] Wysyłam SIGTERM do wszystkich procesów potomnych.\n");
            if (kill(-getpgrp(), SIGTERM) == -1) {
                perror("[Strażak] Nie udało się wysłać SIGTERM");
            }
            if (unlock_semaphore() == -1) {
                perror("[ERROR] Nie udało się odblokować semafora");
            }
            break;
        }
        if (unlock_semaphore() == -1) {
            perror("[ERROR] Nie udało się odblokować semafora");
        }
    }
}