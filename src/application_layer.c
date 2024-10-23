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

unsigned char* buildControlPacket(const char *filename, long fileSize, unsigned char controlField,int *controlPacketSize) {
    int L1 = 0;
    int L2 = strlen(filename);
    int idx = 0;
    long fileSize_aux = fileSize;
    while (fileSize_aux > 0) {
        fileSize_aux = fileSize_aux >> 8;
        L1++;
    }

    *controlPacketSize = L1 + L2 + 5;
    unsigned char *packet = (unsigned char*)malloc(*controlPacketSize);
    if (packet == NULL)
        return NULL;
    
    packet[idx] = controlField;
    idx++;
    packet[idx] = 0;
    idx++;
    packet[idx] = L1;

    for (int i = L1 - 1; i >= 0; i--) { // big endian
        packet[idx] = (fileSize >> (i * 8)) & 0xFF;
        idx++;
    }

    packet[idx] = 1;
    idx++;
    packet[idx] = L2;
    idx++;

    for (int i = 0; i < L2; i++) {
        packet[idx] = filename[i];
        idx++;
    }
    
    return packet;    
}

long getFileSizeFromPacket(unsigned char* packet) {
    int byteCount = packet[2];
    unsigned char buffer[byteCount];
    long fileSize = 0;
    for (int i = 0; i < byteCount; i++) {
        buffer[i] = packet[3 + i];
    }
    for (int i = 0; i < byteCount; i++) {
        fileSize |= buffer[i] << (8 * (byteCount - i - 1)); // big endian
    }
    return fileSize;
}

unsigned char* getFileNameFromPacket(unsigned char* packet) {
    int byteCount = packet[2 + packet[2] + 1];
    unsigned char *filename = (unsigned char*)malloc(byteCount);
    if (filename == NULL)
        return NULL;
    for (int i = 0; i < byteCount; i++) {
        filename[i] = packet[4 + packet[2] + i];
    }
    return filename;
}

unsigned char* getDataFromPacket(unsigned char* packet, int dataSize, unsigned char* buffer) {
    for (int i = 0; i < dataSize; i++) {
        buffer[i] = packet[4 + i];
    }
    return buffer;
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
        //const unsigned char buffer[] = {0x01, 0x02, 0x03, 0x04, 0x7E, 0x05, 0x01, 0x06, 0x05};
        //int bufferSize = sizeof(buffer) / sizeof(buffer[0]);

        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            return;
        }
        long fileSize = getFileSize(file);
        if (fileSize < 0) {
            return;
        }

        int controlPacketSize = 0;
        unsigned char* control_packet = buildControlPacket(filename, fileSize, 1, &controlPacketSize);
        printf("Control packet size: %d\n", controlPacketSize);

        if (llwrite(control_packet, controlPacketSize) < 0) {
            printf("Error sending control packet\n");
            return;
        }
        printf("Control packet sent\n");
        printf("File size: %ld\n", fileSize);

        int fileBytes = fileSize;
        unsigned char sequenceNum = 0;
        while (fileBytes > 0) {
            unsigned char *data = getFileData(file, MAX_PAYLOAD_SIZE);
            if (data == NULL) {
                return;
            }

            unsigned char *dataPacket = buildDataPacket(sequenceNum, MAX_PAYLOAD_SIZE, data);
            if (dataPacket == NULL) {
                return;
            }

            if (llwrite(dataPacket, MAX_PAYLOAD_SIZE + 4) < 0) {
                printf("Error sending data packet\n");
                return;
            }
            printf("Data packet sent\n");

            fileBytes -= MAX_PAYLOAD_SIZE;
            sequenceNum = (sequenceNum + 1) % 100; // 0-99
        }
        control_packet = buildControlPacket(filename, fileSize, 3, &controlPacketSize);
        if (llwrite(control_packet, controlPacketSize) < 0) {
            printf("Error sending end of file packet\n");
            return;
        }
        printf("End of file packet sent\n");
        
        int r = llclose(0, connect_par);
        if (r < 0) {
            return;
        }
    }
    else if (roles == LlRx) {
        unsigned char *controlPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE);
        if (controlPacket == NULL) {
            return;
        }
        int controlPacketSize = 0;
        while(controlPacketSize == 0) {
            controlPacketSize = llread(controlPacket);
        }
        printf("Control packet received\n");
        //long fileSize = getFileSizeFromPacket(controlPacket);
        unsigned char *filename_rx = getFileNameFromPacket(controlPacket);

        FILE *file_rx = fopen((char*)filename_rx, "wb+");

        while (TRUE) {
            while(controlPacketSize == 0) {
                controlPacketSize = llread(controlPacket);
            }
            printf("Data packet received\n");
            if (controlPacket[0] == 3) {
                break;
            }
            else {
                unsigned char* buffer = (unsigned char*)malloc(controlPacketSize);
                if (buffer == NULL) {
                    return;
                }
                getDataFromPacket(controlPacket, controlPacketSize - 4, buffer);
                //seg fault here below
                fwrite(buffer, sizeof(unsigned char), controlPacketSize - 4, file_rx);
                free(buffer);
            }
        }
        fclose(file_rx);
        free(controlPacket);

    }

    

    // TODO
}
