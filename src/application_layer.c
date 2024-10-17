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

    llopen(connect_par);

    // TODO
}
