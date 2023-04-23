#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "eventbuf.h"
#include <pthread.h>
#include <semaphore.h>

int num_producer;
int num_consumer;
int num_event;
int num_outstanding;

struct eventbuf *event_buff;

sem_t *mutex;
sem_t *items;
sem_t *spaces;

sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

void *run_producer(void *arg)
{
    int *id = arg;
    
    for(int i = 0; i < num_event; i++)
    {   
        int event_num = *id * 100 + i;
        sem_wait(spaces);
        sem_wait(mutex);
        printf("P%d: adding event %d\n", *id, event_num);
        eventbuf_add(event_buff, event_num);
        sem_post(mutex);
        sem_post(items);
    }
    printf("P%d: exiting\n", *id);
    return NULL;
}

void *run_consumer(void *arg)
{
    int *id = arg;
    while(1)
    {
        sem_wait(items);
        sem_wait(mutex);
        if(eventbuf_empty(event_buff))
        {
            sem_post(mutex);
            break;
        }
        int event_num = eventbuf_get(event_buff);
        printf("C%d: got event %d\n", *id, event_num);
        sem_post(mutex);
        sem_post(spaces);
    }
    printf("C%d: exiting\n", *id);
    return NULL;
}

void *create_event(void){
    event_buff = eventbuf_create();
    mutex = sem_open_temp("mutex", 1);
    items = sem_open_temp("items", 0);
    spaces = sem_open_temp("spaces", num_outstanding);
    return NULL;
}

void prod_cons_thread(){
    pthread_t *prod_thread = calloc(num_producer, sizeof *prod_thread);
    int *prod_thread_id = calloc(num_producer, sizeof *prod_thread_id);
    pthread_t *cons_thread = calloc(num_consumer , sizeof *cons_thread);
    int *cons_thread_id = calloc(num_consumer, sizeof *cons_thread_id);
    for (int i = 0; i < num_producer; i++) {
        prod_thread_id[i] = i;
        pthread_create(prod_thread + i, NULL, run_producer, &prod_thread_id[i]);
    }

    for (int i = 0; i < num_consumer; i++){
        cons_thread_id[i] = i;
        pthread_create(cons_thread + i, NULL, run_consumer,  &cons_thread_id[i]);
    }

    for (int i = 0; i < num_producer; i++){
        pthread_join(prod_thread[i], NULL);
    }
    
    for (int i = 0; i < num_consumer ; i++){
        sem_post(items);
    }
    
    for (int i = 0; i < num_consumer ; i++){
        pthread_join(cons_thread[i], NULL);
    }
}

int main(int argc, char *argv[]) 
{
    if (argc != 5) {
        fprintf(stderr, "usage: pcseml num_producer num_consumer num_event num_outstanding \n");
        exit(1);
    }
    num_producer = atoi(argv[1]);
    num_consumer = atoi(argv[2]);
    num_event = atoi(argv[3]);
    num_outstanding = atoi(argv[4]);

    create_event();

    prod_cons_thread();

    eventbuf_free(event_buff);
}