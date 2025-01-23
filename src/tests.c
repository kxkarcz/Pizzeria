#include <assert.h>
#include <errno.h>
#include "globals.h"
#include "tables.h"
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include "logging.h"

/**
 * @brief Testuje pamięć współdzieloną i semafory.
 */
void test_shared_memory_and_semaphores() {
    printf("[Test] Sprawdzanie pamięci współdzielonej i semaforów...\n");
    setup_shared_memory_and_semaphores();

    if (shm_data == NULL || sem_id == -1) {
        printf("[Błąd] Pamięć współdzielona lub semafory nie zostały poprawnie utworzone.\n");
        return;
    }

    cleanup_shared_memory_and_semaphores();

    int shm_check = shmget(SHM_KEY, sizeof(struct shared_data), 0666);
    int sem_check = semget(SEM_KEY, 1, 0666);

    if (shm_check != -1 || errno != ENOENT || sem_check != -1 || errno != ENOENT) {
        printf("[Błąd] Zasoby nie zostały poprawnie zwolnione.\n");
        return;
    }

    printf("[Sukces] Test pamięci współdzielonej i semaforów zakończony pomyślnie.\n");
}
/**
 * @brief Testuje konfigurację stolików.
 */
void test_table_configuration() {
    setup_shared_memory_and_semaphores();
    configure_tables(5, 3, 2, 1); // 5x1-osobowe, 3x2-osobowe, 2x3-osobowe, 1x4-osobowy

    if (total_tables != 11 || table_sizes[0] != 1 || table_sizes[10] != 4) {
        printf("[Błąd] Konfiguracja stolików niepoprawna.\n");
        return;
    }

    if (lock_semaphore() == -1) {
        printf("[Błąd] Nie udało się zablokować semafora.\n");
        return;
    }
    for (int i = 0; i < total_tables; i++) {
        if (shm_data->table_occupancy[i] != 0) {
            printf("[Błąd] Nieprawidłowy stan zajętości stolików.\n");
            unlock_semaphore();
            return;
        }
    }
    if (unlock_semaphore() == -1) {
        printf("[Błąd] Nie udało się odblokować semafora.\n");
    }

    cleanup_shared_memory_and_semaphores();
    printf("[Sukces] Test konfiguracji stolików przeszedł pomyślnie.\n");
}
/**
 * @brief Funkcja symulująca zakleszczenie wątków.
 *
 * @param arg Argument przekazywany do wątku (nieużywany).
 * @return void* Zwraca NULL.
 */
void* simulate_deadlock(void* arg) {
    if (lock_semaphore() == -1) {
        log_message("[Błąd] Nie udało się zablokować semafora.\n");
        return NULL;
    }
    log_message("[Test] Zablokowano semafor. Symulacja zakleszczenia.\n");
    sleep(3);
    if (unlock_semaphore() == -1) {
        log_message("[Błąd] Nie udało się odblokować semafora.\n");
    }
    log_message("[Test] Odblokowano semafor. Koniec symulacji.\n");
    return NULL;
}

/**
 * @brief Testuje zakleszczenia i wielowątkowe blokady.
 */
void test_deadlock_and_resource_blocks() {
    log_message("[Test] Sprawdzanie zakleszczeń i blokad zasobów...\n");
    setup_shared_memory_and_semaphores();

    pthread_t threads[2];

    if (pthread_create(&threads[0], NULL, simulate_deadlock, NULL) != 0) {
        perror("[Błąd] Błąd przy tworzeniu wątku 1");
        cleanup_shared_memory_and_semaphores();
        return;
    }

    sleep(1);

    if (pthread_create(&threads[1], NULL, simulate_deadlock, NULL) != 0) {
        perror("[Błąd] Błąd przy tworzeniu wątku 2");
        pthread_join(threads[0], NULL);
        cleanup_shared_memory_and_semaphores();
        return;
    }

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    cleanup_shared_memory_and_semaphores();
    printf("[Sukces] Test zakleszczeń i blokad zakończony pomyślnie.\n");
}

/**
 * @brief Testuje odczyt i zapis do pamięci współdzielonej.
 */
void test_shared_memory_read_write() {
    printf("[Test] Sprawdzanie odczytu i zapisu do pamięci współdzielonej...\n");
    setup_shared_memory_and_semaphores();
    // Zapis przykładowych danych do pamięci współdzielonej
    lock_semaphore();
    shm_data->current_clients = 42;
    unlock_semaphore();
    // Odczyt danych z pamięci współdzielonej
    lock_semaphore();
    int read_value = shm_data->current_clients;
    unlock_semaphore();
    // Weryfikacja poprawności odczytanych danych
    if (read_value != 42) {
        printf("[Błąd] Odczytana wartość z pamięci współdzielonej jest niepoprawna.\n");
        cleanup_shared_memory_and_semaphores();
        return;
    }
    shm_data->current_clients = 0; // Przywrócenie wartości początkowej
    cleanup_shared_memory_and_semaphores();
    printf("[Sukces] Test odczytu i zapisu do pamięci współdzielonej zakończony pomyślnie.\n");
}

/**
* @brief Testuje system logowania.
*/
void test_logging_system() {
    printf("[Test] Sprawdzanie systemu logowania...\n");

    // Inicjalizacja systemu logowania
    initialize_logging();

    // Zapis przykładowej wiadomości do logów
    log_message("[Test] To jest przykładowa wiadomość logu.\n");

    // Sprawdzenie, czy wiadomość została poprawnie zapisana do pliku logów
    int log_file_fd = open("logs.txt", O_RDONLY);
    if (log_file_fd == -1) {
        perror("[Błąd] Nie udało się otworzyć pliku logów do odczytu");
        close_log();
        return;
    }

    // Przesunięcie wskaźnika pliku na koniec minus długość wiadomości
    off_t offset = lseek(log_file_fd, -strlen("[Test] To jest przykładowa wiadomość logu.\n"), SEEK_END);
    if (offset == -1) {
        perror("[Błąd] Nie udało się przesunąć wskaźnika pliku");
        close(log_file_fd);
        close_log();
        return;
    }

    char buffer[1024];
    ssize_t bytes_read = read(log_file_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1) {
        perror("[Błąd] Nie udało się odczytać pliku logów");
        close(log_file_fd);
        close_log();
        return;
    }
    buffer[bytes_read] = '\0';

    // Sprawdzenie, czy wiadomość znajduje się na końcu pliku logów
    if (strstr(buffer, "[Test] To jest przykładowa wiadomość logu.") == NULL) {
        printf("[Błąd] Wiadomość logu nie została poprawnie zapisana.\n");
        close(log_file_fd);
        close_log();
        return;
    }

    close(log_file_fd);

    // Zamknięcie systemu logowania
    close_log();

    printf("[Sukces] Test systemu logowania zakończony pomyślnie.\n");
}