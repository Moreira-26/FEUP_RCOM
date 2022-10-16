// Read from serial port in non-canonical mode
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
#define RR0 (0x05)
#define RR1 (0x85)
#define REJ0 (0x01)
#define REJ1 (0x81) 


#define BUF_SIZE 256

#define TIMEOUT 3 //Tempo até Timeout
#define MAX_SENDS 3 //Numero maximo de tentativas de envio


//alarm variables
int alarmDisparouTimeout = FALSE;
int alarmCount = 0;

int UAreceived = FALSE;
int currentFrame = 0;

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
    int alarmDisparouTimeout = TRUE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

//state Machine receive UA REJ0 REJ1 RR0 RR1 DISC
void receiveControlWord(int fd, unsigned char * cReceived){
    int state = 0;
    unsigned char c;
    while(state != 5 && !alarmDisparouTimeout){
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
                if(c == UA || c == REJ0 || c == REJ1 || c == RR0 || c == RR1 || c == DISC){
                    state = 3;
                    *cReceived = c;
                }else{
                    if(c == FLAG)
                        state = 1;
                    else
                        state = 0;
                }
                break;
            case 3:
                if(c == (A ^ *cReceived))
                    state = 4;
                else{
                    printf("BCC1 error\n");
                    state = 0;
                }
                break;
            case 4:
                if(c == FLAG){
                    state = 5;
                }
                else
                    state = 0;
                break;                
        }
    }
}

void LLWRITE(int fd, unsigned char* msg, int size){
    unsigned char *frameFinal = (unsigned char *)malloc((size + 6) * sizeof(unsigned char));
    int sizeFrameFinal = size + 6;

    int rejected = FALSE;
    
    frameFinal[0] = FLAG;
    frameFinal[1] = A;

    if(currentFrame == 0){ 
        frameFinal[2] = C0;
    }else{
        frameFinal[2] = C1;
    }

    frameFinal[3] = frameFinal[1] ^ frameFinal[2];

    int k = 4;

    //Stuffing data
    for(int i = 0; i < size; i++){
        if(msg[i] == FLAG){
            sizeFrameFinal++;
            frameFinal = (unsigned char *)realloc(frameFinal,sizeFrameFinal);
            frameFinal[k] = ESC;
            frameFinal[k + 1] = FLAGESC;
            k += 2;
        }else if(msg[i] == ESC){
            sizeFrameFinal++;
            frameFinal = (unsigned char *)realloc(frameFinal,sizeFrameFinal);
            frameFinal[k] = ESC ;
            frameFinal[k + 1] = FLAGFLAGESC;
            k += 2;
        }else{
            frameFinal[k] = msg[i];
            k++;
        }
    }

    //calculo BCC2
    unsigned char BCC2 = msg[0];
    for (int j = 1; j < size; j++){
        BCC2 ^= msg[j];
    }

    //TESTE BCC2 ERRADO
    BCC2 = 0x55;

    //Stuffing BCC2
    if(BCC2 == FLAG){
        sizeFrameFinal++;
        frameFinal[k] = ESC;
        frameFinal[k + 1] = FLAGESC;
        k += 2;
    }else if(BCC2 == ESC){
        sizeFrameFinal++;
        frameFinal[k] = ESC;
        frameFinal[k + 1] = FLAGFLAGESC; 
        k += 2;
    }else{
        frameFinal[k] = BCC2;
    }
    
    frameFinal[k + 1] = FLAG;       
    int alarmEnabled;
    //enviar FrameFinal
    do {
        write(fd,frameFinal,sizeFrameFinal);
        printf("Frame sent %i\n", sizeFrameFinal);
        
        //iniciar Alarm
        alarm(3);
        alarmEnabled = TRUE;
        //ler ControlMessage
        unsigned char controlWordReceived;
        receiveControlWord(fd, &controlWordReceived);

        if((controlWordReceived == RR0 && currentFrame == 1) || (controlWordReceived == RR1 && currentFrame == 0)){
            currentFrame ^= 1;
            alarm(0);
            alarmCount = 0;
            alarmEnabled = FALSE;
            printf("Frame accepeted, %X received\n",controlWordReceived);
        }else if(controlWordReceived == REJ0 || controlWordReceived == REJ1 ){
            printf("Frame rejected, %X received\n",controlWordReceived);
                
        }
        if(alarmCount < MAX_SENDS){
            alarmEnabled = FALSE;
        }
            
    }while(alarmEnabled); //enquanto o alarm para ou é um reject 
    
}

void LLOPEN(int fd){
    struct termios newtio;
    struct termios oldtio;

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

    int alarmEnabled;

    unsigned char c;
    do {
        sendControlWord(fd,SET);
        printf("SET SENT\n");
        alarm(3);
        alarmEnabled = TRUE;

        unsigned char cReceived;
        //tentar receber UA
        while(!UAreceived && alarmEnabled){
            receiveControlWord(fd,&cReceived);
            if(cReceived == UA){
                UAreceived = TRUE;
                printf("UA RECEIVED\n");
            }
        }

        if(alarmCount < MAX_SENDS){
            alarmEnabled = FALSE;
        }

    }while(alarmEnabled);


    unsigned char message2[3];


    unsigned char message[2];
    message[0] = 0x50;
    message[1] = 0x70;
    LLWRITE(fd, message,2); 



    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    
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

    LLOPEN(fd);
    

    return 0;
}
 



