/* WSTCPTransport.cpp */

#include "WSTCPTransport.h"
#include "lwip/api.h"

#include <stdlib.h>
#include "pico/stdlib.h"
#include <errno.h>


/***
 * Constructor
 */
WSTCPTransport::WSTCPTransport(){
    xHostDNSFound = xSemaphoreCreateBinary(  );
}

/***
 * Destructor
 */
WSTCPTransport::~WSTCPTransport() {
    vSemaphoreDelete(xHostDNSFound);
}


int32_t WSTCPTransport::transSend(const void * pBuffer, size_t bytesToSend){
    uint32_t dataOut;

    dataOut = write(xSock,(uint8_t *)pBuffer, bytesToSend);
    if (dataOut != bytesToSend){
        printf("Send failed %d\n", dataOut);
    }
    return dataOut;
}


int32_t WSTCPTransport::transRead(void * pBuffer, size_t bytesToRecv){
    int32_t dataIn=0;

    dataIn = read(xSock, (uint8_t *)pBuffer, bytesToRecv);

    if (dataIn < 0){
        if (errno == 0){
            dataIn = 0;
        }
    }

    return dataIn;
}


bool WSTCPTransport::transConnect(const char * host, uint16_t port){
    strncpy(xHostName, host, sizeof(xHostName) - 1);
    xHostName[sizeof(xHostName) - 1] = 0;
    xPort = port;

    // netconn_gethostbyname is the thread-safe, blocking lookup for use
    // from an application task. Calling dns_gethostbyname directly from
    // here skipped lwIP's core lock, and waited the full timeout even when
    // the lookup had already failed (seen as "DNS Timeout ... -6").
    uint32_t now = to_ms_since_boot (get_absolute_time ());
    err_t res = netconn_gethostbyname(xHostName, &xHost);
    if (res != ERR_OK){
        printf("DNS lookup failed for %s: err %d after %u ms\n",
                xHostName, res,
                to_ms_since_boot (get_absolute_time ()) - now
                );
        return false;
    }

    return transConnect();
}


bool WSTCPTransport::transConnect(){
    struct sockaddr_in serv_addr;


    xSock = socket(AF_INET, SOCK_STREAM, 0);
    if (xSock < 0){
        printf("ERROR opening socket\n");
        return false;
    }


    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(xPort);
    memcpy(&serv_addr.sin_addr.s_addr, &xHost, sizeof(xHost));

    int res = connect(xSock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
    if (res < 0){
        char *s = ipaddr_ntoa(&xHost);
        printf("ERROR connecting %d to %s port %d\n",res, s, xPort);
        return false;
    }

    return true;
}


int WSTCPTransport::status(){
    int error = 0;
    socklen_t len = sizeof (error);
    int retval = getsockopt (xSock, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
}

bool WSTCPTransport::transClose(){
    closesocket(xSock);
    return true;
}

void WSTCPTransport::dnsCB(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
    WSTCPTransport *self = (WSTCPTransport *) callback_arg;
    self->dnsFound(name, ipaddr, callback_arg);
}

void WSTCPTransport::dnsFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
    memcpy(&xHost, ipaddr, sizeof(xHost));
    xSemaphoreGiveFromISR(xHostDNSFound, NULL );
}
