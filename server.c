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

  while (1) {

    bzero(buf, BUFSIZE);

    struct packet file_name_packet = {0,0,0,0,0,0,0,0};

    n = recvfrom(sockfd, &file_name_packet, BUFSIZE, 0,(struct sockaddr *) &clientaddr, &clientlen);


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

    /* choose a random start # between 0 and 30720 for the sequence # */ 
    int seq_num = rand() % 30720; 
    printf("starting seq num = %d\n", seq_num);  

    //printf("Receiving packet %d")
    char *filename = file_name_packet.data;

    printf("%s\n",filename);
    fp = fopen(filename, "rb");

    if (fp == NULL){
      struct packet not_found = {1024, -1, -1, -1, "File not found", 0, 0}; 
      n = sendto(sockfd, &not_found, sizeof(not_found), 0, (struct sockaddr*)&clientaddr, clientlen); 
     error("file not found.\n\n\n\n\n");
    }

    // get size of the file so we know how much to read 
   struct stat stats; 
   stat(filename, &stats);
   
   int file_size = stats.st_size; 

   int num_packets = ceil((double)file_size / DATASIZE);

   printf("nummmmmmmmmmeeeeeeeedddd %d\n", num_packets);

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

   //int cycles = 0; // for keeping track of repeated sequence numbers 
   
   while (window_start < num_packets)
   {
    while (next_packet_num - window_start < window_size && next_packet_num < num_packets)
      // have space in the window to send a new packet and haven't sent all the packets in the file yet 
    {
      printf("Sending packet %d\n", packets[next_packet_num].packet_num);

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
   // finished transmitting all packets, do the FIN/FIN-ACK procedure 
   struct packet fin = {1024, num_packets % 30, num_packets, 0, NULL, 3, get_timestamp()}; // flag = 3 denotes FIN
   fin.flag = 3; 
   printf("Sending server FIN, packet %d flag %d\n", fin.packet_num, fin.flag); 
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
        printf("Retransmit FIN\n"); 
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
        printf("Received client FIN-ACK, packet %d\n", ack_num);
        if (ack_num == fin.packet_num) // received FIN-ACK, enter fin wait 2 state 
          fin_wait1 = 0;
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
    //printf("reiterate\n"); 
    n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, &clientaddr, &clientlen); 
    if (n < 0)
      error("ERROR in recvfrom"); 
    printf("received packet # %d\n", received_packet.packet_num); 
    if (received_packet.flag == 3) // FIN packet
    {
      printf("Received client FIN\n"); 
      // received client FIN, server sends an ACK + enters TIME-WAIT
      fin_ack.timestamp = get_timestamp();
      printf("Sending server FIN-ACK\n"); 
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
        printf("Connection finished\n"); 
        fin_wait2 = 0; 
        free(packets); 
      }
    }
   }
  }
}


