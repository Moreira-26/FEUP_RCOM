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

volatile int STOP = FALSE;

int receiveControlWord(int fd, unsigned char C){
    int state = 0;
    unsigned char c;
    while(state != 5){
        read(fd, &c,1);
        switch(state){
            case 0:
                if(c == FLAG){
                    state = 1;
                }
                break;
            case 1:
                if(c == A){
                    state = 2;
                }else{
                    if (c == FLAG){
                        state = 1;
                    }else{
                        state = 0;
                    }
                }
                break;
            case 2:
                if(c == C){
                    state = 3;
                }else{
                    if(c == FLAG)
                        state = 1;
                    else
                        state = 0;
                }
                break;
            case 3:
                if(c == (A ^ C))
                    state = 4;
                else
                    state = 0;
                break;
            case 4:
                if(c == FLAG){
                    state = 5;
                    printf("CONTROL WORD SENT\n");
                }
                else
                    state = 0;
                break;                
        }
    }
    return TRUE;
}

void sendControlWord(int fd, unsigned char C){
    unsigned char message[5];
    message[0] = FLAG;
    message[1] = A;
    message[2] = C;
    message[3] = message[1] ^ message[2];
    message[4] = FLAG;
    write(fd,message,5);
}


int main(int argc, char *argv[])
{
   // Program usage: Uses either COM1 or COM2
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

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    if(receiveControlWord(fd,SET)){
        printf("SET RECEIVED\n");
        sendControlWord(fd,UA);
        printf("UA SENT\n");
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}


