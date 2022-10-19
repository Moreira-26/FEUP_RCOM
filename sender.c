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

#define TIMEOUT 3 //Tempo até Timeout
#define MAX_SENDS 3 //Numero maximo de tentativas de envio

struct termios newtio;
struct termios oldtio;

//alarm variables
int alarmEnabled = FALSE;
int alarmCount = 0;

int UAreceived = FALSE;
int currentFrame = 0;

volatile int STOP = FALSE;

void sendControlWord(int fd, unsigned char C){
    unsigned char message[5];
    message[0] = FLAG;
    if(C == DISC || C == UA){
        message[1] = A_1;
    }else{
        message[1] = A;    
    }
    message[2] = C;
    message[3] = message[1] ^ message[2];
    message[4] = FLAG;
    write(fd,message,5);
}


void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("AlarmHandler #%d\n", alarmCount);
}

//state Machine receive UA REJ0 REJ1 RR0 RR1 DISC
void receiveControlWord(int fd, unsigned char * cReceived){
    int state = 0;
    unsigned char c;
    unsigned char A_check;
    while(state != 5 && alarmEnabled){
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
                
                if(*cReceived == DISC){
                    A_check = A_1;
                }else{
                    A_check = A;
                }

                if(c == (A_check ^ *cReceived))
                    state = 4;
                else{
                    printf("BCC1 error\n");
                    *cReceived = 0xFF; //arbitary value 
                    state = 0;
                }
                break;
            case 4:
                if(c == FLAG){
                    state = 5;
                    alarm(0);
                }
                else
                    state = 0;
                break;                
        }
    }
}

int LLWRITE(int fd, unsigned char* msg, int size){
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
    //enviar FrameFinal
    int stop = FALSE;
    do {
        write(fd,frameFinal,sizeFrameFinal);
        printf("Frame sent nº %i size= %i\n", currentFrame, sizeFrameFinal);
        
        //iniciar Alarm
        alarm(TIMEOUT);
        alarmEnabled = TRUE;
        //ler ControlMessage
        unsigned char controlWordReceived;
        receiveControlWord(fd, &controlWordReceived);

        if((controlWordReceived == RR0 && currentFrame == 1) || (controlWordReceived == RR1 && currentFrame == 0)){
            currentFrame ^= 1;
            alarmCount = 0;
            alarmEnabled = FALSE;
            stop = TRUE; 
            printf("Frame accepeted, %X received\n",controlWordReceived);
            return sizeFrameFinal;
        }else if(controlWordReceived == REJ0 || controlWordReceived == REJ1 ){
            printf("Frame rejected, %X received\n",controlWordReceived);
            alarmCount++;   
            printf("Alarm #%d\n", alarmCount);
            alarm(0);
        }
        controlWordReceived = 0xFF;
            
    }while(!stop && alarmCount < MAX_SENDS); //Para quando numero de tentativas maximo ou recebeu um RR correto 
    
    return -1;
}


int LLCLOSE(int fd){
    printf("LLCLOSE STARTED\n");
    sendControlWord(fd, DISC);//A do Disc deve estar errado
    printf("DISC SENT\n");
    unsigned char charReceived;
    receiveControlWord(fd,&charReceived);
    if(charReceived == DISC){
        printf("DISC RECEIVED\n");
    }
    sendControlWord(fd,UA);
    printf("UA SENT\n");

     if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}

int LLOPEN(int fd){

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
        printf("SET SENT\n");
        alarm(3);
        alarmEnabled = TRUE;

        unsigned char cReceived;
        receiveControlWord(fd,&cReceived);
        if(cReceived == UA){
            printf("UA received\n");
            UAreceived = TRUE;
            alarm(0);
        }
    }while(!UAreceived && alarmCount < MAX_SENDS);

    if(alarmCount > MAX_SENDS){
        return FALSE;
    }else{
        return TRUE;
    }
}

unsigned char* openFile(unsigned char *fileName, int* sizeFile){
    FILE * f;
    unsigned char* fileBytes;

    f = fopen((char*)fileName,"rb");
    if(f == NULL){
        perror("Error opening file\n");
        exit(-1);
    }

    fseek(f,0,SEEK_END);

    (*sizeFile) = ftell(f);

    fseek(f,0,SEEK_SET);    

    fileBytes = (unsigned char*)malloc(*sizeFile * sizeof(unsigned char));
    fread(fileBytes,sizeof(unsigned char),*sizeFile,f);

    return fileBytes;
}

//cria packet com tamanho do ficheiro
unsigned char *createControlPacket(int start,int fileSize,int *sizeControlPacket){
    unsigned char* controlPacket = (unsigned char*)malloc(sizeof(unsigned char) * 7);

    if(start){
        controlPacket[0] = CP_START;
    }else{
        controlPacket[0] = CP_END;
    }
    

    controlPacket[1] = CP_TYPE_FILESIZE; //0
    controlPacket[2] = CP_LENGTH_FILESIZE; //4
    controlPacket[3] = (fileSize >> 24) & 0xFF; 
    controlPacket[4] = (fileSize >> 16) & 0xFF; 
    controlPacket[5] = (fileSize >> 8) & 0xFF; 
    controlPacket[6] = fileSize & 0xFF;

    *sizeControlPacket = (int)(sizeof(unsigned char) * 7);

    return controlPacket;
}


int main(int argc, char *argv[])
{
    const char *serialPortName = argv[1];

    if (argc < 3)
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


    int fileSize;

    unsigned char* fileBytes = openFile((unsigned char*)argv[2], &fileSize);

    if(!LLOPEN(fd)){
        printf("CONNECTION ESTABLISHMENT FAILED\n");
        return(-1);
    }

    unsigned char*fileName = (unsigned char*)malloc(strlen(argv[2]));
    fileName = (unsigned char*)argv[2];

    int sizeControlPacket;
    
    unsigned char * startPacket = createControlPacket(TRUE, fileSize,&sizeControlPacket);
    LLWRITE(fd,startPacket,sizeControlPacket);
    printf("START PACKET SENT\n");

    int indice = 0;
    int numPackets = 0;
    int numPackage = 0;

    int packetSize = 100;

    while(indice < fileSize){
        

        //dividir o fileBytes em tramas 

        //Criar packet com header
        unsigned char* packet;

        if(indice + packetSize > fileSize){
            packetSize = fileSize - indice;
        }
        packet = (unsigned char*)malloc(packetSize);
        
        for(int i = 0; i < packetSize;i++){
            packet[i] = fileBytes[indice];
            indice++;
        }

        unsigned char* packetToSend;

        packetToSend = (unsigned char*)malloc(packetSize + 4);
        packetToSend[0] = Control_DATA_PACKET;
        packetToSend[1] = numPackets % 255;
        packetToSend[2] = fileSize / 256;
        packetToSend[3] = fileSize % 256;
        memcpy(packetToSend + 4,packet,packetSize);
        packetSize += 4;
        numPackets++;
        numPackage++;

        LLWRITE(fd,packetToSend,packetSize);

    
    }

    unsigned char * endPacket = createControlPacket(FALSE, fileSize,&sizeControlPacket);
    LLWRITE(fd,endPacket,sizeControlPacket);
    printf("END PACKET SENT\n");


    LLCLOSE(fd);

    return 0;
}
 



