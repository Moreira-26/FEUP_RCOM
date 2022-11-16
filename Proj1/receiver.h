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


//--------------------Data link layer--------------------------------

//Função com máquina de estados para leitura de controlWord
int receiveControlWord(int fd, unsigned char C);

//Função que verifica o BCC2
int verifyBCC2(unsigned char*messageReceived, int sizeMessageReceived);

//Função que cria uma trama de supervisão e envia
void sendControlWord(int fd, unsigned char C);

//Função de leitura de tramas
int LLREAD(int fd, unsigned char * messageReceived);

//Função de terminação da ligação
void LLCLOSE(int fd);

//Função de estabelecimento da ligação
int LLOPEN(int fd);

//---------------------Application Layer----------------------------

//Função que verifica se um packet é o packet de terminação
int isEndPacket(unsigned char* packetReceived,int sizePacketReceived, unsigned char* startPacket, int sizeStartPacket);

//Função que remove o Header de controlo da application layer
unsigned char * removeControlHeader(unsigned char *packetReceived,int sizePacketReceived);

//Função que cria um ficheiro a partir dos dados recebidos
void createFile(unsigned char* fileData, int fileSize);

int main(int argc, char *argv[]);

#endif //REC_HEADER