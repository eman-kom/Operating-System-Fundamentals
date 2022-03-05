#include "restaurant.h"
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_SIZE 1050

typedef struct Table {
    int size;
    int isOccupied;
} Table;

sem_t sem[5];
sem_t queue_lock;
sem_t tables_lock;

Table* tables;
int total_tables = 0;

sem_t size1[MAX_SIZE];
sem_t size2[MAX_SIZE];
sem_t size3[MAX_SIZE];
sem_t size4[MAX_SIZE];
sem_t size5[MAX_SIZE];

sem_t* queues[5] = {size1, size2, size3, size4, size5};
int front[5] = {0, 0, 0, 0, 0};
int back[5] = {0, 0, 0, 0, 0};

void semaphore_init(int num_tables[5]) {
    
    sem_init(&queue_lock, 0, 1);
    sem_init(&tables_lock, 0, 1);

    for (int i = 0; i < 5; i++) {

        sem_init(&queues[i][0], 0, 1);          //always let the first customer through without waiting
        sem_init(&sem[i], 0, num_tables[i]);    //num of empty tables

        for (int j = 1; j < MAX_SIZE; j++)
            sem_init(&queues[i][j], 0, 0);      //Assume 1000+ customers
    }
}

//create all tables of restaurant
void tables_init(int num_tables[5]) {

    for (int i = 0; i < 5; i++)
        total_tables += num_tables[i];

    tables = (Table*) malloc(total_tables * sizeof(Table));

    int k = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < num_tables[i]; j++) {
            tables[k].size = i+1;
            tables[k].isOccupied = 0;
            k++;
        }
    }

    //print tables
    //for (int i = 0; i < total_tables; ++i) printf("%d, ", tables[i]);
}

void restaurant_init(int num_tables[5]) {
    semaphore_init(num_tables);
    tables_init(num_tables);
}

void restaurant_destroy(void) {

    free(tables);
    sem_destroy(&queue_lock);
    sem_destroy(&tables_lock);

    for (int i = 0; i < 5; i++) {
        sem_destroy(&sem[i]);
        for (int j = 0; j < MAX_SIZE; j++)
            sem_destroy(&queues[i][j]);
    }
}

//------------------------------------------------

//enqueue customer based on size
void wait_in_queue(int ptr) {
    sem_wait(&queue_lock);
    front[ptr]++;   //move to next free spot
    int next_customer = front[ptr] - 1;
    sem_post(&queue_lock);
    sem_wait(&queues[ptr][next_customer]);
}

//gets next customer from the same size
void call_next_in_line(int ptr) {
    sem_wait(&queue_lock);
    back[ptr]++;
    sem_post(&queue_lock);
    sem_post(&queues[ptr][back[ptr]]);
}

//search through array to find empty table with same size to customer
void find_table(group_state *state) {

    sem_wait(&tables_lock);
    for (int i = 0; i < total_tables; i++) {

        if (tables[i].size == state -> num && tables[i].isOccupied == 0) {
            tables[i].isOccupied = 1;
            state -> id = i;
            break;
        }

    }

    sem_post(&tables_lock);
}

int request_for_table(group_state *state, int num_people) {

    on_enqueue();
    state -> num = num_people;
    int ptr = num_people - 1;

    wait_in_queue(ptr);
    sem_wait(&sem[ptr]);    //ensures that there is a empty table
    find_table(state);
    call_next_in_line(ptr);

    return state -> id;
}

//sets table to be empty
//calls next waiting customer from the same size
void leave_table(group_state *state) {
    sem_wait(&tables_lock);
    tables[state -> id].isOccupied = 0;
    sem_post(&tables_lock);
    sem_post(&sem[state -> num - 1]);
}