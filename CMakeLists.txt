cmake_minimum_required(VERSION 3.28)
project(Pizzeria C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(Pizzeria src/main.c
        src/firefighter.c
        src/tables.h
        src/tables.c
        src/client.c
        src/client.h
        src/boss.c
        src/globals.c
        src/globals.h
        src/firefighter.h
        src/pizza.c
        src/pizza.h
        src/boss.h
        src/logging.c
        src/logging.h
        src/pizzeria.c
        src/pizzeria.h
        src/tests.c
        src/tests.h
)

