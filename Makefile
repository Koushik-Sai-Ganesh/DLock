CC = gcc
CFLAGS = -Wall -Wextra -pthread
INCLUDE = -Iinclude

all: dlock_server dlock_client file_demo sync_demo

dlock_server: src/server.c include/dlock.h
	$(CC) $(CFLAGS) $(INCLUDE) src/server.c -o dlock_server

dlock_client: src/client.c include/dlock.h
	$(CC) $(CFLAGS) $(INCLUDE) src/client.c -o dlock_client

file_demo: src/file_demo.c
	$(CC) -Wall -Wextra src/file_demo.c -o file_demo

sync_demo: src/sync_demo.c
	$(CC) $(CFLAGS) src/sync_demo.c -o sync_demo

clean:
	rm -f dlock_server dlock_client file_demo sync_demo

.PHONY: all clean
