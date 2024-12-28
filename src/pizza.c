#include "pizza.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

Pizza menu[MAX_PIZZAS];
int menu_size;

void initialize_menu() {
    if (strcpy(menu[0].name, "Margharitta") == NULL) {
        perror("Błąd przy kopiowaniu nazwy pizzy (strcpy)");
        exit(EXIT_FAILURE);
    }
    menu[0].small_price = 20.0;
    menu[0].large_price = 30.0;
    menu[0].id = 0;

    if (strcpy(menu[1].name, "Pepperoni") == NULL) {
        perror("Błąd przy kopiowaniu nazwy pizzy (strcpy)");
        exit(EXIT_FAILURE);
    }
    menu[1].small_price = 25.0;
    menu[1].large_price = 35.0;
    menu[1].id = 1;

    if (strcpy(menu[2].name, "Hawajska") == NULL) {
        perror("Błąd przy kopiowaniu nazwy pizzy (strcpy)");
        exit(EXIT_FAILURE);
    }
    menu[2].small_price = 22.0;
    menu[2].large_price = 32.0;
    menu[2].id = 2;

    if (strcpy(menu[3].name, "Vege") == NULL) {
        perror("Błąd przy kopiowaniu nazwy pizzy (strcpy)");
        exit(EXIT_FAILURE);
    }
    menu[3].small_price = 18.0;
    menu[3].large_price = 28.0;
    menu[3].id = 3;

    if (strcpy(menu[4].name, "BBQ Chicken") == NULL) {
        perror("Błąd przy kopiowaniu nazwy pizzy (strcpy)");
        exit(EXIT_FAILURE);
    }
    menu[4].small_price = 27.0;
    menu[4].large_price = 37.0;
    menu[4].id = 4;

    menu_size = 5;

    srand(time(NULL));
}

void select_random_pizzas(int group_size, Pizza* selected_pizzas, int* selected_sizes) {
    for (int i = 0; i < group_size; i++) {
        int pizza_index = rand() % menu_size;
        selected_pizzas[i] = menu[pizza_index];
        selected_sizes[i] = rand() % 2; // 0 - mała, 1 - duża
    }
}

double calculate_total_cost(Pizza* selected_pizzas, int* selected_sizes, int group_size) {
    double total_cost = 0.0;
    for (int i = 0; i < group_size; i++) {
        if (selected_sizes[i] == 0) {
            total_cost += selected_pizzas[i].small_price;
        } else {
            total_cost += selected_pizzas[i].large_price;
        }
    }
    return total_cost;
}