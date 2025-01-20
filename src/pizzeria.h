#ifndef PIZZERIA_H
#define PIZZERIA_H

int read_last_day();
void save_last_day(int day);
void display_and_save_summary(int day);
void* end_of_day_timer(void* arg);
void simulate_day(int day);
void remove_client_pid(pid_t pid);

#endif // PIZZERIA_H