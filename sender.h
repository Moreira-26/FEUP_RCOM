#ifndef _RCOM_SENDER_H_
#define _RCOM_SENDER_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define FLAG (0x7E)
#define A (0x03)
#define SET (0x03)
#define UA (0x07)
#define BCC (A^UA)
#define DISC (0x0B)
#define C0 (0x00)
#define C1 (0x40)
#define ESC (0x7D)
#define FLAGESC (0x5E)
#define FLAGFLAGESC (0x5D)

#define BUF_SIZE 256

#define TIMEOUT 3 //Tempo até Timeout
#define MAX_SENDS 3 //Numero maximo de tentativas de envio

void LLOPEN(int fd);

void LLWRITE(int fd, unsigned char* msg, int size); //STUFFING E ENVIA 

void receiveControlWord(int* state, unsigned char *c);//state machineUa

void sendControlWord(int fd, unsigned char C); //send control word com C

void alarmHandler(int signal); //

#endif