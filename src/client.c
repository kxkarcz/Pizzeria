#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "globals.h"
#include "boss.h"
#include "logging.h"

/**
 * @brief Funkcja wywoływana podczas opuszczania lokalu przez klienta.
 */
void client_exit() {
    static int exited = 0;
    if (exited) return;
    exited = 1;

    lock_semaphore();
    if (shm_data->current_clients > 0) {
        shm_data->current_clients--;
        log_message("[Klient] Grupa o PID %d opuszcza lokal. Liczba klientów: %d\n", getpid(), shm_data->current_clients);
    } else {
        log_message("[ERROR] Próba zmniejszenia liczby klientów poniżej 0. PID: %d\n", getpid());
    }
    unlock_semaphore();

    exit(0);
}

/**
 * @brief Obsługa sygnału pożarowego dla klienta.
 */
void handle_fire_signal() {
    log_message("[Klient] Grupa o PID %d otrzymała sygnał pożarowy! Opuszczam lokal.\n", getpid());
    client_exit();
}

/**
 * @brief Funkcja obsługująca sygnały dla klienta.
 *
 * @param signo Numer odebranego sygnału.
 */
void client_signal_handler(int signo) {
    log_message("[Klient] Grupa o PID %d otrzymała sygnał %d. Kończę działanie...\n", getpid(), signo);
    client_exit();
}

/**
 * @brief Główna funkcja obsługi klienta.
 *
 * @param group_size Rozmiar grupy.
 */
void client_function(int group_size) {
    // Ustawienie obsługi sygnałów
    signal(SIGUSR1, handle_fire_signal);
    signal(SIGTERM, client_signal_handler);
    signal(SIGINT, client_signal_handler);

    lock_semaphore();

    // Sprawdzanie limitów klientów
    if (!shm_data || sem_removed || shm_data->current_clients >= MAX_CLIENTS) {
        unlock_semaphore();
        log_message("[Klient] Limit klientów osiągnięty. PID: %d\n", getpid());
        client_exit();
    }

    if (shm_data->end_of_day) {
        unlock_semaphore();
        log_message("[Klient] Dzień zakończony. PID: %d\n", getpid());
        client_exit();
    }

    // Rejestracja klienta
    shm_data->client_pids[shm_data->current_clients] = getpid();
    shm_data->current_clients++;
    log_message("[Klient] Grupa o PID %d dołączyła do kolejki. Liczba klientów: %d\n", getpid(), shm_data->current_clients);
    unlock_semaphore();

    char group_name[MAX_GROUP_NAME_SIZE];
    snprintf(group_name, MAX_GROUP_NAME_SIZE, "Grupa_%d", getpid());

    lock_semaphore();
    if (shm_data->end_of_day) {
        shm_data->current_clients--;
        log_message("[Klient] Dzień zakończony przed przydzieleniem stolika. PID: %d\n", getpid());
        unlock_semaphore();
        client_exit();
    }

    // Dodanie grupy do kolejki
    add_to_priority_queue(&shm_data->queues.queue, group_size, group_name, 0);
    unlock_semaphore();

    int table_assigned = -1;
    while (1) {
        lock_semaphore();
        if (shm_data->end_of_day) {
            log_message("[Klient] Dzień zakończony podczas oczekiwania na stolik. PID: %d\n", getpid());
            unlock_semaphore();
            client_exit();
        }

        // Sprawdzanie, czy stolik został przydzielony
        for (int i = 0; i < total_tables; i++) {
            for (int j = 0; j < shm_data->group_count_at_table[i]; j++) {
                if (strcmp(shm_data->group_at_table[i][j], group_name) == 0) {
                    table_assigned = i;
                    break;
                }
            }
            if (table_assigned != -1) {
                break;
            }
        }
        unlock_semaphore();

        if (table_assigned != -1) {
            lock_semaphore();
            log_message("[Klient] Grupa o PID %d usiadła przy stoliku %d.\n", getpid(), table_assigned);
            unlock_semaphore();
            break;
        }

        usleep(1000);
    }

    // Symulacja czasu spędzonego przy stoliku
    if (table_assigned != -1) {
        sleep(rand() % 5 + 5);

        lock_semaphore();
        for (int i = 0; i < shm_data->group_count_at_table[table_assigned]; i++) {
            if (strcmp(shm_data->group_at_table[table_assigned][i], group_name) == 0) {
                shm_data->table_occupancy[table_assigned] -= group_size;

                // Usuwanie grupy ze stolika
                for (int k = i; k < shm_data->group_count_at_table[table_assigned] - 1; k++) {
                    shm_data->group_sizes_at_table[table_assigned][k] = shm_data->group_sizes_at_table[table_assigned][k + 1];
                    strncpy(shm_data->group_at_table[table_assigned][k], shm_data->group_at_table[table_assigned][k + 1], MAX_GROUP_NAME_SIZE);
                }
                shm_data->group_count_at_table[table_assigned]--;

                // Resetowanie danych stolika, jeśli jest pusty
                if (shm_data->table_occupancy[table_assigned] == 0) {
                    memset(shm_data->group_at_table[table_assigned], 0, sizeof(shm_data->group_at_table[table_assigned]));
                    shm_data->group_count_at_table[table_assigned] = 0;
                }
                break;
            }
        }

        log_message("[Klient] Grupa o PID %d zwalnia stolik i wychodzi z lokalu.\n", getpid());
        unlock_semaphore();
        client_exit();
    }
}