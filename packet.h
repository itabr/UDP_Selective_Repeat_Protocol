#define DATA_SIZE 992
#define CS_SIZE 2

struct packet
{
  // fields other than data take up 28 bytes (4 * 5 + 8) 
  // a packet is 1024 bytes. so th data can be 996 bytes max 
	
  int len; 
  int seq_num;
  int packet_num;
  int cs;
  char data[DATA_SIZE];
  int flag; // 0 = haven't sent, 1 = sent + waiting for ACK, 2 = received ACK, 3 = FIN
  long double timestamp;
  int size;
  
};
