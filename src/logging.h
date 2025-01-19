#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdarg.h>

void log_message(const char* format, ...);
void close_log();
void initialize_logging();

#endif // LOGGING_H