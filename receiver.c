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
volatile int STOP = FALSE;
int expectedFrame = 0;

struct termios oldtio;
struct termios newtio;

int receiveControlWord(int fd, unsigned char C){
    int state = 0;
    unsigned char c = 0xFF;
    unsigned char A_check;
    while(state != 5){
        read(fd, &c,1);
        switch(state){
            case 0:
                if(c == FLAG){
                    state = 1;
                }
                break;
            case 1:
                if(c == A || c == A_1){
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
                if(C == DISC || C == UA){
                    A_check = A_1;
                }else{
                    A_check = A;
                }
                if(c == (A_check ^ C))
                    state = 4;
                else
                    state = 0;
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
    return TRUE;
}


int verifyBCC2(unsigned char*messageReceived, int sizeMessageReceived){

    unsigned char BCC2 = messageReceived[0];
    for (int j = 1; j < sizeMessageReceived - 1; j++){
        BCC2 ^= messageReceived[j];
    }


    if(BCC2 == messageReceived[sizeMessageReceived - 1]){
        return TRUE;
    }else{
        return FALSE;
    }
}


void sendControlWord(int fd, unsigned char C){
    unsigned char message[5];
    message[0] = FLAG;
    if(C == DISC){
        message[1] = A_1;
    }else{
        message[1] = A;    
    }
    message[2] = C;
    message[3] = message[1] ^ message[2];
    message[4] = FLAG;
    write(fd,message,5);
}

int LLREAD(int fd, unsigned char * messageReceived){
    int sizeMessageReceived = 0;

    int acceptedFrame =  FALSE;
    int receivedFrame;

    int state = 0;
    unsigned char readChar,saveC;
    int i= 0;
    while(state != 7){
        read(fd,&readChar,1);
        switch(state){
            case 0:
                if(readChar == FLAG){
                    state = 1;
                }else{
                    state = 0;
                }
                break;
            case 1:
                if(readChar == A){
                    state = 2;
                }else if(readChar == FLAG){
                    state = 1;
                }else{
                    state = 0;
                }
                break;
            case 2:
                if(readChar == C0 || readChar == C1){
                    state = 3;
                    saveC = readChar;
                    if(readChar == C0){
                        receivedFrame = 0;
                    }else{
                        receivedFrame = 1;
                    }
                }else if(readChar == FLAG){
                    state = 1;
                }else{
                    state = 0;
                }
                break;
            case 3:
                if(readChar == (A^saveC)){
                    state = 4;
                }else{
                    printf("error in the protocol\n");
                    state = 0;
                }
                break;
            case 4:
                if(readChar == FLAG){
                    state = 6;
                }else if(readChar == ESC){
                    state = 5;
                }else{
                    sizeMessageReceived++;
                    messageReceived = (unsigned char*)realloc(messageReceived, sizeMessageReceived);
                    messageReceived[sizeMessageReceived - 1 ] = readChar;
                }
                break;
            case 5:
                if(readChar == FLAGESC){
                    //destuffing trocar ESC e FLAGESC por FLAG que é igual a adicionar a FLAG à msg
                    sizeMessageReceived++;
                    messageReceived = (unsigned char*)realloc(messageReceived, sizeMessageReceived);
                    messageReceived[sizeMessageReceived - 1 ] = FLAG;
                }else if(readChar == FLAGFLAGESC){
                    //destuffing trocar ESC e FLAGFLAGESC por ESC que é igual a adicionar a ESC à msg
                    sizeMessageReceived++;
                    messageReceived = (unsigned char*)realloc(messageReceived, sizeMessageReceived);
                    messageReceived[sizeMessageReceived - 1 ] = ESC;
                }
                state = 4;
                break;
            case 6:
                if(receivedFrame == expectedFrame){
                    if(verifyBCC2(messageReceived,sizeMessageReceived)){
                        if(receivedFrame == 0){
                            sendControlWord(fd,RR1);
                            state = 7;
                        }else if(receivedFrame == 1){
                            sendControlWord(fd,RR0);
                            state = 7;     
                        }
                        acceptedFrame = TRUE;
                    }else{
                        if(receivedFrame == 0){
                            sendControlWord(fd,REJ0);
                            state = 7;
                        }else{
                            sendControlWord(fd,REJ1);
                            state = 7;     
                        }
                        acceptedFrame = FALSE;  
                        printf("error in the data\n");
                    }
                }else{
                    //Frame recebido repetido
                    printf("duplicate frame\n");
                    if(expectedFrame == 0){
                        sendControlWord(fd,REJ0);
                        state = 7;
                    }else{
                        sendControlWord(fd,REJ1);
                        state = 7;     
                    }
                    acceptedFrame = FALSE;
                }
                
                break;
        }
    }

    //retirar BCC2 da mensagem
    messageReceived = (unsigned char*)realloc(messageReceived,sizeMessageReceived - 1);
    sizeMessageReceived--;

    int sizeReturn;

    if(acceptedFrame){ 
        if(receivedFrame == expectedFrame){
            expectedFrame ^= 1;
            
            sizeReturn = sizeMessageReceived;
        }else{
            sizeReturn = -1;
        }
    }else{
        sizeReturn = -1;
    } 

    return sizeReturn;
}

void LLCLOSE(int fd){
    printf("LLCLOSE STARTED\n");
    receiveControlWord(fd,DISC);
    printf("DISC RECEIVED\n");
    sendControlWord(fd,DISC);
    printf("DISC SENT\n");
    

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    close(fd);
}


void LLOPEN(int fd){

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


    if(receiveControlWord(fd,SET)){
        printf("SET RECEIVED\n");
        sendControlWord(fd,UA);
        printf("UA SENT\n");
    }

}

int isEndPacket(unsigned char* packetReceived,int sizePacketReceived, unsigned char* startPacket, int sizeStartPacket){
    if(sizePacketReceived != sizeStartPacket){
        return FALSE;
    }
    if(packetReceived[0] != CP_END){
        return FALSE;
    }else{
        for(int i = 1; i < sizePacketReceived; i++){
            if(packetReceived[i] != startPacket[i]){
                return FALSE;
            }
        }
    }
    return TRUE; //TODO 
}

unsigned char * removeControlHeader(unsigned char *packetReceived,int sizePacketReceived){
    unsigned char* newPacketWithoutHeader = (unsigned char*)malloc(sizePacketReceived - 4);
    int j = 4;
    for(int i = 0; i < (sizePacketReceived - 4);i++){
        newPacketWithoutHeader[i] = packetReceived[j];
        j++;
    }
    return newPacketWithoutHeader;
}

void createFile(unsigned char* fileData, int fileSize){
    FILE *fp;
    fp = fopen("pinguimReceiver.gif","wb+");
    fwrite((void*)fileData,1,fileSize,fp);
    fclose(fp);
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

    unsigned char* startPacket = (unsigned char*)malloc(0);
    unsigned char* fileBytes ;
    int fileSize;
    int index = 0;
    int sizeStartPacket;

    sizeStartPacket = LLREAD(fd,startPacket);
    if(sizeStartPacket > 0 ){
        fileSize = (startPacket[3] << 24) | (startPacket[4] << 16) | (startPacket[5] << 8) | startPacket[6];
        printf("FILE HAS %i bytes\n",fileSize);
    }
    

    fileBytes = (unsigned char*)malloc(fileSize * sizeof(unsigned char));
    
    unsigned char * packetReceived;
    int sizePacketReceived;
    while(TRUE){
        packetReceived = (unsigned char*)malloc(0);
        sizePacketReceived = LLREAD(fd,packetReceived);
        if(sizePacketReceived > 0){
            if(isEndPacket(packetReceived,sizePacketReceived,startPacket,sizeStartPacket)){
                printf("End Packect received\n");
                break;
            }else{
                packetReceived = removeControlHeader(packetReceived,sizePacketReceived);
                memcpy(fileBytes + index,packetReceived,sizePacketReceived - 4);
                index += (sizePacketReceived - 4);
            }
        }
        free(packetReceived);

    }

    createFile(fileBytes,fileSize);


    LLCLOSE(fd);

    return 0;
}