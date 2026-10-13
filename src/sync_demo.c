#include <stdio.h>
#include <pthread.h>

int shared_data = 0;
int data_ready = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

void *producer(void *arg)
{
    (void)arg;

    for (int i = 1; i <= 5; i++) {
        pthread_mutex_lock(&mutex);

        // Wait until the previous value has been consumed
        while (data_ready) {
            pthread_cond_wait(&condition, &mutex);
        }

        shared_data = i;
        data_ready = 1;

        printf("Producer created: %d\n", shared_data);

        // Wake the consumer
        pthread_cond_broadcast(&condition);

        // Wait until the consumer takes this value
        while (data_ready) {
            pthread_cond_wait(&condition, &mutex);
        }

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

void *consumer(void *arg)
{
    (void)arg;

    for (int i = 1; i <= 5; i++) {
        pthread_mutex_lock(&mutex);

        // Wait until data is available
        while (!data_ready) {
            pthread_cond_wait(&condition, &mutex);
        }

        printf("Consumer received: %d\n", shared_data);

        // Mark the value as consumed
        data_ready = 0;

        // Wake the producer
        pthread_cond_broadcast(&condition);

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int main(void)
{
    pthread_t p, c;

    // Start both threads
    pthread_create(&c, NULL, consumer, NULL);
    pthread_create(&p, NULL, producer, NULL);

    // Wait for both threads to finish
    pthread_join(p, NULL);
    pthread_join(c, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condition);

    printf("All five values processed successfully!\n");

    return 0;
}
