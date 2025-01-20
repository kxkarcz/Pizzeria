#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include "globals.h"
#include "pizzeria.h"
#include "client.h"
#include "tables.h"
#include "boss.h"
#include "firefighter.h"
#include "logging.h"


// Funkcja wątku szefa
void* boss_thread_function(void* arg) {
    log_message("[Szef] Rozpoczynam pracę.\n");

    while (!shm_data->end_of_day) {
        handle_queue();
        usleep(500000); // 0.5 sekundy
    }

    log_message("[Szef] Koniec dnia. Zamykam pizzerię.\n");
    return NULL;
}
void handle_sigchld(int signum) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_client_pid(pid);
        log_message("[Pizzeria] Proces klienta (PID: %d) zakończył się.\n", pid);
    }
}

void handle_termination_signal(int signum) {
    log_message("[Pizzeria] Otrzymano sygnał %d. Kończę pracę.\n", signum);

    lock_semaphore();
    shm_data->end_of_day = 1; // Informujemy o końcu dnia
    unlock_semaphore();

    // Zakończenie procesów klientów
    lock_semaphore();
    for (int i = 0; i < shm_data->client_count; i++) {
        pid_t client_pid = shm_data->client_pids[i];
        if (client_pid > 0) {
            kill(client_pid, SIGTERM);
            waitpid(client_pid, NULL, 0);
            shm_data->client_pids[i] = -1;
        }
    }
    unlock_semaphore();

    // Zakończenie procesu strażaka
    if (shm_data->fire_signal > 0) {
        kill(shm_data->fire_signal, SIGTERM);
        waitpid(shm_data->fire_signal, NULL, 0);
    }

    cleanup_shared_memory_and_semaphores();
    exit(EXIT_SUCCESS);
}

void setup_signal_handlers() {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Automatyczne wznawianie operacji
    sigaction(SIGCHLD, &sa, NULL);

    // Obsługa SIGINT i SIGTERM
    sa.sa_handler = handle_termination_signal;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main() {
    setup_signal_handlers();
    initialize_logging();
    setup_shared_memory_and_semaphores();
    configure_tables(10, 10, 10, 10);
    initialize_tables();
    initialize_menu();


    // Uruchomienie procesu strażaka
    pid_t firefighter_pid = fork();
    if (firefighter_pid == 0) {
        firefighter_process();
        exit(0);
    } else if (firefighter_pid < 0) {
        perror("[Pizzeria] Błąd przy uruchamianiu strażaka");
        exit(EXIT_FAILURE);
    }
    log_message("[Pizzeria] Strażak uruchomiony. PID: %d. Wyślij sygnał pożarowy 'kill -USR1 %d'.\n", firefighter_pid, firefighter_pid);

    int current_day = read_last_day() + 1;
    save_last_day(current_day);
    log_message("Rozpoczynamy dzień %d w pizzerii!\n", current_day);

    pthread_t boss_thread, timer_thread, day_thread;

    // Wątek szefa
    if (pthread_create(&boss_thread, NULL, boss_thread_function, NULL) != 0) {
        perror("Błąd przy tworzeniu wątku szefa");
        exit(EXIT_FAILURE);
    }

    // Wątek timera
    int timer_duration = 10;//28800; // Czas dnia w sekundach
    if (pthread_create(&timer_thread, NULL, end_of_day_timer, &timer_duration) != 0) {
        perror("Błąd przy tworzeniu wątku timera dnia");
        exit(EXIT_FAILURE);
    }

    // Wątek symulacji dnia
    if (pthread_create(&day_thread, NULL, (void *(*)(void *))simulate_day, (void *)(long)current_day) != 0) {
        perror("Błąd przy tworzeniu wątku dnia");
        exit(EXIT_FAILURE);
    }

    pthread_join(day_thread, NULL);
    pthread_join(boss_thread, NULL);
    pthread_join(timer_thread, NULL);

    // Zakończenie procesu strażaka
    if (firefighter_pid > 0) {
        kill(firefighter_pid, SIGTERM);
        waitpid(firefighter_pid, NULL, 0);
    }

    // Zakończenie procesów klientów
    lock_semaphore();
    for (int i = 0; i < shm_data->client_count; i++) {
        pid_t client_pid = shm_data->client_pids[i];
        if (client_pid > 0) {
            kill(client_pid, SIGTERM);
            waitpid(client_pid, NULL, 0);
        }
    }
    unlock_semaphore();
    display_and_save_summary(current_day);
    terminate_all_processes();
    cleanup_shared_memory_and_semaphores();
    log_message("[Pizzeria] Wszystkie procesy zakończone. Dziękujemy za dziś!\n");
    close_log();
    return 0;
}