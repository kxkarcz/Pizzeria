#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

FILE* log_file = NULL;

void open_log_file() {
    log_file = fopen("logs.txt", "a");
    if (log_file == NULL) {
        perror("Błąd przy otwieraniu pliku logów (fopen)");
        exit(EXIT_FAILURE);
    }
}

void close_log_file() {
    if (log_file != NULL) {
        fclose(log_file);
    }
}

void log_event(const char *format, ...) {
    if (log_file == NULL) {
        open_log_file();
    }

    va_list args;
    va_start(args, format);

    vfprintf(log_file, format, args);
    fflush(log_file);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}