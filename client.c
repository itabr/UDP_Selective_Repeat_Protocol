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
#include "linked_list.h"

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

    int expected_packet = 0;

    while (1)
    {   
        if(expected_packet == 30){
            expected_packet = 0;
        }
        //bzero(buf, BUFSIZE); 
        //n = recvfrom(sockfd, buf, sizeof(buf), 0, &serveraddr, &serverlen);

        n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &serveraddr, &serverlen); 
        if (n < 0) 
          error("ERROR in recvfrom");
        else if (n > 0) 
        {
            if(received_packet.seq_num == expected_packet){
                fwrite(received_packet.data , sizeof(char) , sizeof(received_packet.data) , fp );
                expected_packet = expected_packet +1;
		struct node *temp = find(expected_packet);

                while(temp){
                    fwrite(temp->new_packet.data , sizeof(char) , 			    sizeof(temp->new_packet.data) , fp );
                    delete(temp->key);
                    expected_packet = expected_packet + 1;
			if(expected_packet == 30){
           		 expected_packet = 0;
        		}
                    temp = find(expected_packet);
                }
            }
            else if(received_packet.seq_num > expected_packet){

                insertFirst(received_packet.seq_num,received_packet);

                struct node *temp = find(expected_packet);

                while(temp){
                    fwrite(temp->new_packet.data , sizeof(char) , sizeof(temp->new_packet.data) , fp );

                    delete(temp->key);

                    expected_packet = expected_packet + 1;
                    temp = find(expected_packet);
                }
            }

            // fwrite(received_packet.data , sizeof(char) , sizeof(received_packet.data) , fp );

		        struct node *temp = find(expected_packet);

                while(temp){
                    fwrite(temp->new_packet.data , sizeof(char) , sizeof(temp->new_packet.data) , fp );

                    delete(temp->key);

                    expected_packet = expected_packet + 1;
                    temp = find(expected_packet);
                }

            printf("Receiving packet %d\n", received_packet.seq_num); 
	        printf("packet nummmmm %d\n", expected_packet);
            //printf("Receiving from server: %s\n", buf);
            /*
            printf("len = %d\n", received_packet.len); 
            printf("seq num = %d\n", received_packet.seq_num); 
            printf("data = %s\n", received_packet.data); 
            printf("data size = %d\n", sizeof(received_packet.data));
            printf("checksum = %d\n", received_packet.cs);  
            */ 

            //bzero(buf, BUFSIZE);
            //sprintf(buf, "Client: Sending ACK #%d", received_packet.seq_num);   
            //n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen); 
            int ack_num = received_packet.seq_num;
            printf("ack num = %d\n", ack_num); 

            printf("time stamp = %Lf\n", received_packet.timestamp); 
            n = sendto(sockfd, &ack_num, sizeof(ack_num), 0, &serveraddr, serverlen); 
            if (n < 0)
                error("ERROR in sendto"); 
            printf("Sending ACK #%d\n", received_packet.seq_num); 
        }
        //bzero(buf, BUFSIZE); 

           // struct packet junk = {1024, 10, "0", 0};

           // insertFirst(1,junk);
           // insertFirst(2,junk);
           // insertFirst(3,junk);
           // insertFirst(4,junk);
           // insertFirst(5,junk);
           // insertFirst(6,junk);
        
           //print list
           printList();
    }


    return 0;
}
