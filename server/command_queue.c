#include "command_queue.h"

void command_queue_init(CommandQueue* q)
{
    if (!q) return;

    q->front = 0;
    q->rear = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void command_queue_destroy(CommandQueue* q)
{
    if (!q) return;

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void command_queue_push(CommandQueue* q, WaterCommand cmd)
{
    if (!q) return;

    pthread_mutex_lock(&q->mutex);

    while (q->count == COMMAND_QUEUE_SIZE)
        pthread_cond_wait(&q->not_full, &q->mutex);

    q->items[q->rear] = cmd;
    q->rear = (q->rear + 1) % COMMAND_QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int command_queue_pop(CommandQueue* q, WaterCommand* cmd)
{
    if (!q || !cmd) return 0;

    pthread_mutex_lock(&q->mutex);

    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    *cmd = q->items[q->front];
    q->front = (q->front + 1) % COMMAND_QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}
