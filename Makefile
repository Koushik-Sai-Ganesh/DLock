CC = gcc

CFLAGS = -Wall -Wextra -pthread

INCLUDE = -Iinclude

SERVER = dlock_server

CLIENT = dlock_client


all: $(SERVER) $(CLIENT)


$(SERVER): src/server.c
	$(CC) $(CFLAGS) $(INCLUDE) src/server.c -o $(SERVER)


$(CLIENT): src/client.c
	$(CC) $(CFLAGS) $(INCLUDE) src/client.c -o $(CLIENT)


clean:
	rm -f $(SERVER) $(CLIENT)
