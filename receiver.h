#ifndef REC_HEADER
#define REC_HEADER

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define FLAG (0x7E)
#define A (0x03)
#define A_1 (0x01)
#define SET (0x03)
#define UA (0x07)
#define DISC (0x0B)
#define C0 (0x00)
#define C1 (0x40)
#define ESC (0x7D)
#define FLAGESC (0x5E)
#define FLAGFLAGESC (0x5D)
#define RR0 (0x05)
#define RR1 (0x85)
#define REJ0 (0x01)
#define REJ1 (0x81) 
#define CP_END (0x03)



int receiveControlWord(int fd, unsigned char C);

int verifyBCC2(unsigned char*messageReceived, int sizeMessageReceived);

void sendControlWord(int fd, unsigned char C);

int LLREAD(int fd, unsigned char * messageReceived);

void LLCLOSE(int fd);

int LLOPEN(int fd);

int isEndPacket(unsigned char* packetReceived,int sizePacketReceived, unsigned char* startPacket, int sizeStartPacket);

unsigned char * removeControlHeader(unsigned char *packetReceived,int sizePacketReceived);

void createFile(unsigned char* fileData, int fileSize);

int main(int argc, char *argv[]);

#endif //REC_HEADER