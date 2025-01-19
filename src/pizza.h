#ifndef PIZZERIA_PIZZA_H
#define PIZZERIA_PIZZA_H

#define MAX_PIZZAS 10

typedef struct {
    char name[50];
    double small_price;
    double large_price;
    int id;
} Pizza;

extern Pizza menu[MAX_PIZZAS];
extern int menu_size;

void initialize_menu();
void select_random_pizzas(int group_size, Pizza* selected_pizzas, int* selected_sizes);
double calculate_total_cost(Pizza* selected_pizzas, int* selected_sizes, int group_size);

#endif // PIZZERIA_PIZZA_H
