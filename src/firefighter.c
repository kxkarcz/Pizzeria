#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "globals.h"
#include "firefighter.h"

void* firefighter_function(void* arg) {
    sigset_t sigset;
    int sig;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGTSTP);

    while (1) {
        sigwait(&sigset, &sig);
        if (sig == SIGTSTP) {
            pthread_mutex_lock(&lock);
            fire_signal = 1;
            printf("[Strażak] Otrzymano sygnał pożaru (SIGTSTP).\n");
            pthread_cond_broadcast(&fire_alarm); // Powiadom innych
            pthread_mutex_unlock(&lock);
        }
    }
}

void cleanup_and_exit() {
    pthread_cond_destroy(&fire_alarm);
    pthread_mutex_destroy(&lock);
    printf("koniec dnia\n");
    exit(0);
}

void signal_handler(int signum) {
    if (signum == SIGTSTP) {
        pthread_mutex_lock(&lock);
        fire_signal = 1;
        pthread_cond_broadcast(&fire_alarm);
        pthread_mutex_unlock(&lock);
        printf("[Strażak] Odebrano sygnał o pożarze SIGTSTP. Następuje ewakuacja\n");
        cleanup_and_exit();
    }
}

void setup_signal_handler() {
    signal(SIGTSTP, signal_handler);
}