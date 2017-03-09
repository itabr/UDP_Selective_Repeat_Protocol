/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "packet.h"

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char *filename;

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    // printf("%s",filename);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    bzero(buf, BUFSIZE);
    strcpy(buf, filename); 

    /* send the filename to the server */
    serverlen = sizeof(serveraddr); 
    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */
    buf[sizeof(filename)] = '\n'; 

    struct packet received_packet; 
    bzero(&received_packet, sizeof(struct packet)); 

    FILE *fp;

    fp = fopen("output", "a+");

    while (1)
    {
        //bzero(buf, BUFSIZE); 
        //n = recvfrom(sockfd, buf, sizeof(buf), 0, &serveraddr, &serverlen);
        n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &serveraddr, &serverlen); 
        if (n < 0) 
          error("ERROR in recvfrom");
        else if (n > 0) 
        {

            fwrite(received_packet.data , sizeof(char) , sizeof(received_packet.data) , fp );

            printf("Receiving packet %d\n", received_packet.seq_num); 
            //printf("Receiving from server: %s\n", buf);
            /*
            printf("len = %d\n", received_packet.len); 
            printf("seq num = %d\n", received_packet.seq_num); 
            printf("data = %s\n", received_packet.data); 
            printf("data size = %d\n", sizeof(received_packet.data));
            printf("checksum = %d\n", received_packet.cs);  
            */ 

            bzero(buf, BUFSIZE);
            sprintf(buf, "Client: Sending ACK #%d", received_packet.seq_num);   
            n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen); 
            if (n < 0)
                error("ERROR in sendto"); 
            printf("Sending ACK #%d\n", received_packet.seq_num); 
        }
        //bzero(buf, BUFSIZE); 
    }

    return 0;
}