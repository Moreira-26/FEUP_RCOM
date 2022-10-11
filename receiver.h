#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

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

void LLOPEN(int fd);

void LLREAD();

void sendControlWord(int fd, unsigned char C);

int receiveControlWord(int fd, unsigned char C);

#define BUF_SIZE 256
