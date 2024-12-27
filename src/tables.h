//
// Created by Klaudia Karczmarczyk on 27/12/2024.
//

#ifndef PIZZERIA_TABLES_H
#define PIZZERIA_TABLES_H

#define TABLE_TYPES 4
int table_status[TABLE_TYPES];
int table_semaphores[TABLE_TYPES];

void initialize_tables();
int locate_table_for_group(int group_size);
void free_table(int table_size);

#endif //PIZZERIA_TABLES_H
