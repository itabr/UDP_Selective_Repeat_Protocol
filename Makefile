# declare variable
C = gcc
CFLAGS= -Wall -o
# build an executable named server from server.c
# build an executable named client from client.c

all: server.c 
	@$(C) $(CFLAGS) server -g server.c packet.c
	@$(C) $(CFLAGS) client -g client.c packet.c

server: server.c
	@$(C) $(CFLAGS) server -g server.c packet.c

client: client.c
	@$(C) $(CFLAGS) client -g client.c packet.c