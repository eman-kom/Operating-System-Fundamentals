#include "restaurant.h"
#include <semaphore.h>
#include <stdlib.h>

#define MAX_SIZE 1050

typedef struct Table {
    int size;
    int isOccupied;
} Table;

Table* tables;
int total_tables;

typedef struct sem_queue {
    sem_t wait;
    int q_no;
} sem_queue;

sem_queue sem[5];
sem_queue size1[MAX_SIZE];
sem_queue size2[MAX_SIZE];
sem_queue size3[MAX_SIZE];
sem_queue size4[MAX_SIZE];
sem_queue size5[MAX_SIZE];

int  back[5] = {0, 0, 0, 0, 0};
int front[5] = {0, 0, 0, 0, 0};
sem_queue* queues[5] = {size1, size2, size3, size4, size5};

int count = 0;
sem_t count_lock;
sem_t queue_lock;
sem_t tables_lock;

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
}

void semaphore_init(int num_tables[5]) {

    sem_init(&count_lock, 0, 1);
    sem_init(&queue_lock, 0, 1);
    sem_init(&tables_lock, 0, 1);

    int cum = total_tables;
    for (int i = 0; i < 5; i++) {

        sem_init(&sem[i].wait, 0, cum);
        cum -= num_tables[i];
        sem[i].q_no = -1;
        
        sem_init(&queues[i][0].wait, 0, 1);
        queues[i][0].q_no = -1;

        for (int j = 1; j < MAX_SIZE; j++) {
            sem_init(&queues[i][j].wait, 0, 0);
            queues[i][j].q_no = -1;
        }
    }
}

void restaurant_init(int num_tables[5]) {
    tables_init(num_tables);
    semaphore_init(num_tables);
}

void restaurant_destroy(void) {

    free(tables);
    sem_destroy(&queue_lock);
    sem_destroy(&count_lock);
    sem_destroy(&tables_lock);

    for (int i = 0; i < 5; i++) {
        sem_destroy(&sem[i].wait);

        for (int j = 0; j < MAX_SIZE; j++) {
            sem_destroy(&queues[i][j].wait);
        }
    }
}

void guest_init(group_state *state, int num_people) {
    sem_wait(&count_lock);
    state -> q_no = count;
    state -> num = num_people;
    count++;
    sem_post(&count_lock);
}

void wait_in_queue(group_state *state, int num_people) {
    sem_wait(&queue_lock);
    int ptr = num_people-1;
    int next_customer = front[ptr];
    front[ptr]++;
    sem_post(&queue_lock);
    queues[ptr][next_customer].q_no = state -> q_no;
    sem_wait(&queues[ptr][next_customer].wait);
}

void find_table(group_state *state) {
    sem_wait(&tables_lock);
    for (int i = 0; i < total_tables; i++) {
        if (tables[i].size >= state -> num && tables[i].isOccupied == 0) {
            tables[i].isOccupied = 1;
            state -> id = i;
            state -> num = tables[i].size;
            break;
        }

    }
    sem_post(&tables_lock);
}

void decrement_cumulative(group_state *state, int num_people) {
    int ptr = num_people - 1;
    for (int i = 0; i < state -> num; i++) {
        int isLocked;
        sem_getvalue(&sem[i].wait, &isLocked);
        if (i != ptr && isLocked != 0) //do not decrement yourself twice
            sem_wait(&sem[i].wait);
    }
}

void call_next_in_line(int num_people) {
    int ptr = num_people - 1;
    sem_wait(&queue_lock);
    back[ptr]++;
    sem_post(&queue_lock);
    sem_post(&queues[ptr][back[ptr]].wait);
}

int request_for_table(group_state *state, int num_people) {
    
    on_enqueue();
    guest_init(state, num_people);
    wait_in_queue(state, num_people);

    sem[num_people-1].q_no = state -> q_no;

    int is_not_my_turn = 1;
    while (is_not_my_turn) {
        sem_wait(&sem[num_people-1].wait);

        int max = MAX_SIZE;
        int idx = num_people-1;
        for (int i = 0; i <= num_people-1; i++) {
            if (sem[i].q_no < max && sem[i].q_no != -1) {
                max = sem[i].q_no;
                idx = i;
            }
        }

        if (idx == num_people-1) {
            is_not_my_turn = 0;
        }

        else {
            sem_post(&sem[idx].wait);
        }
    }

    find_table(state);
    sem[num_people-1].q_no = -1;
    
    decrement_cumulative(state, num_people);
    call_next_in_line(num_people);

    return state -> id;
}

void leave_table(group_state *state) {
    sem_wait(&tables_lock);
    tables[state -> id].isOccupied = 0;
    sem_post(&tables_lock);
    for (int i = 0; i < state -> num; i++) {
        sem_post(&sem[i].wait);
    }
}