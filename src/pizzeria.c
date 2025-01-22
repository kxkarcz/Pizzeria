#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "globals.h"
#include "pizza.h"
#include "client.h"
#include "logging.h"

int read_last_day() {
    int file = open("podsumowania/last_day.txt", O_RDONLY);
    int last_day = 0;
    if (file != -1) {
        char buffer[10] = {0};
        ssize_t bytes_read = read(file, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            last_day = atoi(buffer);
        }
        close(file);
    }
    return last_day;
}

void save_last_day(int day) {
    struct stat st = {0};
    if (stat("podsumowania", &st) == -1) {
        if (mkdir("podsumowania", 0700) == -1) {
            perror("Błąd przy tworzeniu katalogu (mkdir)");
            return;
        }
    }
    int file = open("podsumowania/last_day.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        perror("Błąd przy otwieraniu pliku (open)");
        return;
    }
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%d", day);
    write(file, buffer, strlen(buffer));
    close(file);
}

void display_and_save_summary(int day) {
    char filename[100];
    snprintf(filename, sizeof(filename), "podsumowania/podsumowanie_dnia_%d.txt", day);
    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        perror("Błąd przy otwieraniu pliku (open)");
        return;
    }

    log_message("---- Dzień %d Podsumowanie ----\n", day);
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "---- Dzień %d Podsumowanie ----\n", day);
    write(file, buffer, strlen(buffer));

    for (int i = 0; i < 5; i++) {
        double small_earnings = shm_data->small_pizza_sales[i] * menu[i].small_price;
        double large_earnings = shm_data->large_pizza_sales[i] * menu[i].large_price;
        double small_costs = small_earnings * 0.7;
        double large_costs = large_earnings * 0.7;
        double small_profit = small_earnings * 0.3;
        double large_profit = large_earnings * 0.3;

        log_message("%s:\n", menu[i].name);
        log_message("- Mała - %d, Zarobek - %.2f, Koszt - %.2f\n", shm_data->small_pizza_sales[i], small_profit, small_costs);
        log_message("- Duża - %d, Zarobek - %.2f, Koszt - %.2f\n", shm_data->large_pizza_sales[i], large_profit, large_costs);

        snprintf(buffer, sizeof(buffer),
                 "%s:\n- Mała - %d, Zarobek - %.2f, Koszt - %.2f\n- Duża - %d, Zarobek - %.2f, Koszt - %.2f\n",
                 menu[i].name, shm_data->small_pizza_sales[i], small_profit, small_costs,
                 shm_data->large_pizza_sales[i], large_profit, large_costs);
        write(file, buffer, strlen(buffer));
    }

    double total_profit = shm_data->total_earnings * 0.5;
    double total_costs = shm_data->total_earnings * 0.5;
    log_message("Całkowity Zarobek: %.2f\n", total_profit);
    log_message("Całkowity Koszt: %.2f\n", total_costs);

    snprintf(buffer, sizeof(buffer), "Całkowity Zarobek: %.2f\nCałkowity Koszt: %.2f\n", total_profit, total_costs);
    write(file, buffer, strlen(buffer));

    close(file);
}

void* end_of_day_timer(void* arg) {
    int timer = *(int*)arg;
    log_message("[Pizzeria] Timer końca dnia ustawiony na %d sekund.\n", timer);
    while (timer > 0) {
        sleep(1);
        timer--;

        lock_semaphore();
        if (shm_data->evacuation) {
            unlock_semaphore();
            log_message("[Pizzeria] Timer końca dnia przerwany z powodu ewakuacji.\n");
            pthread_exit(NULL);
        }
        unlock_semaphore();
    }

    log_message("[Pizzeria] Koniec dnia, zamykamy pizzerię.\n");
    lock_semaphore();
    shm_data->end_of_day = 1;
    unlock_semaphore();
    pthread_exit(NULL);
}

int can_create_new_process() {
    lock_semaphore();
    int current_clients = shm_data->client_count;
    unlock_semaphore();

    if (current_clients >= MAX_CLIENTS) {
        return 0;
    }

    return 1;
}
void add_client_pid(pid_t pid) {
    lock_semaphore();
    if (shm_data != NULL && shm_data->client_count < MAX_CLIENTS) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (shm_data->client_pids[i] == -1 || shm_data->client_pids[i] == 0) {
                shm_data->client_pids[i] = pid;
                shm_data->client_count++;
                break;
            }
        }
    }
    unlock_semaphore();
}

void remove_client_pid(pid_t pid) {
    lock_semaphore();
    if (shm_data != NULL) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (shm_data->client_pids[i] == pid) {
                shm_data->client_pids[i] = -1;
                shm_data->client_count--;
                break;
            }
        }
    }
    unlock_semaphore();
}
void handle_terminated_processes() {
    int status;
    pid_t child_pid;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_client_pid(child_pid);
        log_message("[Pizzeria] Proces klienta (PID: %d) zakończył się.\n", child_pid);
    }
}


void simulate_day(int day) {
    atexit(handle_terminated_processes);
    log_message("---- Dzień %d ----\n", day);
    while (!shm_data->end_of_day) {
        lock_semaphore();
        if (shm_data->end_of_day) {
            log_message("[Pizzeria] Dzień zakończony. Przerywam symulację dnia.\n");
            unlock_semaphore();
            break;
        }
        unlock_semaphore();

        // Obsługa zakończonych procesów klientów
        handle_terminated_processes();

        // Tworzenie nowych klientów
        if (can_create_new_process() && !shm_data->end_of_day) {
            int group_size = rand() % 3 + 1;
            pid_t pid = fork();

            if (pid == 0) {
                client_function(group_size);
                exit(0);
            } else if (pid > 0) {
                add_client_pid(pid);
            } else {
                perror("[Pizzeria] Błąd przy tworzeniu procesu klienta");
            }
        }

        usleep(500000);
    }

    lock_semaphore();
    shm_data->end_of_day = 1;
    unlock_semaphore();

    log_message("[Pizzeria] Czekam na zakończenie wszystkich procesów klientów...\n");
    while (shm_data->client_count > 0) {
        handle_terminated_processes();
    }

    log_message("[Pizzeria] Wszystkie procesy zakończone. Dziękujemy za dziś!\n");
}


