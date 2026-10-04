#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/dlock.h"


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

LockResource lock_table[MAX_RESOURCES];

pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;

int server_socket;

volatile sig_atomic_t server_running = 1;

/* Logical client ID */
int next_client_id = 1;


/* ============================================================
   CLIENT INFORMATION
   ============================================================ */

typedef struct
{
    int socket_fd;
    int client_id;

} ClientInfo;


/* ============================================================
   WAITING QUEUE
   ============================================================ */

#define MAX_QUEUE 20

typedef struct
{
    int socket_fd;
    int client_id;

} WaitingClient;


typedef struct
{
    WaitingClient clients[MAX_QUEUE];

    int front;
    int rear;
    int count;

} WaitingQueue;


WaitingQueue resource_queues[MAX_RESOURCES];


/* ============================================================
   SIGNAL HANDLER
   ============================================================ */

void handle_sigint(int signal_number)
{
    (void)signal_number;

    printf("\n\nSIGINT received.\n");
    printf("DLock Server shutting down...\n");

    server_running = 0;

    close(server_socket);
}


/* ============================================================
   INITIALIZE QUEUES
   ============================================================ */

void initialize_queues()
{
    for (int i = 0; i < MAX_RESOURCES; i++)
    {
        resource_queues[i].front = 0;
        resource_queues[i].rear = 0;
        resource_queues[i].count = 0;
    }
}


/* ============================================================
   ENQUEUE CLIENT
   ============================================================ */

int enqueue(int resource_index,
            int socket_fd,
            int client_id)
{
    WaitingQueue *queue =
        &resource_queues[resource_index];

    if (queue->count >= MAX_QUEUE)
    {
        return 0;
    }

    queue->clients[queue->rear].socket_fd =
        socket_fd;

    queue->clients[queue->rear].client_id =
        client_id;

    queue->rear =
        (queue->rear + 1) % MAX_QUEUE;

    queue->count++;

    return 1;
}


/* ============================================================
   DEQUEUE CLIENT
   ============================================================ */

int dequeue(int resource_index,
            int *socket_fd,
            int *client_id)
{
    WaitingQueue *queue =
        &resource_queues[resource_index];

    if (queue->count == 0)
    {
        return 0;
    }

    *socket_fd =
        queue->clients[queue->front].socket_fd;

    *client_id =
        queue->clients[queue->front].client_id;

    queue->front =
        (queue->front + 1) % MAX_QUEUE;

    queue->count--;

    return 1;
}


/* ============================================================
   INITIALIZE LOCK TABLE
   ============================================================ */

void initialize_lock_table()
{
    strcpy(lock_table[0].name, "FILE_A");
    lock_table[0].owner = -1;
    lock_table[0].locked = 0;

    strcpy(lock_table[1].name, "FILE_B");
    lock_table[1].owner = -1;
    lock_table[1].locked = 0;

    strcpy(lock_table[2].name, "DATABASE");
    lock_table[2].owner = -1;
    lock_table[2].locked = 0;

    strcpy(lock_table[3].name, "PRINTER");
    lock_table[3].owner = -1;
    lock_table[3].locked = 0;

    initialize_queues();
}


/* ============================================================
   DISPLAY LOCK TABLE
   ============================================================ */

void display_lock_table()
{
    printf("\n========== LOCK TABLE ==========\n");

    for (int i = 0; i < 4; i++)
    {
        printf("%s : %s",
               lock_table[i].name,
               lock_table[i].locked ?
               "LOCKED" : "FREE");

        if (lock_table[i].locked)
        {
            printf(" (Client %d)",
                   lock_table[i].owner);
        }

        printf("\n");
    }

    printf("================================\n");
}


/* ============================================================
   FIND RESOURCE
   ============================================================ */

int find_resource(char *name)
{
    for (int i = 0; i < 4; i++)
    {
        if (strcmp(lock_table[i].name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}


/* ============================================================
   HANDLE CLIENT
   ============================================================ */

void *handle_client(void *arg)
{
    ClientInfo *info =
        (ClientInfo *)arg;

    int client_socket =
        info->socket_fd;

    int client_id =
        info->client_id;

    free(info);


    char buffer[BUFFER_SIZE];


    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);


        int bytes_read =
            read(client_socket,
                 buffer,
                 BUFFER_SIZE - 1);


        if (bytes_read <= 0)
        {
            break;
        }


        buffer[bytes_read] = '\0';


        buffer[strcspn(buffer, "\n")] = '\0';


        printf("\nClient %d requested: %s\n",
               client_id,
               buffer);


        char command[20];
        char resource_name[RESOURCE_NAME_SIZE];


        memset(command, 0, sizeof(command));
        memset(resource_name, 0, sizeof(resource_name));


        sscanf(buffer,
               "%19s %49s",
               command,
               resource_name);


        /* ====================================================
           LOCK
           ==================================================== */

        if (strcmp(command, "LOCK") == 0)
        {
            int index =
                find_resource(resource_name);


            if (index == -1)
            {
                char *response =
                    "ERROR: Resource not found.\n";

                write(client_socket,
                      response,
                      strlen(response));

                continue;
            }


            pthread_mutex_lock(&lock_mutex);


            /*
             * Resource is FREE
             */

            if (lock_table[index].locked == 0)
            {
                lock_table[index].locked = 1;

                lock_table[index].owner =
                    client_id;


                printf("LOCK GRANTED: %s -> Client %d\n",
                       resource_name,
                       client_id);


                char response[BUFFER_SIZE];

                snprintf(response,
                         BUFFER_SIZE,
                         "SUCCESS: %s locked by Client %d\n",
                         resource_name,
                         client_id);


                write(client_socket,
                      response,
                      strlen(response));
            }


            /*
             * Resource is BUSY
             */

            else
            {
                if (enqueue(index,
                            client_socket,
                            client_id))
                {
                    printf("Client %d added to waiting queue for %s\n",
                           client_id,
                           resource_name);


                    char response[BUFFER_SIZE];

                    snprintf(response,
                             BUFFER_SIZE,
                             "WAITING: %s is locked. You are added to the queue.\n",
                             resource_name);


                    write(client_socket,
                          response,
                          strlen(response));
                }

                else
                {
                    char *response =
                        "ERROR: Waiting queue is full.\n";

                    write(client_socket,
                          response,
                          strlen(response));
                }
            }


            pthread_mutex_unlock(&lock_mutex);
        }


        /* ====================================================
           UNLOCK
           ==================================================== */

        else if (strcmp(command, "UNLOCK") == 0)
        {
            int index =
                find_resource(resource_name);


            if (index == -1)
            {
                char *response =
                    "ERROR: Resource not found.\n";

                write(client_socket,
                      response,
                      strlen(response));

                continue;
            }


            pthread_mutex_lock(&lock_mutex);


            /*
             * Check ownership using logical Client ID
             */

            if (lock_table[index].locked &&
                lock_table[index].owner == client_id)
            {
                int next_socket;
                int next_client_id;


                /*
                 * Check waiting queue
                 */

                if (dequeue(index,
                            &next_socket,
                            &next_client_id))
                {
                    /*
                     * Transfer lock directly
                     * to next waiting client.
                     */

                    lock_table[index].owner =
                        next_client_id;

                    lock_table[index].locked = 1;


                    printf("UNLOCK: Client %d released %s\n",
                           client_id,
                           resource_name);


                    printf("LOCK GRANTED FROM QUEUE: %s -> Client %d\n",
                           resource_name,
                           next_client_id);


                    /*
                     * Notify waiting client
                     */

                    char next_response[BUFFER_SIZE];

                    snprintf(next_response,
                             BUFFER_SIZE,
                             "SUCCESS: %s is now locked by Client %d\n",
                             resource_name,
                             next_client_id);


                    write(next_socket,
                          next_response,
                          strlen(next_response));


                    /*
                     * Notify original owner
                     */

                    char original_response[BUFFER_SIZE];

                    snprintf(original_response,
                             BUFFER_SIZE,
                             "SUCCESS: %s unlocked. Lock transferred to Client %d.\n",
                             resource_name,
                             next_client_id);


                    write(client_socket,
                          original_response,
                          strlen(original_response));
                }


                else
                {
                    /*
                     * No waiting clients.
                     * Resource becomes FREE.
                     */

                    lock_table[index].locked = 0;

                    lock_table[index].owner = -1;


                    printf("UNLOCKED: %s\n",
                           resource_name);


                    char *response =
                        "SUCCESS: Resource unlocked.\n";


                    write(client_socket,
                          response,
                          strlen(response));
                }
            }


            else
            {
                char *response =
                    "ERROR: You do not own this lock.\n";


                write(client_socket,
                      response,
                      strlen(response));
            }


            pthread_mutex_unlock(&lock_mutex);
        }


        /* ====================================================
           STATUS
           ==================================================== */

        else if (strcmp(command, "STATUS") == 0)
        {
            pthread_mutex_lock(&lock_mutex);


            display_lock_table();


            char response[BUFFER_SIZE];

            snprintf(response,
                     BUFFER_SIZE,
                     "%s : %s\n",
                     lock_table[0].name,
                     lock_table[0].locked ?
                     "LOCKED" : "FREE");


            write(client_socket,
                  response,
                  strlen(response));


            pthread_mutex_unlock(&lock_mutex);
        }


        /* ====================================================
           UNKNOWN COMMAND
           ==================================================== */

        else
        {
            char *response =
                "ERROR: Unknown command.\n";


            write(client_socket,
                  response,
                  strlen(response));
        }
    }


    printf("Client %d disconnected.\n",
           client_id);


    close(client_socket);


    return NULL;
}


/* ============================================================
   MAIN
   ============================================================ */

int main()
{
    struct sockaddr_in server_address;


    initialize_lock_table();


    /*
     * Register SIGINT handler
     */

    signal(SIGINT,
           handle_sigint);


    /*
     * Create server socket
     */

    server_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (server_socket < 0)
    {
        perror("Socket creation failed");

        exit(EXIT_FAILURE);
    }


    /*
     * Allow address reuse
     */

    int option = 1;

    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &option,
               sizeof(option));


    /*
     * Configure server address
     */

    server_address.sin_family =
        AF_INET;

    server_address.sin_addr.s_addr =
        INADDR_ANY;

    server_address.sin_port =
        htons(PORT);


    /*
     * Bind
     */

    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("Bind failed");

        close(server_socket);

        exit(EXIT_FAILURE);
    }


    /*
     * Listen
     */

    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");

        close(server_socket);

        exit(EXIT_FAILURE);
    }


    printf("========================================\n");
    printf("          DLOCK SERVER\n");
    printf("========================================\n");

    printf("Server listening on port %d...\n",
           PORT);


    /*
     * Accept clients
     */

    while (server_running)
    {
        struct sockaddr_in client_address;

        socklen_t client_length =
            sizeof(client_address);


        /*
         * Allocate ClientInfo
         */

        ClientInfo *info =
            malloc(sizeof(ClientInfo));


        if (info == NULL)
        {
            perror("malloc failed");

            continue;
        }


        /*
         * Accept connection
         */

        info->socket_fd =
            accept(server_socket,
                   (struct sockaddr *)&client_address,
                   &client_length);


        if (info->socket_fd < 0)
        {
            free(info);

            if (!server_running)
            {
                break;
            }

            perror("Accept failed");

            continue;
        }


        /*
         * Assign logical Client ID
         */

        info->client_id =
            next_client_id++;

        
        printf("\nNew client connected.\n");

        printf("Client %d connected (Socket FD: %d)\n",
               info->client_id,
               info->socket_fd);


        /*
         * Create client thread
         */

        pthread_t thread;


        if (pthread_create(&thread,
                           NULL,
                           handle_client,
                           info) != 0)
        {
            perror("Thread creation failed");

            close(info->socket_fd);

            free(info);

            continue;
        }


        pthread_detach(thread);
    }


    close(server_socket);


    pthread_mutex_destroy(&lock_mutex);


    printf("Server stopped.\n");


    return 0;
}
