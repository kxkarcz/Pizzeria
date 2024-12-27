#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "tables.h"
#include "globals.h"

void* client_function(void* arg) {
    int group_size = *(int*)arg;
    pthread_t thread_id = pthread_self();

    printf("Wątek %lu: Grupa %d-osobowa wchodzi do pizzerii.\n", (unsigned long)thread_id, group_size);
    printf("Wątek %lu: Grupa %d-osobowa zamawia pizzę.\n", (unsigned long)thread_id, group_size);

    pthread_mutex_lock(&lock);
    while (!fire_signal) {
        int table_id = locate_table_for_group(group_size, thread_id);
        if (table_id != -1) {
            pthread_mutex_unlock(&lock);
            printf("Wątek %lu: Grupa %d-osobowa usiadła przy stoliku %d.\n", (unsigned long)thread_id, group_size, table_id);
            printf("Wątek %lu: Grupa %d-osobowa otrzymuje pizzę.\n", (unsigned long)thread_id, group_size);

            for (int i = 0; i < 20; i++) {
                if (fire_signal) {
                    printf("Wątek %lu: Pożar wykryty podczas posiłku grupy %d-osobowej!\n", (unsigned long)thread_id, group_size);
                    break;
                }
                sleep(1);
            }

            pthread_mutex_lock(&lock);
            free_table(group_size, table_id, thread_id);
            printf("Wątek %lu: Grupa %d-osobowa zwolniła stolik %d%s.\n",
                   (unsigned long)thread_id, group_size, table_id, fire_signal ? " z powodu pożaru" : "");
            pthread_mutex_unlock(&lock);

            break;
        } else {
            printf("Wątek %lu: Brak dostępnych stolików dla grupy %d-osobowej. Oczekiwanie...\n", (unsigned long)thread_id, group_size);
            pthread_cond_wait(&fire_alarm, &lock);
        }
    }

    pthread_mutex_unlock(&lock);
    if (fire_signal) {
        printf("Wątek %lu: Grupa %d-osobowa opuszcza pizzerię z powodu pożaru.\n",
               (unsigned long)thread_id, group_size);
    }
    return NULL;
}