# declare variable
C = gcc
CFLAGS= -Wall -o 
# build an executable named test from test.c
all: server.c 
	@$(C) $(CFLAGS) server -g server.c
	@$(C) $(CFLAGS) client -g client.c

server: server.c
	@$(C) $(CFLAGS) server -g server.c

client: client.c
	@$(C) $(CFLAGS) client -g client.c
