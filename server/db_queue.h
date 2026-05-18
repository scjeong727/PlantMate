#ifndef DB_QUEUE_H
#define DB_QUEUE_H

#include <pthread.h>

#define DB_QUERY_SIZE 4096
#define DB_QUEUE_CAPACITY 256

typedef enum {
    DB_REQ_CLIENT,
    DB_REQ_SENSOR,
    DB_REQ_SHUTDOWN
} DBRequestType;

typedef struct {
    DBRequestType type;
    int client_sock;
    char query[DB_QUERY_SIZE];
} DBRequest;

typedef struct {
    DBRequest items[DB_QUEUE_CAPACITY];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} DBQueue;

void db_queue_init(DBQueue* q);
void db_queue_destroy(DBQueue* q);
void db_queue_push(DBQueue* q, const DBRequest* req);
void db_queue_pop(DBQueue* q, DBRequest* out);

#endif
