#include "pizza.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

// Tablica przechowująca menu pizzerii
Pizza menu[MAX_PIZZAS];
int menu_size;

/**
 * @brief Inicjalizuje menu pizzerii.
 */
void initialize_menu() {
    // Dodawanie pozycji do menu
    strncpy(menu[0].name, "Margharitta", sizeof(menu[0].name) - 1);
    menu[0].name[sizeof(menu[0].name) - 1] = '\0';
    menu[0].small_price = 20.0;
    menu[0].large_price = 30.0;
    menu[0].id = 0;

    strncpy(menu[1].name, "Pepperoni", sizeof(menu[1].name) - 1);
    menu[1].name[sizeof(menu[1].name) - 1] = '\0';
    menu[1].small_price = 25.0;
    menu[1].large_price = 35.0;
    menu[1].id = 1;

    strncpy(menu[2].name, "Hawajska", sizeof(menu[2].name) - 1);
    menu[2].name[sizeof(menu[2].name) - 1] = '\0';
    menu[2].small_price = 22.0;
    menu[2].large_price = 32.0;
    menu[2].id = 2;

    strncpy(menu[3].name, "Vege", sizeof(menu[3].name) - 1);
    menu[3].name[sizeof(menu[3].name) - 1] = '\0';
    menu[3].small_price = 18.0;
    menu[3].large_price = 28.0;
    menu[3].id = 3;

    strncpy(menu[4].name, "BBQ Chicken", sizeof(menu[4].name) - 1);
    menu[4].name[sizeof(menu[4].name) - 1] = '\0';
    menu[4].small_price = 27.0;
    menu[4].large_price = 37.0;
    menu[4].id = 4;

    menu_size = 5;

    srand(time(NULL)); // Jednorazowa inicjalizacja generatora liczb losowych
}

/**
 * @brief Losowo wybiera pizze dla grupy klientów.
 *
 * @param group_size Liczba osób w grupie.
 * @param selected_pizzas Tablica przechowująca wybrane pizze.
 * @param selected_sizes Tablica przechowująca rozmiary wybranych pizz (0 - mała, 1 - duża).
 */
void select_random_pizzas(int group_size, Pizza* selected_pizzas, int* selected_sizes) {
    for (int i = 0; i < group_size; i++) {
        int pizza_index = rand() % menu_size;
        selected_pizzas[i] = menu[pizza_index];
        selected_sizes[i] = rand() % 2; // 0 - mała, 1 - duża
    }
}

/**
 * @brief Oblicza całkowity koszt zamówienia.
 *
 * @param selected_pizzas Tablica wybranych pizz.
 * @param selected_sizes Tablica rozmiarów pizz (0 - mała, 1 - duża).
 * @param group_size Liczba osób w grupie.
 * @return double Całkowity koszt zamówienia.
 */
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