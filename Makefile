CC = gcc
CFLAGS = -Wall -std=c11 -pthread

compile: client server

client: build/client.o build/ttt.o
	$(CC) $(CFLAGS) build/client.o build/ttt.o -o client

server: build/server.o build/ttt.o
	$(CC) $(CFLAGS) build/server.o build/ttt.o -o server

build:
	mkdir -p build

build/ttt.o: include/ttt.h src/ttt.c build
	$(CC) $(CFLAGS) -c src/ttt.c -o build/ttt.o

build/client.o: include/ttt.h src/client.c build
	$(CC) $(CFLAGS) -c src/client.c -o build/client.o

build/server.o: include/ttt.h src/server.c build
	$(CC) $(CFLAGS) -c src/server.c -o build/server.o

clean:
	rm -rf build client server
