# MsQuic 简单测试程序 Makefile

CC = gcc
CFLAGS = -Wall -Werror -g -I/home/hliu/Source/quic-testing/ms-quic/msquic/src/inc
LDFLAGS = -lmsquic -lnuma -L/home/hliu/Source/quic-testing/ms-quic/msquic/build-static/bin/Release/

all: client server

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

clean:
	rm -f client server

.PHONY: all clean
