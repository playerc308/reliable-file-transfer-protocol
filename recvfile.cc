#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
using namespace std;

const int MAX_MAP_SIZE = 4096;   // 8 M = 8388608 B
const int SEND_PACKET_SIZE = 4;
const int RECV_PACKET_SIZE = 4096;
const int MAX_PAYLOAD_SIZE = SEND_PACKET_SIZE - 16;

unsigned int BKDRHash(char *str, unsigned int len) {
  unsigned int seed = 131;
  unsigned int hash = 0;
  
  while (*str) {
    hash = hash * seed + (*str++);
  }
  
  return hash;
}

int main(int argc, char** argv) {

  int port=atoi(argv[1]);
  char file_name[255];
  
  
  int sockfd;
  if ((sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("opening UDP socket!");
    return -1;
  }
  
  int timeout = 1;  // timeout = 1 ms
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setting UDP socket option");
    return -1;
  }
  
  sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  socklen_t sin_len = sizeof(sin);
  
  if (bind(sock, (struct sockaddr *) &sin, sizeof (sin)) < 0)
    {
      perror("binding socket to address");
      abort();
    }

  /* put the server socket in listen mode */
  if (listen (sock, BACKLOG) < 0)
    {
      perror ("listen on socket failed");
      abort();
    }

  char* sendbuf = new char[SEND_PACKET_SIZE];
  char* recvbuf = new char[RECV_PACKET_SIZE];
  unsigned short serial_no = 0;
  
  struct stat s;
  char* filebuf;
  int map_size=0;
  int offset = 0;
  int file ;
  int file_size=0;
  long file_size_rev;
  while (1) {

    if (filebuf == MAP_FAILED) {
      perror("map file failed!");
      return -1;
    }
    long count = recvfrom(sockfd, recvbuf, RECV_PACKET_SIZE, 0, (sockaddr*)&sin, &sin_len);
    if (count < 0) {
      if (errno == ETIMEDOUT) {
        printf("[timeout error]");
        continue;
      } else {
        perror("error receiving packet");
        return -1;
      }
    }
    unsigned int hash_rev=ntohl(*(unsigned int*)recvbuf);
    unsigned short serial_no_rev=ntohs(*(unsigned short*)(recvbuf + 8));
    unsigned short payload_size_rev = ntohs(*(unsigned short*)(recvbuf + 12));
    unsigned int hash = BKDRHash(recvbuf + 16, 0);
    if(hash!=hash_rev||serial_no_rev!=serial_no){//bad packet, ask client retran
    	*(unsigned short*)(recvbuf) = htons(0xffff);
    	long count = sendto(sockfd, sendbuf, SEND_PACKET_SIZE, 0, (sockaddr*)&sin, sin_len);
    	if (count < 0) {
    		perror("error sending packet");
    	    return -1;
    	}
    	continue;
    }
    if(serial_no==0){
    	memcpy(file_name, recvbuf + 16, strlen(file_name));
    	file_size_rev=ntohl(*(unsigned int*)recvbuf+12);
    	long size_high=ntohl(*(unsigned int*)recvbuf+8);
    	file_size_rev=file_size_rev+size_high<<32;
    	file = open(file_name, O_WRONLY, O_CREAT);
    	fstat(file, &s);
    	map_size=payload_size_rev;

    }
    else{
    	memcpy(filebuf+offset, recvbuf + 16, payload_size_rev);
    	offset+=payload_size_rev;
    	file_size+=payload_size_rev;
    	if(offset>=map_size||file_size==file_size_rev){
    		write(file,filebuf,map_size);
    		fstat(file, &s);
    		offset=0;
    	}
    }

    *(unsigned short*)(recvbuf) = htons(serial_no_rev);
    long count = sendto(sockfd, sendbuf, SEND_PACKET_SIZE, 0, (sockaddr*)&sin, sin_len);
    if (count < 0) {
    	perror("error sending packet");
    	return -1;
    }
    serial_no=serial_no_rev+1;
    
  }
  
  printf("[completed]");
  delete sendbuf;
  delete recvbuf;
  close(file);
  close(sockfd);
  return 0;
}
