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
#define HEADERLENGTH 32

#define MAX( a, b ) ( ( a > b) ? a : b ) 
inline int max ( int a, int b ) { return a > b ? a : b; }

#define MIN( a, b ) ( ( a < b) ? a : b ) 
inline int min ( int a, int b ) { return a < b ? a : b; }



/* 
 * error - wrapper for perror
 */

void error(char *msg) {
	perror(msg);
	exit(0);
}

long double get_timestamp()
{
	struct timeval timer_usec;
	long double timestamp_usec; 
	if (!gettimeofday(&timer_usec, NULL))
	{
		timestamp_usec = ((long double)timer_usec.tv_sec) * 100000011 
			+ (long double) timer_usec.tv_usec; 
	}

	//printf("timestamp = %lld microseconds\n", timestamp_usec); 
	return timestamp_usec; 
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
	bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(portno);

	bzero(buf, BUFSIZE);
	strcpy(buf, filename); 


	/* send the filename to the server */

	struct packet file_name_packet = {HEADERLENGTH + strlen(filename),0,0,0,0,0,0,0};
	strcpy(file_name_packet.data,filename);
	file_name_packet.size = strlen(filename);


	serverlen = sizeof(serveraddr); 
	long double time_sent = get_timestamp(); 

	/* handshake */

	struct packet starter = {HEADERLENGTH,0,0,0,0,0,0,0};
	starter.flag = 5;
    starter.timestamp = get_timestamp();
    printf("Sending packet SYN\n"); 
    n = sendto(sockfd, &starter, sizeof(starter), 0, &serveraddr, serverlen);

    int established = 1;
    int last_received_seq = 0; 

    while(established){
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        n = select(sockfd + 1, &readfds, NULL, NULL, &timeout); 
        
        if(n == 0 && get_timestamp() - time_sent > 500000){
            printf("Sending packet Retransmission SYN\n");
            n = sendto(sockfd, &starter, sizeof(starter), 0, &serveraddr, serverlen);
        }
        else if(n > 0){
            n = recvfrom(sockfd, &starter, sizeof(starter), 0, &serveraddr, &serverlen); 
            printf("Receiving packet %d\n", starter.seq_num); 
            if(starter.flag == 6){
                last_received_seq = starter.seq_num + HEADERLENGTH; 
                established = 0;
            }
        }
    }

	/* send the filename to the server */

    file_name_packet.seq_num = last_received_seq; 
    printf("Sending packet %d\n", last_received_seq); 
	n = sendto(sockfd, &file_name_packet, sizeof(file_name_packet), 0, &serveraddr, serverlen);

	if (n < 0) 
		error("ERROR in sendto");

	struct packet received_packet; 
	bzero(&received_packet, sizeof(struct packet)); 

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;

	int filename_received = 0; 
	while (!filename_received)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		n = select(sockfd + 1, &readfds, NULL, NULL, &timeout); 
		if (n == 0 && get_timestamp() - time_sent > 500000)
		{
            printf("Sending packet %d Retransmission\n", last_received_seq); 
			time_sent = get_timestamp(); 
			n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
			if (n < 0) 
				error("ERROR in sendto");
		}
		else if (n > 0)
        {
            n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &serveraddr, &serverlen); 
            if (strcmp(received_packet.data, "File not found") == 0)
            {
                printf("Requested file not found.\n"); 
                exit(-1);
            }
            else if (strcmp(received_packet.data, "File received") == 0)
			 filename_received = 1; 
        }
	}

	FILE *fp;

	fp = fopen("received.data", "wb+");

	int expected_packet = 0;
	int complete = 0; 

	int size = 0;
    last_received_seq = 0;

	while (!complete)
	{   
		n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &serveraddr, &serverlen); 
		if (n < 0) 
			error("ERROR in recvfrom");
		else if (n > 0)
		{
			if (received_packet.packet_num == expected_packet && received_packet.flag != 3){

				size = sizeof(received_packet.data);
				fwrite(received_packet.data , sizeof(char) , MIN(size,received_packet.size) , fp );

				expected_packet = expected_packet + 1;
				struct node *temp = find(expected_packet);

				while(temp){

					size = sizeof(temp->new_packet.data);
					fwrite(temp->new_packet.data , sizeof(char) , MIN(size,temp->new_packet.size), fp );

					delete(temp->key);
					expected_packet = expected_packet + 1;
					temp = find(expected_packet);
				}
			}
			else if(received_packet.packet_num > expected_packet && received_packet.flag != 3){
				if (find(received_packet.packet_num) == NULL)
					insertFirst(received_packet.packet_num,received_packet);

				struct node *temp = find(expected_packet);

				while(temp){

					size = sizeof(temp->new_packet.data);
					fwrite(temp->new_packet.data , sizeof(char) , MIN(size,temp->new_packet.size) , fp );

					delete(temp->key);

					expected_packet = expected_packet + 1;
					temp = find(expected_packet);
				}
			}
			if (received_packet.flag == 3) { // received FIN from server
				// send ACK to server
				int received_FIN = 1; 

                printf("Receiving packet %d FIN\n", received_packet.seq_num);   

				int ack = received_packet.packet_num;  
                printf("Sending client FIN-ACK packet %d\n", received_packet.seq_num);
				long double time_sent_ack = get_timestamp(); 
				n = sendto(sockfd, &ack, sizeof(ack), 0, &serveraddr, serverlen); 
				if (n < 0)
					error("ERROR in sendto"); 

                last_received_seq = received_packet.seq_num; 

				while (received_FIN)
				{
					fd_set readfds;
					FD_ZERO(&readfds);
					FD_SET(sockfd, &readfds);
					n = select(sockfd + 1, &readfds, NULL, NULL, &timeout); 

					if (n > 0)
					{
						n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &serveraddr, &serverlen); 
						if (received_packet.flag == 3) { 
                            last_received_seq = received_packet.seq_num;
                            printf("Sending packet %d Retransmission FIN\n", received_packet.seq_num); 
							time_sent_ack = get_timestamp();  
							n = sendto(sockfd, &ack, sizeof(ack), 0, &serveraddr, serverlen);
							if (n < 0) 
								error("ERROR in sendto");
						}
					}

					else if (n == 0 && get_timestamp() - time_sent_ack > 500000) // server received ACK successfully
					{
						// now send FIN
						received_FIN = 0;
						int received_ACK = 0; 

						struct packet fin = {HEADERLENGTH, (received_packet.packet_num + 1) % 30, received_packet.packet_num + 1, 0, NULL, 3, get_timestamp()}; // flag = 3 denotes FIN
						fin.flag = 3; 
                        fin.seq_num = last_received_seq + HEADERLENGTH;
                        if (fin.seq_num > 30720)
                            fin.seq_num -= 30720; 

                        printf("Sending packet %d Client FIN\n", fin.seq_num);
						n = sendto(sockfd, &fin, sizeof(fin), 0, &serveraddr, serverlen); 
						if (n < 0)
							error("ERROR in sendto");

						while (!received_ACK) 
						{
							FD_ZERO(&readfds);
							FD_SET(sockfd, &readfds); 
							n = select(sockfd + 1, &readfds, NULL, NULL, &timeout); 
							if (n > 0) // received ACK from server 
							{
								struct packet final_ack; 
								bzero(&final_ack, sizeof(final_ack)); 
								n = recvfrom(sockfd, &final_ack, sizeof(final_ack), 0, &serveraddr, &serverlen); 
								if (n < 0)
									error("ERROR in recvfrom"); 
								if (final_ack.packet_num == expected_packet + 1) 
								{
                                    printf("Receiving packet %d (server FIN-ACK)\n\n\n", fin.seq_num); 
									fclose(fp); 
									close(sockfd); 
									received_ACK = 1; 
									complete = 1; 
								}
							}
							else if (n == 0 && get_timestamp() - fin.timestamp > 500000)
							{
                                printf("Sending packet %d Retransmission Client FIN\n", fin.seq_num);  
								fin.timestamp = get_timestamp();
								n = sendto(sockfd, &fin, sizeof(fin), 0, &serveraddr, serverlen);
								if (n < 0)
									error("ERROR in sendto");
							}
						}
					}
				}

			}

			if (!complete)
			{
                printf("Receiving packet %d\n", received_packet.seq_num); 

				int ack_num = received_packet.packet_num;
				printf("ack num = %d\n", ack_num); 

                printf("Sending packet %d\n", received_packet.seq_num); 
				n = sendto(sockfd, &ack_num, sizeof(ack_num), 0, &serveraddr, serverlen); 
				if (n < 0)
					error("ERROR in sendto"); 
			}
		}
	}

	return 0;
}
