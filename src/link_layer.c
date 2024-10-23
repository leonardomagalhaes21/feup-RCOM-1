// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int alarmEnabled = FALSE;
int alarmCount = 0;

int information_frame_number_tx = 0;
int information_frame_number_rx = 1;

int nRetransmissions = 0;
int timeout = 0;


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

    int received = FALSE;


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
        nRetransmissions = connectionParameters.nRetransmissions;
        int nRetrasmissions_aux = nRetransmissions;
        timeout = connectionParameters.timeout;
        StateMachine state = START;

        while (state != STOP && nRetrasmissions_aux >= 0) {
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
                        received = TRUE;
                        alarmEnabled = FALSE;
                        alarm(0);
                    }
                }
            }
            nRetrasmissions_aux--;
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
                    received = TRUE;
                }
            }
        }
        printf("Sending UA\n");
        sendFrame(ADRESS_REC, CTRL_UA);
    }

    if (received == TRUE)
        return 1;
    else
        return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int information_frame_size = bufSize + 6;
    unsigned char *information_frame = (unsigned char *) malloc(information_frame_size);
    information_frame[0] = FLAG;
    information_frame[1] = ADRESS_SEN;
    information_frame[2] = NS(information_frame_number_tx);
    information_frame[3] = information_frame[1] ^ information_frame[2];
    memcpy(information_frame + 4, buf, bufSize);
    unsigned char bcc2 = 0;
    for (int i = 0; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }

    int frame_index = 4;
    // data byte stuffing
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG) {
            information_frame_size++;
            information_frame = (unsigned char *) realloc(information_frame, information_frame_size);
            information_frame[frame_index] = ESC;
            information_frame[frame_index + 1] = 0x5E; // escape for FLAG
            frame_index += 2;
        }
        else if (buf[i] == ESC) {
            information_frame_size++;
            information_frame = (unsigned char *) realloc(information_frame, information_frame_size);
            information_frame[frame_index] = ESC;
            information_frame[frame_index + 1] = 0x5D; // escape for ESC
            frame_index += 2;
        }
        else {
            information_frame[frame_index] = buf[i];
            frame_index++;
        }
    }

    // bcc2 byte stuffing
    if (bcc2 == FLAG) {
        information_frame_size++;
        information_frame = (unsigned char *) realloc(information_frame, information_frame_size);
        information_frame[frame_index] = ESC;
        information_frame[frame_index + 1] = 0x5E; // escape for FLAG
        frame_index += 2;
    }
    else if (bcc2 == ESC) {
        information_frame_size++;
        information_frame = (unsigned char *) realloc(information_frame, information_frame_size);
        information_frame[frame_index] = ESC;
        information_frame[frame_index + 1] = 0x5D; // escape for ESC
        frame_index += 2;
    }
    else {
        information_frame[frame_index] = bcc2;
        frame_index++;
    }

    information_frame[frame_index] = FLAG;

    StateMachine state = START;
    int nRetrasmissions_aux = nRetransmissions;
    unsigned char byte = 0;
    int accepted = FALSE;

    while (state != STOP && nRetrasmissions_aux >= 0) {
        printf("Sending Info\n");
        int bytes = writeBytesSerialPort(information_frame, information_frame_size);
        if (bytes < 0) {
            return -1;
        }
        
        alarm(timeout);
        alarmEnabled = TRUE;
        printf("Waiting for response\n");
        unsigned char response = 0;
        while (state != STOP && alarmEnabled == TRUE) {
            if (readByteSerialPort(&byte) > 0){
                state = llwrite_state_machine(byte, state, &response);
                if (state == STOP) {
                    if (response == CTRL_RR0 || response == CTRL_RR1) {
                        information_frame_number_tx = (information_frame_number_tx + 1) % 2;
                        printf("Received RR\n");
                        accepted = TRUE;
                        break;
                    }
                    else if (response == CTRL_REJ0 || response == CTRL_REJ1) {
                        printf("Received REJ\n");
                        nRetrasmissions_aux = nRetransmissions; // reset retransmissions
                    }
                    alarmEnabled = FALSE;
                    alarm(0);
                }
            }
        }
        nRetrasmissions_aux--;
    }
    free(information_frame);

    if (accepted)
        return information_frame_size;
    else return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte = 0;
    StateMachine state = START;
    unsigned char ctrl_field = 0;
    int packet_index = 0;

    while (state != STOP) {
        if (readByteSerialPort(&byte) > 0){
            state = llread_state_machine(byte, state, &ctrl_field, packet, &packet_index);
            if (state == STOP) 
                return packet_index;
            
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics, LinkLayer connectionParameters)
{

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
        int nRetrasmissions_aux = nRetransmissions;
        StateMachine state = START;

        while (state != STOP && nRetrasmissions_aux >= 0) {
            printf("Sending DISC\n");
            sendFrame(ADRESS_SEN, CTRL_DISC);
            alarm(timeout);
            alarmEnabled = TRUE;
            printf("Waiting for response\n");
            while (state != STOP && alarmEnabled == TRUE) {
                if (readByteSerialPort(&byte) > 0){
                    state = llclose_tx_state_machine(byte, state);
                    if (state == STOP) {
                        printf("Received DISC\n");
                        printf("Sending UA\n");
                        sendFrame(ADRESS_SEN, CTRL_UA);
                        alarmEnabled = FALSE;
                        alarm(0);
                        printf("Connection closed\n");
                    }
                }
            }
            nRetrasmissions_aux--;
        }
    }
    else if (connectionParameters.role == LlRx) {
        unsigned char byte = 0;
        StateMachine state = START;
        unsigned char control = 0;
        printf("Waiting for DISC\n");
        while (state != STOP) {
            if (readByteSerialPort(&byte) > 0){
                state = llclose_rx_state_machine(byte, state, &control);
                if (state == STOP) {
                    printf("Received DISC\n");
                }
            }
        }
        printf("Sending DISC\n");
        sendFrame(ADRESS_REC, CTRL_DISC);
        printf("Waiting for UA\n");
        state = START;
        while (state != STOP) {
            if (readByteSerialPort(&byte) > 0) {
                state = llclose_rx_state_machine(byte, state, &control);
                if (state == STOP) {
                    printf("Received UA\n");
                    printf("Connection closed\n");
                }
            }
        }
    }
    

    int clstat = closeSerialPort();
    return clstat;
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

StateMachine llwrite_state_machine(unsigned char byte, StateMachine state, unsigned char* response){
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
            if (byte == CTRL_RR0 || byte == CTRL_RR1 || byte == CTRL_REJ0 || byte == CTRL_REJ1) { 
                state = C_RCV;
                *response = byte;
                printf("var = 0x%02X\n", byte);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        
        case C_RCV:
            if (byte == (ADRESS_SEN ^ *response)) {
                state = BCC_OK;
                printf("var = 0x%02X\n", ADRESS_SEN ^ *response);
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

StateMachine llread_state_machine(unsigned char byte, StateMachine state, unsigned char *ctrl_field, unsigned char *packet, int *packet_index){
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
            if (byte == NS(0) || byte == NS(1)) { 
                state = C_RCV;
                *ctrl_field = byte;
                printf("var = 0x%02X\n", byte);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        
        case C_RCV:
            if (byte == (ADRESS_SEN ^ *ctrl_field)) {
                state = DATA_READ;
                printf("var = 0x%02X\n", ADRESS_SEN ^ *ctrl_field);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;

        case DATA_READ:
            if (byte == FLAG) {
                unsigned char bcc2 = packet[*packet_index - 1];
                unsigned char bcc2_calc = 0;
                (*packet_index)--;
                packet[*packet_index] = '\0'; //close the packet
                printf("var = 0x%02X\n", FLAG);
                for (int i = 0; i < *packet_index; i++) {
                    bcc2_calc ^= packet[i];
                }
                state = STOP;
                if (bcc2 == bcc2_calc) { // BCC2 is correct
                    if (*ctrl_field != NS(information_frame_number_rx)) {
                        sendFrame(ADRESS_SEN, *ctrl_field == NS(0) ? CTRL_RR1 : CTRL_RR0);
                        information_frame_number_rx = (information_frame_number_rx + 1) % 2;
                        printf("Sent frame, received correctly\n");
                    }
                    else {
                        sendFrame(ADRESS_SEN, *ctrl_field == NS(0) ? CTRL_RR0 : CTRL_RR1);
                        printf("Sent frame, already received, bcc2 correct\n");
                        *packet_index = 0; // reset packet index
                    }
                }
                else { // BCC2 is incorrect
                    if (*ctrl_field != NS(information_frame_number_rx)) {
                        sendFrame(ADRESS_SEN, *ctrl_field == NS(0) ? CTRL_REJ0 : CTRL_REJ1);
                        printf("Sent frame, received incorrectly\n");
                        *packet_index = -1; // packet index is -1 to indicate that the packet was not received correctly
                    }
                    else {
                        sendFrame(ADRESS_SEN, *ctrl_field == NS(0) ? CTRL_RR0 : CTRL_RR1);
                        printf("Sent frame, already received, bcc2 incorrect\n");
                        *packet_index = 0; // reset packet index
                    }
                    
                }

            }
            else if (byte == ESC) {
                state = DATA_READ_ESC;
            }
            else {
                packet[*packet_index] = byte;
                printf("var = 0x%02X\n", byte);
                (*packet_index)++;
            }
            break;
        
        case DATA_READ_ESC:
            if (byte == 0x5E) {
                packet[*packet_index] = FLAG;
                printf("var = 0x%02X\n", FLAG);
                (*packet_index)++;
            }
            else if (byte == 0x5D) {
                packet[*packet_index] = ESC;
                printf("var = 0x%02X\n", ESC);
                (*packet_index)++;
            }
            else {
                packet[*packet_index] = ESC;
                packet[*packet_index + 1] = byte;
                printf("var = 0x%02X\n", ESC);
                printf("var = 0x%02X\n", byte);
                (*packet_index) += 2;
            }
            state = DATA_READ;
            break;

        default:
            break;
    }
    return state;
}

StateMachine llclose_tx_state_machine(unsigned char byte, StateMachine state){
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
            if (byte == CTRL_DISC) {
                state = C_RCV;
                printf("var = 0x%02X\n", CTRL_DISC);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        
        case C_RCV:
            if (byte == (ADRESS_REC ^ CTRL_DISC)) {
                state = BCC_OK;
                printf("var = 0x%02X\n", ADRESS_REC ^ CTRL_DISC);
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

StateMachine llclose_rx_state_machine(unsigned char byte, StateMachine state, unsigned char* control){
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
            if (byte == CTRL_DISC || byte == CTRL_UA) {
                state = C_RCV;
                *control = byte;
                printf("var = 0x%02X\n", byte);
            }
            else if (byte == FLAG) {
                state = FLAG_RCV;
            }
            else {
                state = START;
            }
            break;
        
        case C_RCV:
            if (byte == (ADRESS_SEN ^ *control)) {
                state = BCC_OK;
                printf("var = 0x%02X\n", ADRESS_SEN ^ *control);
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

