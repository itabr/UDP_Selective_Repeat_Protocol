#define DATA_SIZE 128
#define CS_SIZE 16

struct packet
{
  int len;
  int seq_num;
  char data[DATA_SIZE];
  char cs[CS_SIZE];
};