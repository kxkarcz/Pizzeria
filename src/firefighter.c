#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "globals.h"
#include "logging.h"

/**
 * @brief Obsługuje sygnał pożarowy i rozpoczyna ewakuację.
 *
 * @param signum Numer sygnału (SIGUSR1).
 */
static void handle_fire_signal(int signum) {
    lock_semaphore();
    if (shm_data->fire_signal) {
        log_message("[Strażak] Sygnał pożarowy już został obsłużony. Ignoruję kolejny sygnał.\n");
        unlock_semaphore();
        return;
    }

    log_message("[Strażak] Sygnał pożarowy odebrany! Rozpoczynam ewakuację.\n");
    shm_data->fire_signal = 1;
    shm_data->evacuation = 1;
    shm_data->end_of_day = 1;

    // Rozsyłanie sygnału do klientów
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pid_t client_pid = shm_data->client_pids[i];
        if (client_pid > 0) {
            log_message("[Strażak] Wysyłam sygnał SIGUSR1 do procesu PID %d.\n", client_pid);
            if (kill(client_pid, SIGUSR1) == -1) {
                perror("[Strażak] Nie udało się wysłać sygnału SIGUSR1");
            }
        }
    }

    // Oczyszczenie kolejek
    shm_data->queues.queue.queue_front = shm_data->queues.queue.queue_rear = 0;

    unlock_semaphore();
}

/**
 * @brief Główna funkcja procesu strażaka.
 */
void firefighter_process() {
    log_message("[Strażak] Proces strażaka uruchomiony. PID: %d. Oczekuję na sygnały.\n", getpid());
    signal(SIGUSR1, handle_fire_signal);

    while (1) {
        pause();  // Oczekiwanie na sygnały

        if (sem_removed) {
            log_message("[Strażak] Semafor został usunięty. Zamykam proces strażaka.\n");
            break;
        }

        if (shm_data == NULL) {
            log_message("[Strażak] Pamięć współdzielona nie istnieje. Zamykam proces strażaka.\n");
            break;
        }

        lock_semaphore();
        if (shm_data->end_of_day) {
            log_message("[Strażak] Koniec dnia, zamykam proces strażaka.\n");

            // Wysyłanie SIGTERM do klientów
            for (int i = 0; i < MAX_CLIENTS; i++) {
                pid_t client_pid = shm_data->client_pids[i];
                if (client_pid > 0) {
                    log_message("[Strażak] Wysyłam SIGTERM do procesu PID %d.\n", client_pid);
                    kill(client_pid, SIGTERM);
                }
            }

            log_message("[Strażak] Oczekiwanie na zakończenie procesów klientów...\n");
            while (shm_data->current_clients > 0) {
                unlock_semaphore();
                usleep(100000);  // Czas oczekiwania
                lock_semaphore();
            }

            log_message("[Strażak] Wszystkie procesy klientów zakończone.\n");

            // Wysyłanie SIGINT do procesu głównego pizzerii
            pid_t pizzeria_pid = getppid();  // Pobranie PID procesu rodzica
            log_message("[Strażak] Wysyłam SIGINT do procesu głównego PID %d.\n", pizzeria_pid);
            if (kill(pizzeria_pid, SIGINT) == -1) {
                perror("[Strażak] Nie udało się wysłać sygnału SIGINT");
            }

            unlock_semaphore();
            break;
        }
        unlock_semaphore();
    }
}