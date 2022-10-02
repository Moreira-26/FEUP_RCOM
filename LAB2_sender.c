// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

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

#define BUF_SIZE 256

#define TIMEOUT 3 //Tempo até Timeout
#define MAX_SENDS 3 //Numero maximo de tentativas de envio

//alarm variables
int alarmEnabled = FALSE;
int alarmCount = 0;

int UAreceived = FALSE;

volatile int STOP = FALSE;

void sendControlWord(int fd, unsigned char C){
    unsigned char message[5];
    message[0] = FLAG;
    message[1] = A;
    message[2] = C;
    message[3] = message[1] ^ message[2];
    message[4] = FLAG;
    write(fd,message,5);

}

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

//receiveUA
void receiveControlWord(int* state, unsigned char *c){
    
    switch(*state){
        case 0:
            if(*c == FLAG){
                *state = 1;
            }
            break;
        case 1:
            if(*c == A){
                *state = 2;
            }else{
                if (*c == FLAG){
                    *state = 1;
                }else{
                    *state = 0;
                }
            }
            break;
        case 2:
            if(*c == UA){
                *state = 3;
            }else{
                if(*c == FLAG)
                    *state = 1;
                else
                    *state = 0;
            }
            break;
        case 3:
            if(*c == (A ^ UA))
                *state = 4;
            else
                *state = 0;
            break;
        case 4:
            if(*c == FLAG){
                printf("UA Received");
                UAreceived = TRUE;
                alarm(0);
            }
            else
                state = 0;
            break;                
    }
}

int main(int argc, char *argv[])
{
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    //Alarm setup
    (void)signal(SIGALRM, alarmHandler);

    unsigned char c;
    do {
        sendControlWord(fd,SET);
        printf("SET SENDED\n");
        alarm(3);
        alarmEnabled = TRUE;
        int state = 0;


        //tentar receber UA
        while(!UAreceived && alarmEnabled){
            read(fd, &c,1);
            receiveControlWord( &state, &c);
        }

    }while(!alarmEnabled && alarmCount < MAX_SENDS);
    

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

