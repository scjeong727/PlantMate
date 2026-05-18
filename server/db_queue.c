#include "db_queue.h"
#include <string.h>

void db_queue_init(DBQueue* q)
{
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void db_queue_destroy(DBQueue* q)
{
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void db_queue_push(DBQueue* q, const DBRequest* req)
{
    pthread_mutex_lock(&q->mutex);

    while (q->count == DB_QUEUE_CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->items[q->rear] = *req;
    q->rear = (q->rear + 1) % DB_QUEUE_CAPACITY;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

void db_queue_pop(DBQueue* q, DBRequest* out)
{
    pthread_mutex_lock(&q->mutex);

    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    *out = q->items[q->front];
    memset(&q->items[q->front], 0, sizeof(DBRequest));
    q->front = (q->front + 1) % DB_QUEUE_CAPACITY;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}
