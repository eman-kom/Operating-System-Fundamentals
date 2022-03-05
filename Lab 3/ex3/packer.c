#include "packer.h"
#include <semaphore.h>
#include <stdlib.h>

// You can declare global variables here

struct Box {
    sem_t full;
    sem_t empty;
    int*  arr;
    int   size;
    int   release;
    int   curSize;
} red, green, blue;

sem_t mutex1;
sem_t mutex2;

void pack(struct Box* box, int id, int *other_ids);

void packer_init(int balls_per_pack) {
    // Write initialization code here (called once at the start of the program).
    // It is guaranteed that balls_per_pack >= 2.

    sem_init(&mutex1, 0, 1);
    sem_init(&mutex2, 0, 1);

    red.size = balls_per_pack;
    blue.size = balls_per_pack;
    green.size = balls_per_pack;

    red.curSize = 0;
    blue.curSize = 0;
    green.curSize = 0;

    red.release = 0;
    blue.release = 0;
    green.release = 0;

    sem_init(&red.full, 0, 0);
    sem_init(&blue.full, 0, 0);
    sem_init(&green.full, 0, 0);

    sem_init(&red.empty, 0, balls_per_pack);
    sem_init(&blue.empty, 0, balls_per_pack);
    sem_init(&green.empty, 0, balls_per_pack);

    red.arr = (int*) malloc(balls_per_pack * sizeof(int));
    blue.arr = (int*) malloc(balls_per_pack * sizeof(int));
    green.arr = (int*) malloc(balls_per_pack * sizeof(int));
}

void packer_destroy(void) {
    // Write deinitialization code here (called once at the end of the program).
    free(red.arr);
    free(blue.arr);
    free(green.arr);
    sem_destroy(&mutex1);
    sem_destroy(&mutex2);
    sem_destroy(&red.full);
    sem_destroy(&blue.full);
    sem_destroy(&green.full);
    sem_destroy(&red.empty);
    sem_destroy(&blue.empty);
    sem_destroy(&green.empty);
}

void pack_ball(int colour, int id, int *other_ids) {
    // Write your code here.
    // Remember to populate the array `other_ids` with the (balls_per_pack-1) other balls.

    switch (colour) {
        case 1:
            pack(&red, id, other_ids);
            break;

        case 2:
            pack(&blue, id, other_ids);
            break;

        case 3:
            pack(&green, id, other_ids);
            break;
    }
}

void pack(struct Box* box, int id, int *other_ids) {

    //============= filling in section =============//
    sem_wait(&box -> empty);
    sem_wait(&mutex1);
    
    //store ball in temp box
    box -> arr[box -> curSize] = id;
    box -> curSize++;

    //temp box is full
    if (box -> curSize == box -> size) {

        //push contents of temp box to respective balls
        for (int i = 0; i < box -> size; i++) {
            sem_post(&box -> full);
        }

        //"reset" temp box
        box -> curSize = 0;
    }

    sem_post(&mutex1);
    sem_wait(&box -> full);
    //========= end of filling in section =========//

    //============ releasing section ==============//
    sem_wait(&mutex2);

    int j = 0;
    for (int i = 0; i < box -> size; i++) {
        if (box -> arr[i] != id) {
            other_ids[j] = box -> arr[i];
            j++;
        }
    }

    box -> release++;

    //check if all balls have been released
    if (box -> release == box -> size) {
        for (int i = 0; i < box -> size; i++) {
            sem_post(&box -> empty);
        }

        //reset counter
        box -> release = 0;
    }

    sem_post(&mutex2);
    //========= end of releasing section ==========//
}