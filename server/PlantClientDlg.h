#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <pthread.h>

#define COMMAND_QUEUE_SIZE 100

typedef struct {
    int plant_id;
    int duration;
} WaterCommand;

typedef struct {
    WaterCommand items[COMMAND_QUEUE_SIZE];
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} CommandQueue;

void command_queue_init(CommandQueue* q);
void command_queue_push(CommandQueue* q, WaterCommand cmd);
int command_queue_pop(CommandQueue* q, WaterCommand* cmd);

#endif
