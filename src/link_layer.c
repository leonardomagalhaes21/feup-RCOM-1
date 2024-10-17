// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

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


StateMachine llopen_tx_state_machine(unsigned char byte, StateMachine state){
    switch (state) {
        case START:
            if (byte == FLAG) {
                state = FLAG_RCV;
                printf("var = 0x%02X\n", FLAG);
            }
            break;
        
        case FLAG_RCV:
            if (byte == ADRESS_REC) {
                state = A_RCV;
                printf("var = 0x%02X\n", ADRESS_REC);
            }
            else if (byte != FLAG) {
                state = START;
            }
            break;
        
        case A_RCV:
            if (byte == CTRL_UA) {
                state = C_RCV;
                printf("var = 0x%02X\n", CTRL_UA);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        
        case C_RCV:
            if (byte == (ADRESS_REC ^ CTRL_UA)) {
                state = BCC_OK;
                printf("var = 0x%02X\n", ADRESS_REC ^ CTRL_UA);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        case BCC_OK:
            if (byte == FLAG) {
                state = STOP;
                printf("var = 0x%02X\n", FLAG);
            }
            else {
                state = START;
            }
            break;
        
        default:
            break;
    }
    return state;
}

StateMachine llopen_rx_state_machine(unsigned char byte, StateMachine state){
    switch (state) {
        case START:
            if (byte == FLAG) {
                state = FLAG_RCV;
                printf("var = 0x%02X\n", FLAG);
            }
            break;
        
        case FLAG_RCV:
            if (byte == ADRESS_SEN) {
                state = A_RCV;
                printf("var = 0x%02X\n", ADRESS_SEN);
            }
            else if (byte != FLAG) {
                state = START;
            }
            break;
        
        case A_RCV:
            if (byte == CTRL_SET) {
                state = C_RCV;
                printf("var = 0x%02X\n", CTRL_SET);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        
        case C_RCV:
            if (byte == (ADRESS_SEN ^ CTRL_SET)) {
                state = BCC_OK;
                printf("var = 0x%02X\n", ADRESS_SEN ^ CTRL_SET);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        case BCC_OK:
            if (byte == FLAG) {
                state = STOP;
                printf("var = 0x%02X\n", FLAG);
            }
            else {
                state = START;
            }
            break;
        
        default:
            break;
    }
    return state;
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

        struct sigaction act = {0};
        act.sa_handler = &alarmHandler;
        if (sigaction(SIGALRM, &act, NULL) == -1)
        {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }
        //(void)signal(SIGALRM, alarmHandler);

        unsigned char byte = 0;
        int nRetrasmissions = connectionParameters.nRetransmissions;
        int timeout = connectionParameters.timeout;
        StateMachine state = START;

        while (state != STOP && nRetrasmissions > 0) {
            printf("Sending SET\n");
            sendFrame(ADRESS_SEN, CTRL_SET);
            alarm(timeout);
            alarmEnabled = TRUE;
            printf("Waiting for response\n");
            while (state != STOP && alarmEnabled == TRUE) {
                if (readByteSerialPort(&byte) > 0){
                    state = llopen_tx_state_machine(byte, state);
                    if (state == STOP) {
                        printf("Received UA\n");
                        printf("Connection established\n");
                        alarmEnabled = FALSE;
                        alarm(0);
                    }
                }
            }
            nRetrasmissions--;
        }
    }

    else if (connectionParameters.role == LlRx) {
        unsigned char byte = 0;
        StateMachine state = START;
        printf("Waiting for SET\n");
        while (state != STOP) {
            if (readByteSerialPort(&byte) > 0){
                state = llopen_rx_state_machine(byte, state);
                if (state == STOP) {
                    printf("Received SET\n");
                }
            }
        }
        printf("Sending UA\n");
        sendFrame(ADRESS_REC, CTRL_UA);
    }

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
