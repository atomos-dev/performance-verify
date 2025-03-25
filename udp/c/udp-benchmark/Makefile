CC = gcc
CFLAGS = -Wall -O2 -Werror
LDFLAGS =

all: udp_server udp_client

udp_server: udp_server.c
	$(CC) $(CFLAGS) -o udp_server udp_server.c $(LDFLAGS)

udp_client: udp_client.c
	$(CC) $(CFLAGS) -o udp_client udp_client.c $(LDFLAGS)

clean:
	rm -f udp_server udp_client *.o

.PHONY: all clean