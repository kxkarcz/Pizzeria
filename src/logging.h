#ifndef PIZZERIA_LOGGING_H
#define PIZZERIA_LOGGING_H

void log_event(const char* format, ...);
void close_log_file();
void open_log_file();

#endif //PIZZERIA_LOGGING_H
