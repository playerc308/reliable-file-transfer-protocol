#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
using namespace std;

const int MAX_MAP_SIZE = 4096;   // 8 M = 8388608 B
const int SEND_PACKET_SIZE = 4096;
const int RECV_PACKET_SIZE = 4;
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
  /********************** parse argument *************************/
  int oc;                     // option character -r or -f
  char* conn_info = NULL;     // option string <recv host>:<recv port>
  char* file_name = NULL;     // option string <filename>
  
  opterr = 0;                 // stop getopt() to output error info
  while ((oc = getopt(argc, argv, "r:f:")) != -1) {
    switch (oc) {
      case 'r':
        conn_info = optarg;
        break;
      case 'f':
        file_name = optarg;
        break;
      default:
        cerr << "Illegal Argument!" << endl;
        return -1;
    }
  }
  
  /********************** init socket *************************/
  char host_name[20];
  int port;
  if (sscanf(conn_info, "%[^:]:%d", host_name, &port) != 2) {
    cerr << "Illegal Argument!" << endl;
    return -1;
  }
  //cout << host_name << port << endl;
  hostent* host = gethostbyname(host_name);
  unsigned int addr = *(unsigned int*)host->h_addr_list[0];
  
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
  sin.sin_addr.s_addr = addr;
  sin.sin_port = htons(port);
  socklen_t sin_len = sizeof(sin);
  
  char* sendbuf = new char[SEND_PACKET_SIZE];
  char* recvbuf = new char[RECV_PACKET_SIZE];
  unsigned short serial_no = 0;
  
  struct stat s;
  char* filebuf;
  int file = open(file_name, O_RDONLY);
  fstat(file, &s);
  
  int file_size = (int)s.st_size;
  int map_offset = 0;
  int map_size = (file_size - map_offset) > MAX_MAP_SIZE ? MAX_MAP_SIZE : file_size - map_offset;
  
  while (map_offset < file_size) {
    /* map file into memory */
    filebuf = (char*)mmap(NULL, map_size, PROT_READ, MAP_SHARED, file, map_offset);
    if (filebuf == MAP_FAILED) {
      perror("map file failed!");
      return -1;
    }
    
    int offset = 0;
    int payload_size = (map_size - offset) > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : map_size - offset;
    bool retransmit = false;
    
    while (offset < map_size) {
      if (!retransmit) {
        memset(sendbuf, 0, SEND_PACKET_SIZE);
        *(unsigned short*)(sendbuf + 8) = htons(serial_no);
        *(unsigned short*)(sendbuf + 12) = htons(payload_size);
        
        if (serial_no == 0) {
          memcpy(sendbuf + 16, file_name, strlen(file_name));
        } else {
          memcpy(sendbuf + 16, filebuf + offset, payload_size);
        }
        
        unsigned int hash = BKDRHash(sendbuf + 16, 0);
        *(unsigned int*)sendbuf = htonl(hash);
      }
      
      long count = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (sockaddr*)&sin, sin_len);
      if (count < 0) {
        perror("error sending packet");
        return -1;
      }
      
      count = recvfrom(sockfd, recvbuf, RECV_PACKET_SIZE, 0, (sockaddr*)&sin, &sin_len);
      if (count < 0) {
        if (errno == ETIMEDOUT) {
          retransmit = true;
          continue;
        } else {
          perror("error receiving packet");
          return -1;
        }
      }
      
      /* check serial number */
      if (serial_no != ntohs(*(unsigned short*)recvbuf)) {
        retransmit = true;
        continue;
      } else {
        retransmit = false;
      }
      
      offset += payload_size;
      payload_size = (map_size - offset) ? MAX_PAYLOAD_SIZE : map_size - offset;
      serial_no++;
    }
    
    /* unmap file into memory */
    munmap(filebuf, map_size);
    map_offset += map_size;
    map_size = (file_size - map_offset) > MAX_MAP_SIZE ? MAX_MAP_SIZE : file_size - map_offset;
  }
  
  close(file);
  close(sockfd);
  return 0;
}