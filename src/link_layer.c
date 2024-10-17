// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;


int sendFrame(unsigned char address, unsigned char ctrl) {
    unsigned char buf[5] = {0};
    buf[0] = FLAG;
    buf[1] = address;
    buf[2] = ctrl;
    buf[3] = address ^ ctrl;
    buf[4] = FLAG;

    int bytes = writeBytesSerialPort(buf, 5);
    return bytes;
}
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    if (connectionParameters.role == LlTx) {

        /*struct sigaction act = {0};
        act.sa_handler = &alarmHandler;
        if (sigaction(SIGALRM, &act, NULL) == -1)
        {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }*/
        (void)signal(SIGALRM, alarmHandler);

        int bytes = 0;
        while (alarmCount < connectionParameters.nRetransmissions) {
            if (alarmEnabled == FALSE) {
                bytes = sendFrame(ADRESS_SEN, CTRL_SET);
                printf("%d bytes written\n", bytes);

                alarm(connectionParameters.timeout); // Set alarm to be triggered in 4s
                alarmEnabled = TRUE;
            }

            unsigned char buf_set[BUF_SIZE] = {0};
            int bytes_read = 0;
            //erro aqui ver depois fica parado a dar read
            while (bytes_read < 5){
                unsigned char byte_content = 0;
                int byte = readByteSerialPort(&byte_content);
                if (byte < 0){
                    printf("fatal error 1 \n");
                    return -1;
                }
                else if (byte == 0){
                    printf("byte 0 \n");
                    continue;
                }
                else {
                    buf_set[bytes_read] = byte_content;
                    bytes_read++;
                }
            }
            

            for(int i = 0; i < bytes_read; i++){
                printf("var = 0x%02X\n", buf_set[i]);
            }


            if (bytes_read == 5) {
                //buf_set[bytes_read] = '\0';
                if (buf_set[0] == FLAG && buf_set[1] == ADRESS_REC && buf_set[2] == CTRL_UA && buf_set[3] == (buf_set[1] ^ buf_set[2]) && buf_set[4] == FLAG){
                    printf("Received correctly\n");

                    alarm(0);
                    alarmEnabled = FALSE;
                    break;
                }
            }
        }

    }

    else if (connectionParameters.role == LlRx) {
           // Loop for input
        unsigned char buf[BUF_SIZE] = {0};

        while (STOP == FALSE) {   

            int bytes_read = 0;
            while (bytes_read < 5){
                unsigned char byte_content = 0;
                int byte = readByteSerialPort(&byte_content);
                if (byte < 0){
                    printf("fatal error 2\n");
                    return -1;
                }
                else if (byte == 0){
                    continue;
                }
                else {
                    buf[bytes_read] = byte_content;
                    bytes_read++;
                }
            }
            buf[bytes_read] = '\0'; // Set end of string to '\0'
            
            if(buf[0] == FLAG && buf[1] == ADRESS_SEN && buf[2] == CTRL_SET && buf[3] == ( buf[1] ^ buf[2] ) && buf[4] == FLAG) {
                printf("Message received correctly from sender\n");
                
                for(int i=0; i<5; i++) {
                    printf("var = 0x%02X\n", buf[i]);
                }

                sleep(1);
                int bytes = sendFrame(ADRESS_REC, CTRL_UA);
                printf("%d bytes written\n", bytes);
                
                STOP = TRUE;
            }
            
        }
    }

    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
