#ifndef PIZZERIA_FIREFIGHTER_H
#define PIZZERIA_FIREFIGHTER_H

void* firefighter_function(void* arg);
void cleanup_and_exit();
void signal_handler(int signum);
void setup_signal_handler();

#endif //PIZZERIA_FIREFIGHTER_H
