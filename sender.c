// Read from serial port in non-canonical mode
#include "sender.h"

//alarm variables
int alarmEnabled = FALSE;
int alarmCount = 0;

int UAreceived = FALSE;
int trama = 0;

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
}

void LLWRITE(int fd, unsigned char* msg, int size){
    unsigned char *frameFinal = (unsigned char *)malloc((size + 6) * sizeof(unsigned char));
    int sizeFrameFinal = size + 6;
    
    frameFinal[0] = FLAG;
    frameFinal[1] = A;

    if(trama == 0){
        frameFinal[2] = C0;
    }else{
        frameFinal[2] = C1;
    }

    frameFinal[3] = frameFinal[1] ^ frameFinal[2];

    int k = 4;

    for(int i = 0; i < size; i++){
        if(msg[i] == FLAG){
            sizeFrameFinal++;
            frameFinal = (unsigned char *)realloc(frameFinal,sizeFrameFinal);
            frameFinal[k] = ESC;
            frameFinal[k + 1] = flagESC;
            k += 2;
        }else if(msg[i] == ESC){
            sizeFrameFinal++;
            frameFinal = (unsigned char *)realloc(frameFinal,sizeFrameFinal);
            frameFinal[k] = ESC ;
            frameFinal[k + 1] = FlagFlagESC;
            k += 2;
        }else{
            frameFinal[k] = msg[i];
            k++;
        }
    }

    unsigned char BCC2 = msg[0];
    for (int j = 1; j < size; j++){
        BCC2 ^= msg[j];
    }

    frameFinal[k] = BCC2;
    frameFinal[k + 1] = FLAG;   

    //enviar FrameFinal
    do {
        write(fd,frameFinal,sizeFrameFinal);
        
        //iniciar Alarm
        //ler ControlMessage 
        //se for um RR0 e a tramaAtual = 1 ou se for um RR1 e a tramaAtual = 0
            //trocar a tramaAtual
            //parar alarm
        //senao verificar se recebemos um REJ e para alarm
            
    }while();//enquanto o alarm para ou é um reject 
    



}
