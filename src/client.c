#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/dlock.h"


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

int client_socket;

volatile int client_running = 1;


/* ============================================================
   SERVER MESSAGE RECEIVER THREAD
   ============================================================ */

void *receive_messages(void *arg)
{
    (void)arg;

    char buffer[BUFFER_SIZE];


    while (client_running)
    {
        memset(buffer, 0, BUFFER_SIZE);


        int bytes =
            read(client_socket,
                 buffer,
                 BUFFER_SIZE - 1);


        if (bytes <= 0)
        {
            /*
             * Server disconnected
             */

            if (client_running)
            {
                printf("\nServer disconnected.\n");
            }

            client_running = 0;

            break;
        }


        buffer[bytes] = '\0';


        printf("\nServer: %s",
               buffer);


        printf("DLock> ");

        fflush(stdout);
    }


    return NULL;
}


/* ============================================================
   MAIN
   ============================================================ */

int main()
{
    struct sockaddr_in server_address;


    /*
     * Create socket
     */

    client_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (client_socket < 0)
    {
        perror("Socket creation failed");

        exit(EXIT_FAILURE);
    }


    /*
     * Configure server address
     */

    server_address.sin_family =
        AF_INET;


    server_address.sin_port =
        htons(PORT);


    server_address.sin_addr.s_addr =
        inet_addr("127.0.0.1");


    /*
     * Connect to server
     */

    if (connect(client_socket,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0)
    {
        perror("Connection failed");

        close(client_socket);

        exit(EXIT_FAILURE);
    }


    printf("========================================\n");
    printf("           DLOCK CLIENT\n");
    printf("========================================\n");

    printf("Connected to DLock Server.\n");


    printf("\nCommands:\n");

    printf("  LOCK FILE_A\n");
    printf("  UNLOCK FILE_A\n");
    printf("  STATUS FILE_A\n");
    printf("  EXIT\n");


    /*
     * Create receiver thread
     */

    pthread_t receiver_thread;


    if (pthread_create(&receiver_thread,
                       NULL,
                       receive_messages,
                       NULL) != 0)
    {
        perror("Receiver thread creation failed");

        close(client_socket);

        exit(EXIT_FAILURE);
    }


    /*
     * Main client loop
     *
     * This thread handles keyboard input.
     */

    char buffer[BUFFER_SIZE];


    while (client_running)
    {
        printf("\nDLock> ");

        fflush(stdout);


        if (fgets(buffer,
                  BUFFER_SIZE,
                  stdin) == NULL)
        {
            break;
        }


        /*
         * EXIT command
         */

        if (strncmp(buffer,
                    "EXIT",
                    4) == 0)
        {
            client_running = 0;

            break;
        }


        /*
         * Send command to server
         */

        if (write(client_socket,
                  buffer,
                  strlen(buffer)) < 0)
        {
            perror("Send failed");

            client_running = 0;

            break;
        }
    }


    /*
     * Close socket
     *
     * This also causes receiver thread's
     * read() to return.
     */

    shutdown(client_socket,
             SHUT_RDWR);

    close(client_socket);


    /*
     * Wait for receiver thread
     */

    pthread_join(receiver_thread,
                 NULL);


    printf("\nDisconnected from server.\n");


    return 0;
}
