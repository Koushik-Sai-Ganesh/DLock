#ifndef DLOCK_H
#define DLOCK_H

#define PORT 8080
#define MAX_RESOURCES 10
#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define RESOURCE_NAME_SIZE 50

typedef struct {
    char name[RESOURCE_NAME_SIZE];
    int owner;
    int locked;
} LockResource;

#endif
