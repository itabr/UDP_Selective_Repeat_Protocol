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
#define DATASIZE 1016

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
  int size = 1016; 
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

  //printf("timestamp = %lld microseconds\n", timestamp_usec); 
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

  /* 
   * main loop: wait for a datagram, then echo it
   */

  clientlen = sizeof(clientaddr);

  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */

    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
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
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    /* 
     * sendto: echo the input back to the client 
     */

    fp = fopen(buf, "rb");

    if (fp == NULL){
     error("file not found.\n\n\n\n\n");
    }

    // get size of the file so we know how much to read 
   struct stat stats; 
   stat(buf, &stats); 
   int file_size = stats.st_size; 

   int num_packets = ceil((double)file_size / DATASIZE);  

   struct packet* packets = malloc(num_packets * sizeof(struct packet)); 

   int packet_num = 0; 
   char data[DATASIZE]; 
   
   while (!feof(fp)) 
   {
      struct packet pack = {1024, packet_num % 30, packet_num, calc_checksum(data), data, 0, 0}; 
      size_t read_length = fread(pack.data, sizeof(char), DATASIZE, fp); 
      pack.cs = calc_checksum(pack.data); 

      if (read_length == 0)
      {
        error("Could not read file.\n\n"); 
      }  

      *(packets + packet_num) = pack; 
      packet_num++; 
   }

   int window_start = 0; // start index for the window 
   int next_packet_num = 0; // packet number of the next packet to be sent 

   //int cycles = 0; // for keeping track of repeated sequence numbers 

   /* for timeout */
   struct timeval timeout;
   timeout.tv_sec = 0;
   timeout.tv_usec = 500000;
   
   while (window_start < num_packets)
   {
    while (next_packet_num - window_start < window_size && next_packet_num < num_packets)
      // have space in the window to send a new packet and haven't sent all the packets in the file yet 
    {
      printf("Sending packet %d\n", packets[next_packet_num].seq_num);

      packets[next_packet_num].timestamp = get_timestamp();  

      n = sendto(sockfd, &packets[next_packet_num], sizeof(packets[next_packet_num]), 0, (struct sockaddr*)&clientaddr, clientlen); 
      if (n < 0)
        error("ERROR in sendto");

      packets[next_packet_num].flag = 1; // sent packet, now waiting for ACK 
      next_packet_num++;  
      printf("window start = %d\n", window_start); 
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
       int k = recvfrom(sockfd, &ack_num, sizeof(ack_num), 0, (struct sockaddr*)&clientaddr, &clientlen); 
       printf("Server received ack num = %d\n", ack_num); 

       if (ack_num < next_packet_num)
       {
	       packets[ack_num].flag = 2; 
	       if (ack_num == window_start)
         {
	         packets[window_start].flag = 2; 
           while (packets[window_start].flag == 2)
           {
              window_start++; // move the window forward to the first unacked packet
	      /*
              if (window_start == 30)
                cycles++; 
	      */
           }
         }
       }
    }
    else if (n == 0)
    {
      //printf("timeout occurred\n"); 
      int i;
      for (i = window_start; i < window_start + window_size; i++)
      {
        if (packets[i].flag == 1)
        {
            struct timeval timer_usec;
            long double timestamp_usec; 
            gettimeofday(&timer_usec, NULL);
            timestamp_usec = ((long double)timer_usec.tv_sec) * 100000011 + (long double) timer_usec.tv_usec; 

            //printf("current = %Lf microseconds\n", timestamp_usec); 
	          //printf("packet timestamp = %Lf microseconds\n", packets[i].timestamp); 
            //printf("time diff for packet %d = %Lf\n", i, timestamp_usec - packets[i].timestamp); 

            if (timestamp_usec - packets[i].timestamp >= 500000)
            {
                printf("timeout for packet %d\n", i);
                packets[i].timestamp = get_timestamp();  
                n = sendto(sockfd, &packets[i], sizeof(packets[i]), 0, (struct sockaddr*)&clientaddr, clientlen); 
                if (n < 0)
                error("ERROR in retransmit"); 
            }
        }
      }
    }
   }
  }
}


