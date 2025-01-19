#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "logging.h"

#define LOG_FILE "logs.txt"

static FILE* log_file = NULL;

void initialize_logging() {
    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Błąd przy otwieraniu pliku logów");
        exit(EXIT_FAILURE);
    }
}

void log_message(const char* format, ...) {
    va_list args;

    if (!log_file) {
        initialize_logging();
    }

    // Logowanie do konsoli
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Logowanie do pliku
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

void close_log() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}