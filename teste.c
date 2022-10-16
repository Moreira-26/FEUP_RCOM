void receiveControlWord(int fd, unsigned char * cReceived){
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
                else
                    state = 0;
                break;
            case 4:
                if(c == FLAG){
                    state = 5;
                    printf("Control Word Received");
                }
                else
                    state = 0;
                break;                
        }
    }
}