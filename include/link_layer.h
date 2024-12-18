// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>



typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA_READ,
    DATA_READ_ESC,
    STOP
} StateMachine;

typedef struct {
    int numFrames;
    int numRetransmissions;
    int numTimeouts;
    double timeTaken; // Time taken in seconds
} CommunicationStats;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

#define FLAG 0x7E
#define ADRESS_SEN 0x03
#define ADRESS_REC 0X01
#define CTRL_SET 0X03
#define CTRL_UA 0X07
#define CTRL_RR0 0XAA
#define CTRL_RR1 0XAB
#define CTRL_REJ0 0X54
#define CTRL_REJ1 0X55
#define CTRL_DISC 0X0B

#define ESC 0x7D

#define NS(n) (n << 7)

#define BUF_SIZE 256

// MISC
#define FALSE 0
#define TRUE 1



int sendSUFrame(unsigned char address, unsigned char ctrl);
void alarmHandler(int signal);

StateMachine llopen_tx_state_machine(unsigned char byte, StateMachine state);
StateMachine llopen_rx_state_machine(unsigned char byte, StateMachine state);
StateMachine llwrite_state_machine(unsigned char byte, StateMachine state, unsigned char* response);
StateMachine llread_state_machine(unsigned char byte, StateMachine state, unsigned char *ctrl_field, unsigned char *packet, int *packet_index);
StateMachine llclose_tx_state_machine(unsigned char byte, StateMachine state);
StateMachine llclose_rx_state_machine(unsigned char byte, StateMachine state, unsigned char* control);

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics, LinkLayer connectionParameters);

void printStatistics(const CommunicationStats *stats);

#endif // _LINK_LAYER_H_
