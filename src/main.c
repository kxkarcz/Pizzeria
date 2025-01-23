#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "globals.h"
#include "pizzeria.h"
#include "client.h"
#include "tables.h"
#include "boss.h"
#include "firefighter.h"
#include "logging.h"
#include "tests.h"

// Funkcja wątku szefa
void* boss_thread_function(void* arg) {
    log_message("[Szef] Rozpoczynam pracę.\n");

    while (1) {
        lock_semaphore();
        int end_of_day = shm_data->end_of_day;
        unlock_semaphore();

        if (end_of_day) {
            log_message("[Szef] Otrzymano sygnał końca dnia. Przerywam obsługę.\n");
            break;
        }

        if (!is_queue_empty(&shm_data->queues.queue)) {
            handle_queue();
        }
    }

    lock_semaphore();
    if (!is_queue_empty(&shm_data->queues.queue)) {
        while (!is_queue_empty(&shm_data->queues.queue)) {
            QueueEntry entry = remove_from_queue(&shm_data->queues.queue);
        }
    }
    unlock_semaphore();

    log_message("[Szef] Koniec dnia. Zamykam pizzerię.\n");
    return NULL;
}

// Obsługa sygnału SIGCHLD do zapobiegania procesom zombie
void sigchld_handler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Usuwamy procesy zombie
    }
}

void setup_signal_handlers() {
    struct sigaction sa;

    // Obsługa SIGCHLD
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    // Obsługa SIGINT i SIGTERM
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    log_message("[Pizzeria] Handlery sygnałów zostały poprawnie zarejestrowane.\n");
}

int main() {
    setup_signal_handlers();
    initialize_logging();
    setup_shared_memory_and_semaphores();
    configure_tables(0, 0, 0, 5);
    initialize_tables();
    initialize_menu();

    pid_t firefighter_pid = fork();
    if (firefighter_pid == 0) {
        firefighter_process();
        exit(0);
    } else if (firefighter_pid < 0) {
        perror("[Pizzeria] Błąd przy uruchamianiu strażaka");
        exit(EXIT_FAILURE);
    }

    log_message("[Pizzeria] Strażak uruchomiony. PID: %d.\n", firefighter_pid);

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
    int timer_duration = 30;
    static int timer_started = 0;

    if (!timer_started) {
        timer_started = 1;
        if (pthread_create(&timer_thread, NULL, end_of_day_timer, &timer_duration) != 0) {
            perror("Błąd przy tworzeniu wątku timera dnia");
            exit(EXIT_FAILURE);
        }
    } else {
        log_message("[Pizzeria] Timer dnia już działa, nie tworzę kolejnego.\n");
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

    if (!shm_data->summary_done) {
        display_and_save_summary(current_day);
        shm_data->summary_done = 1;
    }
    sleep(1);
    cleanup_shared_memory_and_semaphores();
    log_message("[Pizzeria] Wszystkie procesy zakończone. Dziękujemy za dziś!\n");
    close_log();

    return 0;
}