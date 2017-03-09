# declare variable
C = gcc
CFLAGS= -w -Wall -o
# build an executable named server from server.c
# build an executable named client from client.c

all: server.c 
	@$(C) $(CFLAGS) server -g server.c
	@$(C) $(CFLAGS) client -g client.c

server: server.c
	@$(C) $(CFLAGS) server -g server.c

client: client.c
	@$(C) $(CFLAGS) client -g client.c