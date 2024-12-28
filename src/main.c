#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "tables.h"
#include "globals.h"
#include "firefighter.h"
#include "pizza.h"
#include "logging.h"

FILE* log_file;

void* client_function(void* arg);

int read_last_day() {
    FILE* file = fopen("podsumowania/last_day.txt", "r");
    int last_day = 0;
    if (file != NULL) {
        fscanf(file, "%d", &last_day);
        fclose(file);
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

    FILE* file = fopen("podsumowania/last_day.txt", "w");
    if (file == NULL) {
        perror("Błąd przy otwieraniu pliku (fopen)");
        return;
    }
    fprintf(file, "%d", day);
    fclose(file);
}

void display_and_save_summary(int day) {
    char filename[100];
    snprintf(filename, sizeof(filename), "podsumowania/podsumowanie_dnia_%d.txt", day);
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("Błąd przy otwieraniu pliku (fopen)");
        return;
    }

    fprintf(file, "---- Dzień %d Podsumowanie ----\n", day);
    for (int i = 0; i < 5; i++) {
        double small_earnings = small_pizza_sales[i] * menu[i].small_price;
        double large_earnings = large_pizza_sales[i] * menu[i].large_price;
        double small_costs = small_earnings * 0.7;
        double large_costs = large_earnings * 0.7;
        double small_profit = small_earnings * 0.3;
        double large_profit = large_earnings * 0.3;
        fprintf(file, "%s:\n", menu[i].name);
        fprintf(file, "- Mała - %d, Zarobek - %.2f, Koszt - %.2f\n", small_pizza_sales[i], small_profit, small_costs);
        fprintf(file, "- Duża - %d, Zarobek - %.2f, Koszt - %.2f\n", large_pizza_sales[i], large_profit, large_costs);
    }
    fprintf(file, "Całkowity Zarobek: %.2f\n", total_earnings * 0.3);
    fprintf(file, "Całkowity Koszt: %.2f\n", total_earnings * 0.7);

    fclose(file);
}

void* end_of_day_timer(void* arg) {
    sleep(50);
    end_of_day = 1;
    log_event("-- KONIEC DNIA, nie przyjmujemy nowych klientów --\n");
    return NULL;
}

void simulate_day(int day) {
    log_event("---- Dzień %d ----\n", day);
    sleep(2);

    pthread_t timer_thread;
    if (pthread_create(&timer_thread, NULL, end_of_day_timer, NULL) != 0) {
        perror("Błąd przy tworzeniu wątku timera (pthread_create)");
        return;
    }
    pthread_detach(timer_thread);

    srand(time(NULL));
    pthread_t client_threads[100];
    int client_thread_count = 0;

    while (!end_of_day && !evacuation) {
        int* group_size = malloc(sizeof(int));
        if (group_size == NULL) {
            perror("Błąd przy alokacji pamięci (malloc)");
            break;
        }
        *group_size = (rand() % 3) + 1;  // 1-3 osoby
        if (pthread_create(&client_threads[client_thread_count], NULL, client_function, (void*)group_size) != 0) {
            perror("Błąd przy tworzeniu wątku klienta (pthread_create)");
            free(group_size);
            break;
        }
        client_thread_count++;
        sleep(rand() % 5 + 1); // 1-5 sekund
    }

    for (int i = 0; i < client_thread_count; i++) {
        pthread_join(client_threads[i], NULL);
    }

    display_and_save_summary(day);
}

int main() {
    open_log_file();

    initialize_tables();
    initialize_menu();
    setup_signal_handler();

    if (pthread_cond_init(&fire_alarm, NULL) != 0) {
        perror("Błąd przy inicjalizacji zmiennej warunkowej (pthread_cond_init)");
        return EXIT_FAILURE;
    }
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Błąd przy inicjalizacji mutexa (pthread_mutex_init)");
        return EXIT_FAILURE;
    }

    pthread_t firefighter_thread;
    if (pthread_create(&firefighter_thread, NULL, firefighter_function, NULL) != 0) {
        perror("Błąd przy tworzeniu wątku strażaka (pthread_create)");
        return EXIT_FAILURE;
    }

    int day = read_last_day() + 1;
    while (1) {
        fire_signal = 0;
        end_of_day = 0;
        evacuation = 0;
        simulate_day(day);
        save_last_day(day);
        day++;
        sleep(2);
    }

    pthread_cond_destroy(&fire_alarm);
    pthread_mutex_destroy(&lock);

    for (int i = 0; i < TABLE_TYPES; i++) {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/table_sem_%d", i);
        sem_close(table_semaphores[i]);
        sem_unlink(sem_name);
    }

    close_log_file();
    return 0;
}