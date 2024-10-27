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

    unsigned char *buffer = (unsigned char*)malloc(fileSize * sizeof(unsigned char));
    if (buffer == NULL)
        return NULL;

    fread(buffer, sizeof(unsigned char), fileSize, file);

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
    idx++;

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
    int idx = 4 + packet[2];
    int byteCount = packet[idx];
    idx++;
    printf("Byte count: %d\n", byteCount);
    unsigned char *filename = (unsigned char*)malloc(byteCount);
    if (filename == NULL)
        return NULL;
    for (int i = 0; i < byteCount; i++) {
        filename[i] = packet[idx];
        idx++;
    }
    return filename;
}

unsigned char* getDataFromPacket(unsigned char* packet, int dataSize) {
    unsigned char *buffer = (unsigned char*)malloc(dataSize);
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


    if (roles == LlTx) {
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

        for (int i = 0; i < controlPacketSize; i++) {
            printf("0x%02X ", control_packet[i]);
        }
        printf("\n");


        int fileBytes = fileSize;
        unsigned char sequenceNum = 0;
        while (fileBytes > 0) {
            int bytesToSend = (fileBytes > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE : fileBytes;

            unsigned char *data = getFileData(file, bytesToSend); 
            if (data == NULL) {
                return;
            }

            unsigned char *dataPacket = buildDataPacket(sequenceNum, bytesToSend, data);
            if (dataPacket == NULL) {
                return;
            }

            if (llwrite(dataPacket, bytesToSend + 4) < 0) {
                printf("Error sending data packet\n");
                return;
            }
            printf("Data packet sent\n");

            fileBytes -= bytesToSend;  
            sequenceNum = (sequenceNum + 1) % 100; 
            free(data);
            free(dataPacket);
        }

        control_packet = buildControlPacket(filename, fileSize, 3, &controlPacketSize);
        if (llwrite(control_packet, controlPacketSize) < 0) {
            printf("Error sending end of file packet\n");
            return;
        }
        printf("End of file packet sent\n");
        
        int r = llclose(1, connect_par);
        if (r < 0) {
            return;
        }
    }
    else if (roles == LlRx) {
        unsigned char *packetRead = (unsigned char*)malloc(MAX_PAYLOAD_SIZE);
        if (packetRead == NULL) {
            return;
        }
        int packetSize = 0;
        while(packetSize == 0) {
            packetSize = llread(packetRead);
        }

        printf("Control packet size: %d\n", packetSize);
        for (int i = 0; i < packetSize; i++) {
            printf("0x%02X ", packetRead[i]);
        }
        printf("\n");

        printf("Control packet received\n");
        unsigned char *filename_rx = getFileNameFromPacket(packetRead);
        printf("Filename: %s\n", filename_rx);

        FILE *file_rx = fopen(filename, "wb+");
        if (file_rx == NULL) {
            printf("Error opening file for writing\n");
            return;
        }

        while (TRUE) {
            packetSize = llread(packetRead);
            if (packetSize > 0) {
                printf("Data packet received\n");
                if (packetRead[0] == 3) {
                    break; 
                } else {
                    printf("Data packet size: %d\n", packetSize);
                    unsigned char *buffer = getDataFromPacket(packetRead, packetSize - 4);
                    if (buffer == NULL) {
                        return;
                    }

                    fwrite(buffer, sizeof(unsigned char), packetSize - 4, file_rx);
                    free(buffer); 
                }
            }
        }
        fclose(file_rx);

        free(packetRead);
        int r = llclose(0, connect_par);
        if (r < 0) {
            return;
        }

    }

}

