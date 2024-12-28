#include "boss.h"
#include "pizza.h"
#include "globals.h"
#include <stdio.h>
#include "logging.h"

extern FILE* log_file;



void update_sales_and_earnings(Pizza* selected_pizzas, int* selected_sizes, int group_size) { //wybrane pizze, rozmiary, rozmiar grupy
    for (int i = 0; i < group_size; i++) {
        if (selected_sizes[i] == 0) {
            small_pizza_sales[selected_pizzas[i].id]++;
            total_earnings += selected_pizzas[i].small_price;
        } else {
            large_pizza_sales[selected_pizzas[i].id]++;
            total_earnings += selected_pizzas[i].large_price;
        }
    }
}

void boss_function(int group_size, pthread_t thread_id) {
    Pizza selected_pizzas[group_size];
    int selected_sizes[group_size];

    select_random_pizzas(group_size, selected_pizzas, selected_sizes);
    double total_cost = calculate_total_cost(selected_pizzas, selected_sizes, group_size);

    log_event("Wątek %lu: Grupa %d-osobowa zamawia pizze:\n", (unsigned long)thread_id, group_size);
    for (int i = 0; i < group_size; i++) {
        log_event("  - %s (%s) - %.2f zł\n", selected_pizzas[i].name, selected_sizes[i] == 0 ? "mała" : "duża", selected_sizes[i] == 0 ? selected_pizzas[i].small_price : selected_pizzas[i].large_price);
    }
    log_event("Wątek %lu: Całkowity koszt zamówienia: %.2f zł\n", (unsigned long)thread_id, total_cost);

    update_sales_and_earnings(selected_pizzas, selected_sizes, group_size);
}