#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "globals.h"

static void handle_fire_signal(int signum) {
    lock_semaphore();
    if (shm_data->fire_signal) {
        printf("[Strażak] Sygnał pożarowy już został obsłużony. Ignoruję kolejny sygnał.\n");
        unlock_semaphore();
        return;
    }

    printf("[Strażak] Sygnał pożarowy odebrany! Rozpoczynam ewakuację.\n");
    shm_data->fire_signal = 1;
    shm_data->evacuation = 1;
    shm_data->end_of_day = 1;

    // Rozsyłanie sygnału do klientów
    for (int i = 0; i < shm_data->client_count; i++) {
        pid_t client_pid = shm_data->client_pids[i];
        if (client_pid > 0) {
            printf("[Strażak] Wysyłam sygnał do procesu PID %d.\n", client_pid);
            if (kill(client_pid, SIGUSR1) == -1) {
                perror("[Strażak] Nie udało się wysłać sygnału");
            }
        }
    }

    // Oczyszczenie kolejek
    shm_data->queues.small_groups.queue_front = shm_data->queues.small_groups.queue_rear = 0;
    shm_data->queues.large_groups.queue_front = shm_data->queues.large_groups.queue_rear = 0;

    unlock_semaphore();
}

void firefighter_process() {
    printf("[Strażak] Proces strażaka uruchomiony. PID: %d. Oczekuję na sygnały.\n", getpid());
    signal(SIGUSR1, handle_fire_signal);

    while (1) {
        pause();
        if (sem_removed) {
            printf("[Strażak] Semafor został usunięty. Zamykam proces strażaka.\n");
            break;
        }

        if (shm_data == NULL) {
            printf("[Strażak] Pamięć współdzielona nie istnieje. Zamykam proces strażaka.\n");
            break;
        }

        lock_semaphore();
        if (shm_data->end_of_day) {
            printf("[Strażak] Koniec dnia, zamykam proces strażaka.\n");

            // Wysyłanie sygnału końca dnia do klientów
            for (int i = 0; i < shm_data->client_count; i++) {
                pid_t client_pid = shm_data->client_pids[i];
                if (client_pid > 0) {
                    if (kill(client_pid, SIGTERM) == -1) {
                        perror("[Strażak] Nie udało się wysłać sygnału końca dnia");
                    }
                }
            }
            unlock_semaphore();
            break;
        }
        unlock_semaphore();
    }
}