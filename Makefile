CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: clean build

default: build

build: server.c client.c
	gcc -Wall -Wextra -Wno-unused-variable -o server server.c
	gcc -Wall -Wextra -Wno-unused-variable -o client client.c

clean:
	rm -f server client output.txt project2.zip

zip: 
	zip project2.zip server.c client.c utils.h report.txt Makefile README
