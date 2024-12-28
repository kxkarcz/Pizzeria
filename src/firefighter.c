#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "globals.h"
#include "firefighter.h"
#include "logging.h"

void* firefighter_function(void* arg) {
    sigset_t sigset;
    int sig;

    if (sigemptyset(&sigset) == -1) {
        perror("Błąd przy inicjalizacji zbioru sygnałów (sigemptyset)");
        pthread_exit(NULL);
    }
    if (sigaddset(&sigset, SIGTTOU) == -1) {
        perror("Błąd przy dodawaniu sygnału do zbioru (sigaddset)");
        pthread_exit(NULL);
    }

    while (1) {
        if (sigwait(&sigset, &sig) != 0) {
            perror("Błąd przy oczekiwaniu na sygnał (sigwait)");
            pthread_exit(NULL);
        }
        if (sig == SIGTTOU) {
            if (pthread_mutex_lock(&lock) != 0) {
                perror("Błąd przy blokowaniu mutexa (pthread_mutex_lock)");
                pthread_exit(NULL);
            }
            fire_signal = 1;
            log_event("[Strażak] Otrzymano sygnał pożaru (SIGTTOU).\n");
            if (pthread_cond_broadcast(&fire_alarm) != 0) {
                perror("Błąd przy rozsyłaniu sygnału warunkowego (pthread_cond_broadcast)");
                pthread_mutex_unlock(&lock);
                pthread_exit(NULL);
            }
            if (pthread_mutex_unlock(&lock) != 0) {
                perror("Błąd przy odblokowywaniu mutexa (pthread_mutex_unlock)");
                pthread_exit(NULL);
            }
        }
    }
}

void cleanup_and_exit() {
    if (pthread_cond_destroy(&fire_alarm) != 0) {
        perror("Błąd przy niszczeniu zmiennej warunkowej (pthread_cond_destroy)");
    }
    if (pthread_mutex_destroy(&lock) != 0) {
        perror("Błąd przy niszczeniu mutexa (pthread_mutex_destroy)");
    }
    log_event("-- KONIEC DNIA SPOWODOWANY EWAKUACJĄ\n");
}

void signal_handler(int signum) {
    if (signum == SIGTTOU) {
        if (pthread_mutex_lock(&lock) != 0) {
            perror("Błąd przy blokowaniu mutexa (pthread_mutex_lock)");
            return;
        }
        fire_signal = 1;
        evacuation = 1;
        if (pthread_cond_broadcast(&fire_alarm) != 0) {
            perror("Błąd przy rozsyłaniu sygnału warunkowego (pthread_cond_broadcast)");
            pthread_mutex_unlock(&lock);
            return;
        }
        if (pthread_mutex_unlock(&lock) != 0) {
            perror("Błąd przy odblokowywaniu mutexa (pthread_mutex_unlock)");
            return;
        }
        log_event("[Strażak] Odebrano sygnał o pożarze SIGTTOU. Następuje ewakuacja\n");
        cleanup_and_exit();
    }
}

void setup_signal_handler() {
    if (signal(SIGTTOU, signal_handler) == SIG_ERR) {
        perror("Błąd przy ustawianiu obsługi sygnału (signal)");
    }
}