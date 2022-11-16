#ifndef SENDER_HEADER
#define SENDER_HEADER

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

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

//packet Application layer
#define CP_START (0x02)
#define CP_END (0x03)
#define CP_TYPE_FILESIZE (0x00)
#define CP_LENGTH_FILESIZE (0x04)
#define Control_DATA_PACKET (0x01)
#define BUF_SIZE 256
#define PACKET_SIZE 500

#define TIMEOUT 3 //Tempo até Timeout
#define MAX_SENDS 3 //Numero maximo de tentativas de envio

#define BCC1ERROR 0
#define BCC2ERROR 0


//--------------------Data link layer--------------------------------

//Função que cria uma trama de supervisão e envia
void sendControlWord(int fd, unsigned char C);

//Função de alarme
void alarmHandler(int signal);

//Função com máquina de estados para leitura de controlWord
void receiveControlWord(int fd, unsigned char * cReceived);

//Função de envio das tramas
int LLWRITE(int fd, unsigned char* msg, int size);

//Função de terminação da ligação
int LLCLOSE(int fd);

//Função de estabelecimento da ligação
int LLOPEN(int fd);


//---------------------Application Layer----------------------------

//Função que abre um ficheiro, lê o seu conteudo e retira o seu tamanho 
unsigned char* openFile(unsigned char *fileName, int* sizeFile);

//Função que cria um packet de Controlo
unsigned char *createControlPacket(int start,int fileSize,int *sizeControlPacket);

//Função que adiciona o cabeçalho da application layer
unsigned char* addHeaderPacket(unsigned char* packet, int fileSize, int packetSize);

//Função que cria um packet a partir dos dados do ficheiro 
void createPacket(unsigned char* fileData,int*indexFile, int fileSize,int* packetSize, unsigned char* packet);


//Função que introduz erros no BCC1
unsigned char *errorBCC1(unsigned char* packet, int packetSize);

//Função que introduz erros no BCC2
unsigned char *errorBCC2(unsigned char* packet, int packetSize);

int main(int argc, char *argv[]);

#endif //SENDER_HEADER