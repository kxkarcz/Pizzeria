#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "logging.h"

#define LOG_FILE "logs.txt"

static int log_file_fd = -1;

void initialize_logging() {
    log_file_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_file_fd == -1) {
        perror("Błąd przy otwieraniu pliku logów");
        exit(EXIT_FAILURE);
    }
}

void log_message(const char* format, ...) {
    va_list args;
    char buffer[1024];
    int length;

    if (log_file_fd == -1) {
        initialize_logging();
    }

    // Logowanie do konsoli
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Logowanie do pliku
    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length > 0) {
        write(log_file_fd, buffer, length);
    }
}

void close_log() {
    if (log_file_fd != -1) {
        close(log_file_fd);
        log_file_fd = -1;
    }
}