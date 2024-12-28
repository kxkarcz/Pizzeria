#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "tables.h"
#include "globals.h"
#include "boss.h"
#include "logging.h"

extern FILE* log_file;
extern int evacuation;

void* client_function(void* arg) {
    int group_size = *(int*)arg;
    pthread_t thread_id = pthread_self();
    int waiting_message_printed = 0;

    if (evacuation) {
        log_event("Wątek %lu: Grupa %d-osobowa opuszcza pizzerię z powodu pożaru.\n",
                  (unsigned long)thread_id, group_size);
        free(arg);
        return NULL;
    }

    log_event("Wątek %lu: Grupa %d-osobowa wchodzi do pizzerii.\n",
              (unsigned long)thread_id, group_size);

    boss_function(group_size, thread_id);

    while (1) {
        if (pthread_mutex_lock(&lock) != 0) {
            perror("Błąd przy blokowaniu mutexa (pthread_mutex_lock)");
            free(arg);
            return NULL;
        }

        if (end_of_day || evacuation || fire_signal) {
            if (pthread_mutex_unlock(&lock) != 0) {
                perror("Błąd przy odblokowywaniu mutexa (pthread_mutex_unlock)");
            }

            if (end_of_day) {
                log_event("Wątek %lu: Niestety lokal został zamknięty zanim grupa %d-osobowa znalazła stolik.\n",
                          (unsigned long)thread_id, group_size);
            } else if (evacuation || fire_signal) {
                log_event("Wątek %lu: Grupa %d-osobowa opuszcza pizzerię z powodu ewakuacji.\n",
                          (unsigned long)thread_id, group_size);
            }
            free(arg);
            return NULL;
        }

        int table_id = locate_table_for_group(group_size, thread_id);
        if (table_id != -1) {
            if (pthread_mutex_unlock(&lock) != 0) {
                perror("Błąd przy odblokowywaniu mutexa (pthread_mutex_unlock)");
                free(arg);
                return NULL;
            }

            log_event("Wątek %lu: Grupa %d-osobowa usiadła przy stoliku %d.\n",
                      (unsigned long)thread_id, group_size, table_id);
            log_event("Wątek %lu: Grupa %d-osobowa otrzymuje pizzę.\n",
                      (unsigned long)thread_id, group_size);

            int eating_time = rand() % 10 + 10; // 10-20 sekund
            for (int i = 0; i < eating_time; i++) {
                if (fire_signal) {
                    break;
                }
                sleep(1);
            }

            if (pthread_mutex_lock(&lock) != 0) {
                perror("Błąd przy blokowaniu mutexa (pthread_mutex_lock)");
                free(arg);
                return NULL;
            }
            free_table(group_size, table_id, thread_id);
            log_event("Wątek %lu: Grupa %d-osobowa zwolniła stolik %d%s.\n",
                      (unsigned long)thread_id, group_size, table_id,
                      fire_signal ? " z powodu pożaru" : "");
            if (pthread_cond_broadcast(&fire_alarm) != 0) {
                perror("Błąd przy rozsyłaniu sygnału warunkowego (pthread_cond_broadcast)");
                pthread_mutex_unlock(&lock);
                free(arg);
                return NULL;
            }
            if (pthread_mutex_unlock(&lock) != 0) {
                perror("Błąd przy odblokowywaniu mutexa (pthread_mutex_unlock)");
                free(arg);
                return NULL;
            }

            break;
        }

        if (!waiting_message_printed) {
            log_event("Wątek %lu: Brak dostępnych stolików dla grupy %d-osobowej. Oczekiwanie...\n",
                      (unsigned long)thread_id, group_size);
            waiting_message_printed = 1;
        }
        if (pthread_mutex_unlock(&lock) != 0) {
            perror("Błąd przy odblokowywaniu mutexa (pthread_mutex_unlock)");
            free(arg);
            return NULL;
        }
        usleep(100000); // 100 ms
    }

    // Obsługa sytuacji pożaru, jeśli nie zakończono wcześniej
    if (fire_signal) {
        log_event("Wątek %lu: Grupa %d-osobowa opuszcza pizzerię z powodu pożaru.\n",
                  (unsigned long)thread_id, group_size);
    }

    free(arg);
    return NULL;
}