// Application layer protocol implementation

#include "application_layer.h"
#include <string.h>

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
