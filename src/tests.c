#include <assert.h>
#include <errno.h>
#include "globals.h"
#include "tables.h"
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "boss.h"
#include "firefighter.h"
#include "logging.h"

void test_shared_memory_and_semaphores() {
    setup_shared_memory_and_semaphores();

    // Test pamięci współdzielonej
    if (shm_data == NULL || sem_id == -1) {
        printf("[Błąd] Pamięć współdzielona lub semafory nie zostały poprawnie utworzone.\n");
        return;
    }

    // Test inicjalizacji wartości w pamięci współdzielonej
    if (shm_data->client_count != 0 || shm_data->end_of_day != 0) {
        printf("[Błąd] Nieprawidłowe wartości w pamięci współdzielonej.\n");
        return;
    }

    cleanup_shared_memory_and_semaphores();

    // Sprawdzenie, czy zasoby zostały zwolnione
    int shm_check = shmget(SHM_KEY, sizeof(struct shared_data), 0666);
    int sem_check = semget(SEM_KEY, 1, 0666);

    if (shm_check != -1 || errno != ENOENT || sem_check != -1 || errno != ENOENT) {
        printf("[Błąd] Zasoby nie zostały poprawnie zwolnione.\n");
        return;
    }

    printf("[Sukces] Test pamięci współdzielonej i semaforów przeszedł pomyślnie.\n");
}

void test_table_configuration() {
    setup_shared_memory_and_semaphores();
    configure_tables(5, 3, 2, 1); // 5x1-osobowe, 3x2-osobowe, 2x3-osobowe, 1x4-osobowy

    if (total_tables != 11 || table_sizes[0] != 1 || table_sizes[10] != 4) {
        printf("[Błąd] Konfiguracja stolików niepoprawna.\n");
        return;
    }

    lock_semaphore();
    for (int i = 0; i < total_tables; i++) {
        if (shm_data->table_occupancy[i] != 0) {
            printf("[Błąd] Nieprawidłowy stan zajętości stolików.\n");
            unlock_semaphore();
           return;
        }
    }
    unlock_semaphore();

    cleanup_shared_memory_and_semaphores();
    printf("[Sukces] Test konfiguracji stolików przeszedł pomyślnie.\n");
}

void test_max_process_limit() {
    log_message("[Test] Sprawdzanie maksymalnej liczby procesów.\n");

    for (int i = 0; i < MAX_CLIENTS + 5; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            log_message("[Test] Proces klienta uruchomiony: PID %d.\n", getpid());
            sleep(1);
            exit(0);
        } else if (pid < 0) {
            printf("[Sukces] Osiągnięto limit procesów lub wystąpił błąd, zgodnie z oczekiwaniami.\n");
            break;
        }
    }

    while (wait(NULL) > 0);
    log_message("[Test] Test maksymalnej liczby procesów zakończony.\n");
}

void* test_deadlock_simulation(void* arg) {
    lock_semaphore();
    log_message("[Test] Zablokowano semafor. Symulacja zakleszczenia.\n");
    sleep(5);
    unlock_semaphore();
    log_message("[Test] Odblokowano semafor. Koniec symulacji.\n");
    return NULL;
}

void test_resource_block_handling() {
    log_message("[Test] Sprawdzanie blokad zasobów.\n");
    setup_shared_memory_and_semaphores();

    pthread_t thread1, thread2;

    // Tworzenie wątków
    if (pthread_create(&thread1, NULL, test_deadlock_simulation, NULL) != 0) {
        perror("[Błąd] Błąd przy tworzeniu wątku 1");
        cleanup_shared_memory_and_semaphores();
        return;
    }

    sleep(1); // Upewnij się, że thread1 zablokował semafor

    if (pthread_create(&thread2, NULL, test_deadlock_simulation, NULL) != 0) {
        perror("[Błąd] Błąd przy tworzeniu wątku 2");
        pthread_join(thread1, NULL);
        cleanup_shared_memory_and_semaphores();
        return;
    }

    // Czekanie na zakończenie wątków
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    cleanup_shared_memory_and_semaphores();

    printf("[Sukces] Test blokad zasobów zakończony pomyślnie.\n");
}

void run_all_tests() {
    printf("Rozpoczynanie testów...\n");

    test_shared_memory_and_semaphores();
    test_table_configuration();
    test_max_process_limit();
    test_resource_block_handling();

    printf("\nPodsumowanie: Wszystkie testy zostały wykonane.\n");
}
