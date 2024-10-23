// Application layer protocol implementation

#include "application_layer.h"
#include <string.h>

long getFileSize(FILE *file) {
    if (file == NULL)
        return -1;

    long currentPos = ftell(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, currentPos, SEEK_SET);

    return size;
}

unsigned char* getFileData(FILE *file, long fileSize) {
    if (file == NULL) 
        return NULL;

    unsigned char *buffer = (unsigned char*)malloc(fileSize);
    if (buffer == NULL)
        return NULL;

    size_t bytesRead = fread(buffer, sizeof(unsigned char), fileSize, file);
    if (bytesRead != fileSize) {
        perror("Error reading file\n");
        free(buffer);
        return NULL;
    }
    return buffer;
}

unsigned char* buildDataPacket(unsigned char sequenceNum, int dataSize, unsigned char *data) {
    int packetSize = dataSize + 4;
    unsigned char *packet = (unsigned char*)malloc(packetSize);
    if (packet == NULL)
        return NULL;

    packet[0] = 2;
    packet[1] = sequenceNum;
    packet[2] = (dataSize >> 8) & 0xFF; // msb
    packet[3] = dataSize & 0xFF; // lsb

    memcpy(packet + 4, data, dataSize);

    return packet;
}

unsigned char* buildControlPacket(const char *filename, long fileSize, unsigned char controlField) {
    int V1 = 0;
    int V2 = strlen(filename);
    int idx = 0;

    int packetSize = V1 + V2 + 5;
    unsigned char *packet = (unsigned char*)malloc(packetSize);
    if (packet == NULL)
        return NULL;
    
    packet[idx] = controlField;
    idx++;
    // TODO

         
}

 
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayerRole roles;

    if (!strcmp("tx", role)){
        roles = LlTx;
    }
    else if (!strcmp("rx", role)){
        roles = LlRx;
    }
    else {
        return;
    }

    LinkLayer connect_par = {
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .role = roles,
        .timeout = timeout
    };

    strcpy(connect_par.serialPort,serialPort);

    int open = llopen(connect_par);
    if (open < 0) {
        return;
    }


    // testar
    if (roles == LlTx) {
        const unsigned char buffer[] = {0x01, 0x02, 0x03, 0x04, 0x7E, 0x05};
        int bufferSize = sizeof(buffer) / sizeof(buffer[0]);
        
        int result = llwrite(buffer, bufferSize);
        if (result < 0) {
            return;
        }
        int r = llclose(0, connect_par);
        if (r < 0) {
            return;
        }
    }
    else if (roles == LlRx) {
        unsigned char buffer[256];
        int result = llread(buffer);
        if (result < 0) {
            return;
        }
        printf("Received data: ");
        for (int i = 0; i < result; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");

        int r = llclose(0, connect_par);
        if (r < 0) {
            return;
        }
    }

    

    // TODO
}
