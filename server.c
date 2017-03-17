/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "packet.h"
#include <math.h>
#include <sys/time.h>
#include <sys/timeb.h> 
#define BUFSIZE 1024
#define DATASIZE 992
#define HEADERLENGTH 32 

/*
 * error - wrapper for perror
 */

void error(char *msg) {
	perror(msg);
	exit(1);
}

char calc_checksum(char* data)
{
	int checksum = 0; 
	int size = DATASIZE; 
	while (size-- != 0)
		checksum -= *data++; 
	return checksum; 
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
	return timestamp_usec; 
}

int main(int argc, char **argv) {
	int sockfd; /* socket */
	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char buf[BUFSIZE]; /* message buf */
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */

	int window_size = 5;
	srand(time(NULL)); // seed random # generator


	FILE *fp;

	/* 
	 * check command line arguments 
	 */

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]);

	/* 
	 * socket: create the parent socket 
	 */

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	/* setsockopt: Handy debugging trick that lets 
	 * us rerun the server immediately after we kill it; 
	 * otherwise we have to wait about 20 secs. 
	 * Eliminates "ERROR on binding: Address already in use" error. 
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
			(const void *)&optval , sizeof(int));

	/*
	 * build the server's Internet address
	 */

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	/* 
	 * bind: associate the parent socket with a port 
	 */
	if (bind(sockfd, (struct sockaddr *) &serveraddr, 
				sizeof(serveraddr)) < 0) 
		error("ERROR on binding");

	clientlen = sizeof(clientaddr);
	int seq_num; 

	while (1) {

		bzero(buf, BUFSIZE);

		struct packet file_name_packet = {HEADERLENGTH,0,0,0,0,0,0,0};

		n = recvfrom(sockfd, &file_name_packet, sizeof(file_name_packet), 0,(struct sockaddr *) &clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");

		/* 
		 * gethostbyaddr: determine who sent the datagram
		 */

		hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		if (hostp == NULL)
			error("ERROR on gethostbyaddr");
		hostaddrp = inet_ntoa(clientaddr.sin_addr);
		if (hostaddrp == NULL)
			error("ERROR on inet_ntoa\n");


		// SYN-ACK
		if(file_name_packet.flag == 5){
			printf("Receiving packet SYN\n"); 
			file_name_packet.flag = 6;  
			/* choose a random start # between 0 and 30720 for the sequence # */
			seq_num = rand() % 30720; 
			printf("starting seq num = %d\n", seq_num); 
			file_name_packet.seq_num = seq_num; 
			printf("Sending packet %d 5120 SYN\n", file_name_packet.seq_num); 
			n = sendto(sockfd, &file_name_packet, sizeof(file_name_packet), 0, (struct sockaddr*)&clientaddr, clientlen); 
		}

		else{ 
			seq_num += HEADERLENGTH; 
			printf("Receiving packet %d\n", seq_num); 

			char *filename = file_name_packet.data;
			int file_found = 0; 
			seq_num += strlen(filename); 

			fp = fopen(filename, "rb");

			if (fp == NULL){
				struct packet not_found = {1024, -1, -1, -1, "File not found", 0, 0}; 
				n = sendto(sockfd, &not_found, sizeof(not_found), 0, (struct sockaddr*)&clientaddr, clientlen); 
				printf("File not found.\n\n"); 
			}
			else
			{
				struct packet file_ack = {1024, -1, -1, -1, "File received", 0, 0}; 
				n = sendto(sockfd, &file_ack, sizeof(file_ack), 0, (struct sockaddr*)&clientaddr, clientlen); 
				file_found = 1; 
			}


			if (file_found) 
			{
				// get size of the file so we know how much to read 
				struct stat stats; 
				stat(filename, &stats);

				int file_size = stats.st_size; 

				int num_packets = ceil((double)file_size / DATASIZE);

				struct packet* packets = malloc(num_packets * sizeof(struct packet)); 

				int packet_num = 0;
				char data[DATASIZE];

				/* for timeout */
				struct timeval timeout;
				timeout.tv_sec = 0;
				timeout.tv_usec = 500000;

				int temp = file_size;

				while (!feof(fp)) 
				{
					struct packet pack = {1024, packet_num % 30, packet_num, calc_checksum(data), data, 0, 0}; 
					size_t read_length = fread(pack.data, sizeof(char), DATASIZE, fp); 
					pack.cs = calc_checksum(pack.data); 
					pack.len = HEADERLENGTH + read_length; // size of packet = size of header + size of data 
					pack.seq_num = seq_num; 
					seq_num += pack.len; 
					if (seq_num > 30720)
						seq_num -= 30720; 

					pack.size = temp;
					temp = temp - DATASIZE;

					if (read_length == 0)
					{
						error("Could not read file.\n"); 
					}  

					*(packets + packet_num) = pack; 
					packet_num++; 
				}

				int window_start = 0; // start index for the window 
				int next_packet_num = 0; // packet number of the next packet to be sent  

				while (window_start < num_packets)
				{
					while (next_packet_num - window_start < window_size && next_packet_num < num_packets)
						// have space in the window to send a new packet and haven't sent all the packets in the file yet 
					{
						printf("Sending packet %d 5120\n", packets[next_packet_num].seq_num); 

						packets[next_packet_num].timestamp = get_timestamp();  

						n = sendto(sockfd, &packets[next_packet_num], sizeof(packets[next_packet_num]), 0, (struct sockaddr*)&clientaddr, clientlen); 
						if (n < 0)
							error("ERROR in sendto");

						packets[next_packet_num].flag = 1; // sent packet, now waiting for ACK 
						next_packet_num++;  
					}

					bzero(buf, BUFSIZE);
					int ack_num;
					bzero(&ack_num, sizeof(ack_num)); 

					fd_set readfds; 
					FD_ZERO(&readfds); 
					FD_SET(sockfd, &readfds); 

					n = select(sockfd + 1, &readfds, NULL, NULL, &timeout); 
					if (n > 0)
					{ 
						printf("received data\n"); 
						int k = recvfrom(sockfd, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&clientaddr, &clientlen); 
						printf("Server received ack num = %d\n", ack_num); 
						printf("Receiving packet %d\n", packets[ack_num].seq_num);

						if (ack_num < next_packet_num)
						{
							packets[ack_num].flag = 2; 
							if (ack_num == window_start)
							{
								packets[window_start].flag = 2; 
								while (packets[window_start].flag == 2)
								{
									window_start++; // move the window forward to the first unacked packet
								}
							}
						}
					}
					else if (n == 0)
					{
						printf("no data received\n"); 
						int i;
						for (i = window_start; i < window_start + window_size; i++)
						{
							if (packets[i].flag == 1)
							{
								struct timeval timer_usec;
								long double timestamp_usec; 
								gettimeofday(&timer_usec, NULL);
								timestamp_usec = ((long double)timer_usec.tv_sec) * 100000011 + (long double) timer_usec.tv_usec;  

								if (timestamp_usec - packets[i].timestamp >= 500000)
								{
									packets[i].timestamp = get_timestamp();  
									printf("Sending packet %d 5120 Retransmission\n", packets[i].seq_num); 
									n = sendto(sockfd, &packets[i], sizeof(packets[i]), 0, (struct sockaddr*)&clientaddr, clientlen); 
									if (n < 0)
										error("ERROR in retransmit"); 
								}
							}
						}
					}
				}
				// finished transmitting all packets, do the FIN/FIN-ACK procedure 
				struct packet fin = {1024, num_packets % 30, num_packets, 0, NULL, 3, get_timestamp()}; // flag = 3 denotes FIN
				fin.flag = 3;
				fin.seq_num = seq_num; 
				seq_num += fin.len; 
				if (seq_num > 30720)
					seq_num -= 30720;  

				printf("Sending packet %d 5120 FIN\n", fin.seq_num); 
				n = sendto(sockfd, &fin, sizeof(fin), 0, (struct sockaddr*)&clientaddr, clientlen); 
				if (n < 0)
					error("ERROR in sendto"); 

				int fin_wait1 = 1; 
				while (fin_wait1)
				{
					fd_set readfds; 
					FD_ZERO(&readfds); 
					FD_SET(sockfd, &readfds); 
					n = select(sockfd + 1, &readfds, NULL, NULL, &timeout); 
					if (n == 0 && get_timestamp() - fin.timestamp > 500000)
					{
						printf("Sending packet %d 5120 Retransmission FIN\n", fin.seq_num);
						fin.timestamp = get_timestamp(); 
						n = sendto(sockfd, &fin, sizeof(fin), 0, &clientaddr, clientlen);
						if (n < 0) 
							error("ERROR in sendto");
					}
					else if (n > 0)
					{ 
						int ack_num; 
						bzero(&ack_num, sizeof(ack_num)); 
						n = recvfrom(sockfd, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&clientaddr, &clientlen);
						if (n < 0)
							error("ERROR in recvfrom"); 
						if (ack_num == fin.packet_num) // received FIN-ACK, enter fin wait 2 state 
						{
							printf("Receiving packet %d\n", fin.seq_num); 
							fin_wait1 = 0;
						}
						else
						{
							printf("Receiving packet %d\n", packets[ack_num].seq_num); 
						}
					} 
				}

				int fin_wait2 = 1; // wait for a FIN from the client 
				struct packet received_packet; 
				bzero(&received_packet, sizeof(struct packet)); 

				struct packet fin_ack = {1024, (num_packets + 1) % 30, num_packets + 1, 0, NULL, 4, get_timestamp()};
				fin_ack.flag = 4; 

				struct timeval timewait;
				timewait.tv_sec = 1; 
				timewait.tv_usec = 0; 

				while (fin_wait2)
				{
					n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &clientaddr, &clientlen); 
					if (n < 0)
						error("ERROR in recvfrom"); 
					if (received_packet.flag == 3) // FIN packet
					{
						printf("Receiving packet %d\n", received_packet.seq_num); 
						// received client FIN, server sends an ACK + enters TIME-WAIT
						fin_ack.timestamp = get_timestamp();
						fin_ack.seq_num = received_packet.seq_num; 
						printf("Sending packet %d 5120\n", fin_ack.seq_num); 
						n = sendto(sockfd, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr*)&clientaddr, clientlen); 
						if (n < 0)
							error("ERROR in sendto");  

						// see if client sent over anything
						fd_set readfds; 
						FD_ZERO(&readfds); 
						FD_SET(sockfd, &readfds); 
						n = select(sockfd + 1, &readfds, NULL, NULL, &timewait); 
						if (n == 0)
						{
							printf("\n\n"); 
							fin_wait2 = 0; 
							free(packets); 
						}
					}
					else
						printf("Receiving packet %d\n", received_packet.seq_num); 
				}
			}
		}
	}
}


