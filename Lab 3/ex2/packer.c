#include "packer.h"
#include <semaphore.h>
#include <stdbool.h>

// You can declare global variables here

sem_t mutex1;
sem_t mutex2;

struct Box {
    sem_t full;
    sem_t empty;
    int ball_1;
    int ball_2;
    bool isOne;
    int release;
} red, green, blue;

int pack(struct Box* box, int id);

void packer_init(void) {
    // Write initialization code here (called once at the start of the program).

    sem_init(&mutex1, 0, 1);
    sem_init(&mutex2, 0, 1);

    sem_init(&red.full, 0, 0);
    sem_init(&blue.full, 0, 0);
    sem_init(&green.full, 0, 0);

    sem_init(&red.empty, 0, 2);
    sem_init(&blue.empty, 0, 2);
    sem_init(&green.empty, 0, 2);

    red.isOne = false;
    blue.isOne = false;
    green.isOne = false;

    red.release = 0;
    blue.release = 0;
    green.release = 0;
}

void packer_destroy(void) {
    // Write deinitialization code here (called once at the end of the program).

    sem_destroy(&mutex1);
    sem_destroy(&mutex2);

    sem_destroy(&red.full);
    sem_destroy(&blue.full);
    sem_destroy(&green.full);

    sem_destroy(&red.empty);
    sem_destroy(&blue.empty);
    sem_destroy(&green.empty);
}

int pack_ball(int colour, int id) {
    // Write your code here.

    int ret = 0;

    switch (colour) {
        case 1:
            ret = pack(&red, id);
            break;

        case 2:
            ret = pack(&blue, id);
            break;

        case 3:
            ret = pack(&green, id);
            break;
        }

    return ret;
}

int pack(struct Box* box, int id) {

    sem_wait(&box -> empty);
    sem_wait(&mutex1);

    if (box -> isOne) {
        box -> ball_2 = id;
        box -> isOne = false;
        sem_post(&box -> full);
        sem_post(&box -> full);
    }

    else {
        box -> ball_1 = id;
        box -> isOne = true;
    }

    sem_post(&mutex1);
    sem_wait(&box -> full);

    sem_wait(&mutex2);

    int other_id = (box -> ball_1 == id) ? box -> ball_2 : box -> ball_1;

    box -> release++;
    if (box -> release == 2) {
        sem_post(&box -> empty);
        sem_post(&box -> empty);
        box -> release = 0;
    }

    sem_post(&mutex2);

    return other_id;
}