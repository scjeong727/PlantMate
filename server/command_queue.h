#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <pthread.h>

#define COMMAND_QUEUE_SIZE 100

typedef struct {
    int plant_id;
    int duration;
    int owner_sock;
} WaterCommand;

typedef struct {
    WaterCommand items[COMMAND_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} CommandQueue;

void command_queue_init(CommandQueue* q);
void command_queue_destroy(CommandQueue* q);
void command_queue_push(CommandQueue* q, WaterCommand cmd);
int command_queue_pop(CommandQueue* q, WaterCommand* cmd);

#endif
