#include "receiver.h"

volatile int STOP = FALSE;
int expectedFrame = 0;

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

void LLOPEN(int fd){

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

    if(receiveControlWord(fd,SET)){
        printf("SET RECEIVED\n");
        sendControlWord(fd,UA);
        printf("UA SENT\n");
    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd); 
}

void LLREAD(int fd){
    unsigned char *messageReceived = (unsigned char*)malloc(0);
    int sizeMessageReceived = 0;

    int acceptedFrame =  FALSE;
    int receivedFrame;

    int state = 0;
    unsigned char readChar,saveC;
    
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
                }else{
                    //REJ pedir de novo? Char inválido a seguir ao ESC
                    /*
                    if(frame == 0){
                        sendControlWord(fd,REJ1);
                        state = 7;
                    }else{
                        sendControlWord(fd,REJ0);
                        state = 7;     
                    }
                    */
                }
                state = 4;
                break;
            case 6:
                if(verifyBCC2(messageReceived)){
                    if(frame == 0){
                        sendControlWord(fd,RR1);
                        state = 7;
                    }else{
                        sendControlWord(fd,RR0);
                        state = 7;     
                    }
                    acceptedFrame = TRUE;
                }else{
                    if(frame == 0){
                        sendControlWord(fd,REJ1);
                        state = 7;
                    }else{
                        sendControlWord(fd,REJ0);
                        state = 7;     
                    }
                    acceptedFrame = FALSE;
                }
                break;
        }
    }

    //retirar BCC2 da mensagem
    messageReceived = (unsigned char*)realloc(messageReceived,sizeof(messageReceived) - 1);
    
    if(acceptedFrame){
        if(frameReceived == expectedFrame){
            expectedFrame ^= 1;
        }else{
            //nao passar mensagem para applicationLayer
        }
    }else{
        //nao passar mensagem para applicationLayer
    } 

    return 0;

}

int verifyBCC2(unsigned char*messageReceived){

    unsigned char BCC2 = msg[0];
    for (int j = 0; j < (int)sizeof(messageReceived) - 2; j++){
        BCC2 ^= msg[j];
    }

    if(BCC2 == messageReceived[(int)sizeof(messageReceived) - 1]){
        return TRUE;
    }else{
        return FALSE;
    }
}

