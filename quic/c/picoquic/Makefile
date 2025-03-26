CC = gcc
CFLAGS = -Wall -Werror -g
LDFLAGS = -lpicoquic-core -lpicotls-core -lpicotls-openssl -lpicotls-fusion -lpicotls-minicrypto  -lcrypto -lssl

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

clean:
	rm -f server client